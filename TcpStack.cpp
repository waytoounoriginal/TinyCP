#include "TcpStack.h"
#include "utils/Utils.h"

TcpStack::TcpStack(TunDevice& tun_device)
    : listener_table_(socket_table_),
      connection_table_(socket_table_),
      tun_device_(tun_device) {
    daemon_thread_ = std::thread(&TcpStack::lifecycle_, this);
}

TcpStack::~TcpStack() {
    stop_();
    if (daemon_thread_.joinable()) {
        daemon_thread_.detach();
    }
}

void TcpStack::stop_() {
    is_running_ = false;
    has_dirty_data_.notify_one();
}

TcbNonOwningPtr TcpStack::find_connections_tcb_(IPv4Address src_address, IPv4Address dst_address) noexcept {
    return connection_table_.find(src_address, dst_address);
}

TcpPacket TcpStack::create_tcp_header_packet_(const TcbNonOwningPtr& tcb, std::span<const uint8_t> data) noexcept {
    TcpHeader header{};
    header.set_source_port(tcb->src_address.port);
    header.set_dest_port(tcb->dst_address.port);
    header.set_data_offset(5);
    header.set_syn(1);

    TcpPacket packet{};
    packet.header = header;
    packet.payload = data;
    return packet;
}

void TcpStack::process_block_(const TcbNonOwningPtr& tcb) noexcept {
    if (!tcb) return;
    if (tcb->current_state == TcpState::SYN_SENT) {
        send_control_packet_(tcb, /*syn=*/1, /*ack=*/0, /*rst=*/0, tcb->SND.ISS, 0);
        return;
    }

    uint8_t tcp_packet_buffer[MAX_IPV4_PACKET_SIZE];
    auto read_size = tcb->send_buffer.read({tcp_packet_buffer, MAX_IPV4_PACKET_SIZE});
    const auto packet = create_tcp_header_packet_(
        tcb,
        {tcp_packet_buffer, read_size}
    );

    // write packet
    write_packet_(tcb->src_address, tcb->dst_address, packet);
}

void TcpStack::process_dirty_blocks_() {
    while (!dirty_blocks_.empty()) {
        auto curr_block = dirty_blocks_.front();
        dirty_blocks_.pop();

        // Read data from the tcb block & write to tun
        process_block_(curr_block);
    }
}

void TcpStack::lifecycle_() {
    while (is_running_) {
        {
            std::unique_lock lock(mutex);
            has_dirty_data_.wait(lock, [this] {
                return !dirty_blocks_.empty() || !is_running_;
            });

            if (!is_running_) break;

            // process dirty (aka writing writes to tun)
            process_dirty_blocks_();
        }

        // Read and route to the right socket
        size_t last_size_read;
        do {
            last_size_read = read_incoming_packets_();
        } while (last_size_read > 0); // This reads all the packets in the TUN queue
    }
}

void TcpStack::add_dirty_tcb(TcbNonOwningPtr tcb) {
    if (!tcb) return;
    std::lock_guard<std::mutex> lock(mutex);

    dirty_blocks_.push(tcb);
    has_dirty_data_.notify_one();
}

TcbNonOwningPtr TcpStack::bind_socket(IPv4Address addr) {
    return listener_table_.bind_new(addr);
}

TcbNonOwningPtr TcpStack::register_connection(IPv4Address local_addr, IPv4Address remote_addr, TcbNonOwningPtr tcb) {
    if (local_addr.address == 0) {
        local_addr.address = local_address().address;
    }
    if (local_addr.port == 0) {
        local_addr.port = allocate_ephemeral_port();
    }

    return connection_table_.register_connection(local_addr, remote_addr, tcb);
}

IPv4Address TcpStack::local_address() const noexcept {
    return tun_device_.get_usable_ip_address();
}

uint16_t TcpStack::allocate_ephemeral_port() noexcept {
    static uint16_t next_port = 49152;

    for (size_t i = 0; i < 16384; ++i) {
        uint16_t candidate = next_port++;
        if (next_port > 65535) next_port = 49152;

        if (!listener_table_.contains(candidate)) {
            return candidate;
        }
    }
    return next_port++;
}

size_t TcpStack::send_control_packet_(const TcbNonOwningPtr &tcb, uint8_t syn, uint8_t ack, uint8_t rst,
                                      uint32_t seq_num, uint32_t ack_num, uint16_t window_size) noexcept {
    if (!tcb) return 0;
    TcpHeader header{};
    header.set_source_port(tcb->src_address.port);
    header.set_dest_port(tcb->dst_address.port);
    header.set_data_offset(5);
    header.set_syn(syn);
    header.set_ack(ack);
    header.set_rst(rst);
    header.set_seq_num(seq_num);
    header.set_ack_num(ack_num);
    header.set_window(window_size);

    TcpPacket packet{};
    packet.header = header;

    return write_packet_(tcb->src_address, tcb->dst_address, packet);
}

size_t TcpStack::send_raw_rst_(const IPv4Address& src, const IPv4Address& dst, uint32_t seq_num, uint32_t ack_num, uint8_t ack_flag) noexcept {
    TcpHeader header{};
    header.set_source_port(src.port);
    header.set_dest_port(dst.port);
    header.set_data_offset(5);
    header.set_rst(1);
    header.set_ack(ack_flag);
    header.set_seq_num(seq_num);
    header.set_ack_num(ack_num);

    TcpPacket packet{ header, {} };
    return write_packet_(src, dst, packet);
}

size_t TcpStack::handle_closed_reset_response_(const IPv4Address& src, const IPv4Address& dst, const TcpPacketView& packet) noexcept {
    if (packet.rst()) {
        return 0; // RFC 793: Do not send RST in response to another RST
    }

    if (packet.ack()) {
        // RFC 793: If segment has ACK, reset takes sequence number from ACK field
        return send_raw_rst_(dst, src, packet.ack_num_ntoh(), 0, /*ack=*/0);
    } else {
        // RFC 793: If segment has no ACK, reset SEQ = 0, ACK = SEQ + SEG.LEN
        uint32_t ack_val = packet.seq_num_ntoh() + packet.payload().size() + (packet.syn() ? 1 : 0);
        return send_raw_rst_(dst, src, 0, ack_val, /*ack=*/1);
    }
}

size_t TcpStack::handle_passive_open_syn_(const IPv4Address& src, const IPv4Address& dst, const TcpPacketView& packet) noexcept {
    auto listener_tcb = listener_table_.find(dst.port);
    if (listener_tcb && packet.syn() && !packet.ack()) {

        INFO << "[TcpStack] [1/3] Passive Open: Received SYN from " << src.port
             << " -> Local Port " << dst.port
             << " (SEQ=" << packet.seq_num_ntoh() << ")";

        // Allocate child TCB for incoming connection via SocketTable
        auto child_tcb = socket_table_.create_socket(TcpState::SYN_RECEIVED, dst, src);

        child_tcb->RCV.IRS = packet.seq_num_ntoh();
        child_tcb->RCV.NXT = child_tcb->RCV.IRS + 1;

        child_tcb->SND.ISS = generate_random_uint32();
        child_tcb->SND.UNA = child_tcb->SND.ISS;
        child_tcb->SND.NXT = child_tcb->SND.ISS + 1;

        // Register connection 4-tuple (src = remote, dst = local)
        connection_table_.insert(src, dst, child_tcb);

        INFO << "[TcpStack] [2/3] Responding with SYN-ACK to " << src.port
             << " (ISS=" << child_tcb->SND.ISS << ", ACK=" << child_tcb->RCV.NXT << ")";

        // Send SYN-ACK
        return send_control_packet_(child_tcb, /*syn=*/1, /*ack=*/1, /*rst=*/0, child_tcb->SND.ISS, child_tcb->RCV.NXT);
    }
    return 0;
}

size_t TcpStack::handle_syn_sent_state_(const TcbNonOwningPtr& tcb, const IPv4Address& src, const TcpPacketView& packet) noexcept {
    if (!tcb) return 0;
    // 1. Process RST if set
    if (packet.rst()) {
        if (packet.ack() && packet.ack_num_ntoh() == tcb->SND.NXT) {
            INFO << "[TcpStack] Connection reset by peer in SYN_SENT";
            tcb->set_state(TcpState::CLOSED);
        }
        return 0;
    }

    // 2. Check unacceptable ACK
    if (packet.ack() && packet.ack_num_ntoh() != tcb->SND.NXT) {
        WARN << "[TcpStack] SYN_SENT received unacceptable ACK: " << packet.ack_num_ntoh()
             << " (expected " << tcb->SND.NXT << "). Transmitting RST.";
        return send_control_packet_(tcb, /*syn=*/0, /*ack=*/0, /*rst=*/1, packet.ack_num_ntoh(), 0);
    }

    if (packet.syn() && packet.ack()) {
        // Normal 3-way handshake completion: SYN-ACK received in SYN_SENT
        INFO << "[TcpStack] [2/3] Received SYN-ACK from " << src.port
             << ". Transitioning state SYN_SENT -> ESTABLISHED.";

        tcb->RCV.IRS = packet.seq_num_ntoh();
        tcb->RCV.NXT = tcb->RCV.IRS + 1;
        tcb->SND.UNA = packet.ack_num_ntoh();

        tcb->set_state(TcpState::ESTABLISHED);

        INFO << "[TcpStack] [3/3] Sending final ACK to " << src.port;

        // Send the final ACK
        return send_control_packet_(tcb, /*syn=*/0, /*ack=*/1, /*rst=*/0, tcb->SND.NXT, tcb->RCV.NXT);
    } else if (packet.syn()) {
        // Simultaneous Open: Received bare SYN in SYN_SENT
        INFO << "[TcpStack] Simultaneous Open: Received SYN from " << src.port
             << ". Transitioning state SYN_SENT -> SYN_RECEIVED.";

        tcb->RCV.IRS = packet.seq_num_ntoh();
        tcb->RCV.NXT = tcb->RCV.IRS + 1;

        tcb->set_state(TcpState::SYN_RECEIVED);

        // Transmit SYN-ACK segment: <SEQ=SND.ISS><ACK=RCV.NXT><CTL=SYN,ACK>
        return send_control_packet_(tcb, /*syn=*/1, /*ack=*/1, /*rst=*/0, tcb->SND.ISS, tcb->RCV.NXT);
    } else {
        WARN << "[TcpStack] Unknown packet in SYN_SENT state";
        return 0;
    }
}

size_t TcpStack::handle_syn_received_state_(const TcbNonOwningPtr& tcb, const IPv4Address& src, const TcpPacketView& packet) noexcept {
    if (!tcb) return 0;
    // 1. Handle incoming RST segment
    if (packet.rst()) {
        INFO << "[TcpStack] Received RST in SYN_RECEIVED. Aborting connection.";
        tcb->set_state(TcpState::CLOSED);
        connection_table_.erase(src, tcb->src_address);
        socket_table_.destroy_socket(tcb);
        return 0;
    }

    if (!packet.ack()) return 0;

    // Check ACK validity
    if (packet.ack_num_ntoh() != tcb->SND.NXT) {
        WARN << "[TcpStack] SYN_RECEIVED received invalid ACK number: " << packet.ack_num_ntoh()
             << " (expected " << tcb->SND.NXT << ")";
        return 0;
    }

    INFO << "[TcpStack] Received ACK in SYN_RECEIVED from " << src.port
         << ". Handshake complete! Transitioning SYN_RECEIVED -> ESTABLISHED.";

    tcb->SND.UNA = packet.ack_num_ntoh();
    tcb->set_state(TcpState::ESTABLISHED);

    // If this connection originated from a passive listening socket, push to parent's accept queue
    auto listener_tcb = listener_table_.find(tcb->src_address.port);
    if (listener_tcb) {
        std::lock_guard<std::mutex> lk(listener_tcb->state_mutex);
        listener_tcb->accept_queue.push(tcb);
        listener_tcb->state_cv.notify_all();
    }

    // In simultaneous open (or passive open), send an ACK if needed
    return send_control_packet_(tcb, /*syn=*/0, /*ack=*/1, /*rst=*/0, tcb->SND.NXT, tcb->RCV.NXT);
}

size_t TcpStack::handle_established_state_(const TcbNonOwningPtr& tcb, const TcpPacketView& packet) noexcept {
    if (!tcb) return 0;
    if (packet.payload().size() > 0) {
        return tcb->recv_buffer.write(packet.payload());
    }
    return 0;
}

size_t TcpStack::process_incoming_packet_(std::span<const uint8_t> buffer) noexcept {
    IPv4PacketView tmp(buffer);
    if (!tmp.valid()) return 0;

    TcpPacketView packet{tmp.payload()};
    if (!packet.valid()) return 0;

    IPv4Address src{ tmp.source_address_ntoh(), packet.source_port_ntoh() };
    IPv4Address dst{ tmp.destination_address_ntoh(), packet.dest_port_ntoh() };

    auto tcb = find_connections_tcb_(src, dst);

    if (!tcb) {
        // If passive open on listening socket
        if (listener_table_.contains(dst.port) && packet.syn() && !packet.ack()) {
            return handle_passive_open_syn_(src, dst, packet);
        }
        // Non-existent connection reset handling
        return handle_closed_reset_response_(src, dst, packet);
    }

    // Dispatch to modular state machine handlers
    switch (tcb->current_state) {
        case CLOSED:
            return handle_closed_reset_response_(src, dst, packet);

        case LISTEN:
            return handle_passive_open_syn_(src, dst, packet);

        case SYN_SENT:
            return handle_syn_sent_state_(tcb, src, packet);

        case SYN_RECEIVED:
            return handle_syn_received_state_(tcb, src, packet);

        case ESTABLISHED:
            return handle_established_state_(tcb, packet);

        default:
            break;
    }

    return 0;
}

size_t TcpStack::write_packet_(IPv4Address src_address, IPv4Address dst_address, const TcpPacket& packet) noexcept {
    uint8_t ip_packet_buffer[MAX_IPV4_PACKET_SIZE];
    uint8_t tcp_packet_buffer[MAX_IPV4_PACKET_SIZE];

    // Serialize TCP segment with full RFC 793 checksum using native IPv4Address
    auto payload_size = packet.write({tcp_packet_buffer, MAX_IPV4_PACKET_SIZE}, src_address, dst_address);

    // Write the IP packet
    IPv4Header ip_header{};
    ip_header.set_version(4);
    ip_header.set_ihl(5);
    ip_header.set_ttl(255);
    ip_header.set_source_address(src_address.address);
    ip_header.set_destination_address(dst_address.address);
    ip_header.set_protocol(IPPROTO_TCP);
    ip_header.set_fragment_offset(0);

    IPv4Packet packet_to_send{};
    packet_to_send.header = ip_header;
    packet_to_send.payload = {tcp_packet_buffer, payload_size};

    // packet_to_send.write automatically sets total_length and computes header checksum
    auto ip_payload_size = packet_to_send.write({ip_packet_buffer, MAX_IPV4_PACKET_SIZE});

#ifdef TCP_STACK_TESTING
    if (outbound_interceptor_) {
        outbound_interceptor_(src_address, dst_address, packet);
    }
#endif
    
    // Write to TUN device
    return tun_device_.tun_write(reinterpret_cast<const char *>(ip_packet_buffer), ip_payload_size);
}

size_t TcpStack::read_incoming_packets_() noexcept {
    uint8_t buffer[MAX_IPV4_PACKET_SIZE];

    // Read current packet and fwd it to the socket
    auto read_size {tun_device_.tun_read(reinterpret_cast<char *>(buffer), MAX_IPV4_PACKET_SIZE)};
    if (read_size <= 0) {
        return 0;
    }

    return process_incoming_packet_({buffer, static_cast<size_t>(read_size)});
}
