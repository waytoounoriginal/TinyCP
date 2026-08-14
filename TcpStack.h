//
// Created by waytoounoriginal on 8/12/2026.
//

#ifndef TCP_FROM_SCRATCH_TCPDEMULTIPLEXER_H
#define TCP_FROM_SCRATCH_TCPDEMULTIPLEXER_H

#include <cstring>
#include <memory>
#include <unordered_map>
#include "utils/Platform.h"
#include <mutex>
#include <queue>
#include <thread>

#include "IPv4.h"
#include "IPv4Packet.h"
#include "IPv4PacketView.h"
#include "TcpPacket.h"
#include "TcpPacketView.h"
#include "TransmissionControlBlock.h"
#include "TunDevice.h"
#include "utils/Logger.h"


using TcbSharedResource = std::shared_ptr<TransmissionControlBlock>;
using ObserverUpdateCallback = void (*)(TcbSharedResource);

/** The router for the userspace socket */
class TcpStack {
private:

    struct ConnectionKey {
        IPv4Address src_address;
        IPv4Address dst_address;

        bool operator==(const ConnectionKey& other) const {
            return src_address == other.src_address && dst_address == other.dst_address;
        }
    };

    struct ConnectionKeyHash {
        std::size_t operator()(const ConnectionKey& key) const noexcept {
            std::size_t h{0};

            h ^= std::hash<uint32_t>{}(key.src_address.address)
                + 0x71be114 + (h << 6) + (h >> 2);

            h ^= std::hash<uint16_t>{}(key.src_address.port)
                + 0x71be114 + (h << 6) + (h >> 2);

            h ^= std::hash<uint32_t>{}(key.dst_address.address)
                + 0x71be114 + (h << 6) + (h >> 2);

            h ^= std::hash<uint16_t>{}(key.dst_address.port)
                + 0x71be114 + (h << 6) + (h >> 2);

            return h;
        }
    };

    std::mutex mutex;

    /** Used for waking the thread up when a socket writes */
    std::condition_variable has_dirty_data_ {};

    /** tcp sockets table */
    std::unordered_map<uint16_t, TcbSharedResource> tcp_listeners_;

    /** tcp connections table */
    std::unordered_map<
        ConnectionKey,
        TcbSharedResource,
        ConnectionKeyHash
    > tcp_connections_;

    /** Queue for processing resources */
    std::queue<TcbSharedResource> dirty_blocks_ {};


    /** Thread-safe, Find the socket in a certain conenction */
    TcbSharedResource find_tcb(IPv4Address src_address, IPv4Address dst_address) noexcept {
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

    TcpPacket create_tcp_header_packet_(const TcbSharedResource& tcb, std::span<const uint8_t> data) noexcept {
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

    void process_block_(const TcbSharedResource& tcb) noexcept {
        uint8_t tcp_packet_buffer[MAX_IPV4_PACKET_SIZE];

        // This is for sure non-blocked
        auto read_size = tcb->send_buffer.read({tcp_packet_buffer, MAX_IPV4_PACKET_SIZE});
        const auto packet = create_tcp_header_packet_(
            tcb,
            {tcp_packet_buffer, read_size}
        );

        // write packet
        write(tcb->src_address, tcb->dst_address, packet);
    }

    void process_dirty_blocks_() {
        while (!dirty_blocks_.empty()) {

            auto curr_block = std::move(dirty_blocks_.front());
            dirty_blocks_.pop();

            // Read the data from the tcb block & write to tun
            process_block_(curr_block);
        }
    }

    /** The daemon thread's lifetime */
    void lifecycle_() {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mutex);
                has_dirty_data_.wait(lock, [this] {
                    return !dirty_blocks_.empty();
                });

                // process dirty (aka writing writes to tun)
                process_dirty_blocks_();
            }

            // Read and route to the right socket
            read();
        }
    }

    TcpStack() {
        // Create a detached daemon background thread
        std::thread(&TcpStack::lifecycle_, this).detach();
    }

public:
    static TcpStack& instance() {
        static TcpStack instance {};
        return instance;
    }

    /** Callback for adding dirty blocks */
    void add_dirty_tcb(TcbSharedResource tcb) {
        std::lock_guard<std::mutex> lock(instance().mutex);

        dirty_blocks_.push(tcb);
        has_dirty_data_.notify_one();
    }

    /** Thread-safe logging into the sockets table */
    inline auto bind_socket(IPv4Address addr) {
        std::lock_guard<std::mutex> lock(mutex);

        // Make a new resource to return
        return tcp_listeners_[addr.port] = std::make_shared<TransmissionControlBlock>(
            TcpState::CLOSED, addr
        );
    }

    /** Registers an established 4-tuple connection in the connections table */
    inline void register_connection(IPv4Address local_addr, IPv4Address remote_addr, TcbSharedResource tcb) {
        std::lock_guard<std::mutex> lock(mutex);
        tcb->src_address = local_addr;
        tcb->dst_address = remote_addr;
        // Key is (remote_addr, local_addr) because incoming packets come FROM remote TO local
        tcp_connections_[ConnectionKey{remote_addr, local_addr}] = tcb;
    }

    /** Feeds a raw IPv4+TCP packet directly into the stack (useful for in-memory testing) */
    inline size_t process_incoming_packet(std::span<const uint8_t> buffer) noexcept {
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

    /** Thread-safe writing to TUN device of a whole IPv4 Packet */
    inline size_t write(IPv4Address src_address, IPv4Address dst_address, const TcpPacket& packet) noexcept {
        uint8_t ip_packet_buffer[MAX_IPV4_PACKET_SIZE];
        uint8_t tcp_packet_buffer[MAX_IPV4_PACKET_SIZE];

        // Serialize TCP segment with full RFC 793 checksum using IP addresses
        auto payload_size = WriteTcpPacket({tcp_packet_buffer, MAX_IPV4_PACKET_SIZE},
                                           htonl(src_address.address),
                                           htonl(dst_address.address),
                                           packet);

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

        // WriteIPv4Packet automatically sets total_length and computes header checksum
        auto ip_payload_size = WriteIPv4Packet({ip_packet_buffer, MAX_IPV4_PACKET_SIZE}, packet_to_send);

        // Write to TUN device
        return TunDevice::instance().tun_write(reinterpret_cast<const char *>(ip_packet_buffer), ip_payload_size);
    }

    /** Read the current TUN Device packet and route to the right socket */
    inline size_t read() noexcept {
        uint8_t buffer[MAX_IPV4_PACKET_SIZE];

        // Read the current packet and fwd it to the socket
        auto read_size {TunDevice::instance().tun_read(reinterpret_cast<char *>(buffer), MAX_IPV4_PACKET_SIZE)};
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

    TcpStack(TcpStack&) = delete;
    TcpStack& operator=(TcpStack&) = delete;
    TcpStack(TcpStack&&) = delete;
    TcpStack& operator=(TcpStack&&) = delete;
};


#endif //TCP_FROM_SCRATCH_TCPDEMULTIPLEXER_H
