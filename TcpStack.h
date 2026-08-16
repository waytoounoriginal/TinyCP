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

    TunDevice& tun_device_;
    std::thread daemon_thread_;
    std::atomic_bool is_running_ {true};

    /** Thread-safe, Find the socket in a certain connection */
    TcbSharedResource find_connections_tcb_(IPv4Address src_address, IPv4Address dst_address) noexcept;

    TcpPacket create_tcp_header_packet_(const TcbSharedResource& tcb, std::span<const uint8_t> data) noexcept;

    void process_block_(const TcbSharedResource& tcb) noexcept;

    void process_dirty_blocks_();

    /** The daemon thread's lifetime */
    void lifecycle_();

    /** Sends a TCP control packet (SYN, SYN-ACK, ACK, RST) without payload */
    size_t send_control_packet_(const TcbSharedResource &tcb, uint8_t syn, uint8_t ack, uint8_t rst, uint32_t seq_num,
                                uint32_t ack_num, uint16_t window_size = 65535) noexcept;

    /** Process a parsed incoming IP+TCP packet against the TCP state machine */
    size_t process_incoming_packet_(std::span<const uint8_t> buffer) noexcept;

    /** Writing to TUN device of a whole IPv4 Packet */
    size_t write_packet_(IPv4Address src_address, IPv4Address dst_address, const TcpPacket& packet) noexcept;

    /** Read the current TUN Device packet and route to the right socket */
    size_t read_incoming_packets_() noexcept;

    /** Stop the tcp stack */
    void stop_();

public:
    explicit TcpStack(TunDevice& tun_device);
    ~TcpStack();

    /** Callback for adding dirty blocks */
    void add_dirty_tcb(TcbSharedResource tcb);

    /** Thread-safe logging into the sockets table */
    TcbSharedResource bind_socket(IPv4Address addr);

    /** Thread-safe, registers an established 4-tuple connection in the connections table */
    TcbSharedResource register_connection(IPv4Address local_addr, IPv4Address remote_addr, TcbSharedResource tcb);

    /** Returns local IPv4 address of the stack's network interface */
    IPv4Address local_address() const noexcept;

    /** Thread-safe allocation of an unused ephemeral port (49152..65535) */
    uint16_t allocate_ephemeral_port() noexcept;
};


#endif //TCP_FROM_SCRATCH_TCPDEMULTIPLEXER_H
