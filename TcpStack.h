//
// Created by waytoounoriginal on 8/12/2026.
//

#ifndef TCP_FROM_SCRATCH_TCPDEMULTIPLEXER_H
#define TCP_FROM_SCRATCH_TCPDEMULTIPLEXER_H

#include <cstring>
#include <functional>
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


#include "SocketTable.h"
#include "ListenerTable.h"
#include "ConnectionTable.h"

using TcbResource = std::unique_ptr<TransmissionControlBlock>;
using TcbNonOwningPtr = TransmissionControlBlock*;
using ObserverUpdateCallback = void (*)(TcbNonOwningPtr);

/** The router for the userspace socket */
class TcpStack {
private:

    std::mutex mutex;

    /** Master owner of all TCB memory */
    SocketTable socket_table_;

    /** Port routing table for listening sockets */
    ListenerTable listener_table_;

    /** 4-tuple routing table for active connections */
    ConnectionTable connection_table_;

    /** Queue for processing socket IDs with pending outbound data */
    std::queue<uint64_t> dirty_socket_ids_ {};

    /** Fast-path in-memory loopback queue for local traffic */
    std::mutex loopback_mutex_;
    std::queue<std::vector<uint8_t>> loopback_packet_queue_ {};

    TunDevice& tun_device_;
    std::thread daemon_thread_;
    std::atomic_bool is_running_ {true};

    /** Lookup socket in active connections table */
    uint64_t find_connections_tcb_(IPv4Address src_address, IPv4Address dst_address);

    TcpPacket create_writer_tcp_packet_(const TransmissionControlBlock* tcb, std::span<const uint8_t> data, uint32_t seq_num) noexcept;

    void process_block_(uint64_t socket_id);

    void process_dirty_blocks_();

    /** Checks and retransmits unacknowledged segments on RTO expiry */
    void check_retransmissions_();

    /** The daemon thread's lifetime */
    void lifecycle_();

    /** Sends a TCP control packet (SYN, SYN-ACK, ACK, RST, FIN) using an existing TCB */
    size_t send_control_packet_(TransmissionControlBlock* tcb, uint8_t syn, uint8_t ack, uint8_t rst, uint8_t fin, uint32_t seq_num,
                                uint32_t ack_num, uint16_t window_size = 65535) noexcept;

    /** 
     * Transmits a raw RST segment when no valid TCB exists (e.g. for CLOSED sockets or unknown 4-tuples).
     */
    size_t send_raw_rst_(const IPv4Address& src, const IPv4Address& dst, uint32_t seq_num, uint32_t ack_num, uint8_t ack_flag) noexcept;

    /** 
     * RFC 793 Reset Generation Rule (CLOSED / Non-Existent Connection).
     */
    size_t handle_closed_reset_response_(const IPv4Address& src, const IPv4Address& dst, const TcpPacketView& packet) noexcept;

    /** 
     * Handles passive open SYN arrival on a listening socket (LISTEN state).
     */
    size_t handle_passive_open_syn_(const IPv4Address& src, const IPv4Address& dst, const TcpPacketView& packet);

    /** Handles SYN_SENT state */
    size_t handle_syn_sent_state_(uint64_t socket_id, const IPv4Address& src, const TcpPacketView& packet);

    /** Handles SYN_RECEIVED state */
    size_t handle_syn_received_state_(uint64_t socket_id, const IPv4Address& src, const TcpPacketView& packet);

    /** Handles ESTABLISHED state */
    size_t handle_established_state_(uint64_t socket_id, const TcpPacketView& packet);

    /** Handles FIN_WAIT_1 state */
    size_t handle_fin_wait_1_state_(uint64_t socket_id, const TcpPacketView& packet);

    /** Handles FIN_WAIT_2 state */
    size_t handle_fin_wait_2_state_(uint64_t socket_id, const TcpPacketView& packet);

    /** Handles CLOSING state */
    size_t handle_closing_state_(uint64_t socket_id, const TcpPacketView& packet);

    /** Handles LAST_ACK state */
    size_t handle_last_ack_state_(uint64_t socket_id, const TcpPacketView& packet);

    /** Process a parsed incoming IP+TCP packet against the TCP state machine */
    size_t process_incoming_packet_(std::span<const uint8_t> buffer);

    /** Writing to TUN device of a whole IPv4 Packet */
    size_t write_packet_(IPv4Address src_address, IPv4Address dst_address, const TcpPacket& packet) noexcept;

    /** Read the current TUN Device packet and route to the right socket */
    size_t read_incoming_packets_();

    /** Stop the tcp stack */
    void stop_();

public:
    explicit TcpStack(TunDevice& tun_device);
    ~TcpStack();

    /** Looks up a TransmissionControlBlock by its unique socket ID, validating existence */
    TransmissionControlBlock* get_tcb(uint64_t socket_id) const {
        return socket_table_.find_socket(socket_id);
    }

    /** Callback for adding dirty socket IDs */
    void add_dirty_tcb(uint64_t socket_id);

    /** Thread-safe logging into the sockets table, returning allocated socket ID */
    uint64_t bind_socket(IPv4Address addr);

    /** Thread-safe, registers an established 4-tuple connection in the connections table, returning socket ID */
    uint64_t register_connection(IPv4Address local_addr, IPv4Address remote_addr, uint64_t socket_id);

    /** Closes a connection between 2 sockets */
    size_t close_connection(uint64_t socket_id);

    /** Destroys a connection by 4-tuple key (remote_addr, local_addr) and frees TCB resource */
    bool destroy_connection(IPv4Address remote_addr, IPv4Address local_addr);

    /** Returns local IPv4 address of the stack's network interface */
    IPv4Address local_address() const noexcept;

    /** Thread-safe allocation of an unused ephemeral port (49152..65535) */
    uint16_t allocate_ephemeral_port();

#ifdef TCP_STACK_TESTING
public:
    using OutboundPacketCallback = std::function<void(IPv4Address src, IPv4Address dst, const TcpPacket& packet)>;
    using PacketDropPredicate = std::function<bool(IPv4Address src, IPv4Address dst, const TcpPacket& packet)>;

    /** In-memory packet injection helper for testing without TUN device */
    size_t inject_packet(std::span<const uint8_t> raw_bytes) {
        return process_incoming_packet_(raw_bytes);
    }

    /** Outbound packet interception helper for testing */
    void set_outbound_interceptor(OutboundPacketCallback callback) noexcept {
        outbound_interceptor_ = std::move(callback);
    }

    /** Outbound packet drop predicate for simulating transient loss */
    void set_packet_drop_predicate(PacketDropPredicate predicate) noexcept {
        packet_drop_predicate_ = std::move(predicate);
    }

    /** Simulates sudden node crash by wiping connection TCB without FIN/RST */
    void simulate_node_crash(IPv4Address remote_addr, IPv4Address local_addr) {
        connection_table_.erase(remote_addr, local_addr);
    }

private:
    OutboundPacketCallback outbound_interceptor_;
    PacketDropPredicate packet_drop_predicate_;
#endif
};


#endif //TCP_FROM_SCRATCH_TCPDEMULTIPLEXER_H
