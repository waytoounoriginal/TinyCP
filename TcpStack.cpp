#include "TcpStack.h"

TcpStack::TcpStack(TunDevice& tun_device)
    : tun_device_(tun_device) {
    daemon_thread_ = std::thread(&TcpStack::lifecycle_, this);
}

TcpStack::~TcpStack() {
    stop();
    if (daemon_thread_.joinable()) {
        daemon_thread_.detach();
    }
}

void TcpStack::stop() {
    is_running_ = false;
    has_dirty_data_.notify_one();
}

TcbSharedResource TcpStack::find_tcb(IPv4Address src_address, IPv4Address dst_address) noexcept {
    std::lock_guard<std::mutex> lock(mutex);

    const ConnectionKey key{src_address, dst_address};

    auto it = tcp_connections_.find(key);
    if (it != tcp_connections_.end()) {
        return it->second;
    }

    // Fallback to listening
    auto listener_it = tcp_listeners_.find(dst_address.port);
    if (listener_it != tcp_listeners_.end()) {
        return listener_it->second;
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
    uint8_t tcp_packet_buffer[MAX_IPV4_PACKET_SIZE];

    // This is for sure non-blocked
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
            std::unique_lock<std::mutex> lock(mutex);
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

void TcpStack::register_connection(IPv4Address local_addr, IPv4Address remote_addr, TcbSharedResource tcb) {
    std::lock_guard<std::mutex> lock(mutex);
    tcb->src_address = local_addr;
    tcb->dst_address = remote_addr;
    // Key is (remote_addr, local_addr) because incoming packets come FROM remote TO local
    tcp_connections_[ConnectionKey{remote_addr, local_addr}] = tcb;
}

size_t TcpStack::process_incoming_packet_(std::span<const uint8_t> buffer) noexcept {
    IPv4PacketView tmp(buffer);
    if (!tmp.valid()) return 0;

    TcpPacketView packet{tmp.payload()};
    if (!packet.valid()) return 0;

    IPv4Address src{ tmp.source_address_ntoh(), packet.source_port_ntoh() };
    IPv4Address dst{ tmp.destination_address_ntoh(), packet.dest_port_ntoh() };

    auto tcb = find_tcb(src, dst);
    if (!tcb) return 0;

    return tcb->recv_buffer.write(packet.payload());
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

    IPv4PacketView tmp({buffer, static_cast<size_t>(read_size)});
    if (!tmp.valid()) {
        Logger::instance().warn() << "Invalid IPv4 packet!!";
        return 0;
    }

    TcpPacketView packet{tmp.payload()};
    if (!packet.valid()) {
        Logger::instance().warn() << "Invalid TCP packet!!";
        return 0;
    }

    IPv4Address src {
        tmp.source_address_ntoh(),
        packet.source_port_ntoh(),
    },
    dst {
        tmp.destination_address_ntoh(),
        packet.dest_port_ntoh(),
    };

    // Route to the right socket
    auto tcb = find_tcb(src, dst);
    if (!tcb) {
        return 0;
    }

    return tcb->recv_buffer.write(packet.payload());
}
