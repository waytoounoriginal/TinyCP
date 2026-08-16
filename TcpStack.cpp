#include "TcpStack.h"
#include "utils/Utils.h"

TcpStack::TcpStack(TunDevice& tun_device)
    : tun_device_(tun_device) {
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

TcbSharedResource TcpStack::find_connections_tcb_(IPv4Address src_address, IPv4Address dst_address) noexcept {
    std::lock_guard<std::mutex> lock(mutex);

    const ConnectionKey key{src_address, dst_address};

    auto it = tcp_connections_.find(key);
    if (it != tcp_connections_.end()) {
        return it->second;
    }

    return nullptr;
}

TcpPacket TcpStack::create_tcp_header_packet_(const TcbSharedResource& tcb, std::span<const uint8_t> data) noexcept {
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

void TcpStack::process_block_(const TcbSharedResource& tcb) noexcept {
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
        auto curr_block = std::move(dirty_blocks_.front());
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

void TcpStack::add_dirty_tcb(TcbSharedResource tcb) {
    std::lock_guard<std::mutex> lock(mutex);

    dirty_blocks_.push(tcb);
    has_dirty_data_.notify_one();
}

TcbSharedResource TcpStack::bind_socket(IPv4Address addr) {
    std::lock_guard<std::mutex> lock(mutex);

    return tcp_listeners_[addr.port] = std::make_shared<TransmissionControlBlock>(
        TcpState::CLOSED, addr
    );
}

TcbSharedResource TcpStack::register_connection(IPv4Address local_addr, IPv4Address remote_addr, TcbSharedResource tcb) {
    if (local_addr.address == 0) {
        local_addr.address = local_address().address;
    }
    if (local_addr.port == 0) {
        local_addr.port = allocate_ephemeral_port();
    }

    std::lock_guard<std::mutex> lock(mutex);

    if (!tcb) {
        tcb = std::make_shared<TransmissionControlBlock>(TcpState::CLOSED, local_addr, remote_addr);
    } else {
        tcb->src_address = local_addr;
        tcb->dst_address = remote_addr;
    }

    // Key is (remote_addr, local_addr) because incoming packets come FROM remote TO local
    return tcp_connections_[ConnectionKey{remote_addr, local_addr}] = tcb;
}

IPv4Address TcpStack::local_address() const noexcept {
    return tun_device_.get_usable_ip_address();
}

uint16_t TcpStack::allocate_ephemeral_port() noexcept {
    std::lock_guard<std::mutex> lock(mutex);
    static uint16_t next_port = 49152;

    for (size_t i = 0; i < 16384; ++i) {
        uint16_t candidate = next_port++;
        if (next_port > 65535) next_port = 49152;

        if (tcp_listeners_.find(candidate) == tcp_listeners_.end()) {
            return candidate;
        }
    }
    return next_port++;
}

size_t TcpStack::send_control_packet_(const TcbSharedResource &tcb, uint8_t syn, uint8_t ack, uint8_t rst,
                                      uint32_t seq_num, uint32_t ack_num, uint16_t window_size) noexcept {
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

size_t TcpStack::process_incoming_packet_(std::span<const uint8_t> buffer) noexcept {
    IPv4PacketView tmp(buffer);
    if (!tmp.valid()) return 0;

    TcpPacketView packet{tmp.payload()};
    if (!packet.valid()) return 0;

    IPv4Address src{ tmp.source_address_ntoh(), packet.source_port_ntoh() };
    IPv4Address dst{ tmp.destination_address_ntoh(), packet.dest_port_ntoh() };

    auto tcb = find_connections_tcb_(src, dst);

    if (!tcb) {
        // Check listening socket table for passive open (SYN)
        std::lock_guard lock(mutex);
        auto listener_it = tcp_listeners_.find(dst.port);
        if (listener_it != tcp_listeners_.end() && packet.syn() && !packet.ack()) {
            auto listener_tcb = listener_it->second;

            Logger::instance().info() << "[TcpStack] [1/3] Passive Open: Received SYN from " << src.port
                                      << " -> Local Port " << dst.port
                                      << " (SEQ=" << packet.seq_num_ntoh() << ")";

            // Allocate child TCB for incoming connection
            auto child_tcb = std::make_shared<TransmissionControlBlock>(TcpState::SYN_RECEIVED, dst, src);
            child_tcb->RCV.IRS = packet.seq_num_ntoh();
            child_tcb->RCV.NXT = child_tcb->RCV.IRS + 1;
            child_tcb->SND.ISS = generate_random_uint32();
            child_tcb->SND.NXT = child_tcb->SND.ISS + 1;

            // Register connection 4-tuple (src = remote, dst = local)
            tcp_connections_[ConnectionKey{src, dst}] = child_tcb;

            Logger::instance().info() << "[TcpStack] [2/3] Responding with SYN-ACK to " << src.port
                                      << " (ISS=" << child_tcb->SND.ISS << ", ACK=" << child_tcb->RCV.NXT << ")";

            // Send SYN-ACK
            return send_control_packet_(child_tcb, /*syn=*/1, /*ack=*/1, /*rst=*/0, child_tcb->SND.ISS, child_tcb->RCV.NXT);
        }
        return 0;
    }

    // Handle the different states
    switch (tcb->current_state) {
        case SYN_SENT:
            // check that the ACK matches
            if (!(packet.syn() && packet.ack())) break;

            if (packet.ack_num_ntoh() != tcb->SND.NXT) {
                Logger::instance().warn() << "[TcpStack] SYN_SENT received invalid ACK number: " << packet.ack_num_ntoh()
                                          << " (expected " << tcb->SND.NXT << ")";
                break;
            }

            Logger::instance().info() << "[TcpStack] [2/3] Client received SYN-ACK from " << src.port
                                      << ". Transitioning state SYN_SENT -> ESTABLISHED.";

            tcb->RCV.IRS = packet.seq_num_ntoh();
            tcb->RCV.NXT = tcb->RCV.IRS + 1;
            tcb->SND.UNA = packet.ack_num_ntoh();
            tcb->set_state(TcpState::ESTABLISHED);

            Logger::instance().info() << "[TcpStack] [3/3] Sending final ACK to " << src.port;

            // Send the final ack
            return send_control_packet_(tcb, /*syn=*/0, /*ack=*/1, /*rst=*/0, tcb->SND.NXT, tcb->RCV.NXT);

        case SYN_RECEIVED:
            // Handle receiving the ack from the sender
            if (packet.syn() || !packet.ack()) break;

            if (packet.ack_num_ntoh() != tcb->SND.NXT) {
                Logger::instance().warn() << "[TcpStack] SYN_RECEIVED received invalid ACK number: " << packet.ack_num_ntoh()
                                          << " (expected " << tcb->SND.NXT << ")";
                break;
            }

            Logger::instance().info() << "[TcpStack] [3/3] Server received final ACK from " << src.port
                                      << ". Handshake complete! Transitioning SYN_RECEIVED -> ESTABLISHED.";

            tcb->set_state(TcpState::ESTABLISHED);

            // Find sender and push to accept queue
            {
                std::lock_guard<std::mutex> lock(mutex);
                auto sender_it = tcp_listeners_.find(tcb->src_address.port);
                if (sender_it != tcp_listeners_.end()) {
                    auto listener_tcb = sender_it->second;
                    {
                        std::lock_guard<std::mutex> lk(listener_tcb->state_mutex);
                        listener_tcb->accept_queue.push(tcb);
                    }
                    listener_tcb->state_cv.notify_all();
                }
            }

            break;

        default:
            // todo: proper sending logic
            tcb->recv_buffer.write(packet.payload());
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
