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

    /** Used for waking the thread up when a socket writes */
    std::condition_variable has_dirty_data_ {};

    /** Master owner of all TCB memory */
    SocketTable socket_table_;

    /** Port routing table for listening sockets */
    ListenerTable listener_table_;

    /** 4-tuple routing table for active connections */
    ConnectionTable connection_table_;

    /** Queue for processing resources */
    std::queue<TcbNonOwningPtr> dirty_blocks_ {};

    TunDevice& tun_device_;
    std::thread daemon_thread_;
    std::atomic_bool is_running_ {true};

    /** Thread-safe, Find the socket in a certain connection */
    TcbNonOwningPtr find_connections_tcb_(IPv4Address src_address, IPv4Address dst_address) noexcept;

    TcpPacket create_tcp_header_packet_(const TcbNonOwningPtr& tcb, std::span<const uint8_t> data) noexcept;

    void process_block_(const TcbNonOwningPtr& tcb) noexcept;

    void process_dirty_blocks_();

    /** The daemon thread's lifetime */
    void lifecycle_();

    /** Sends a TCP control packet (SYN, SYN-ACK, ACK, RST) using an existing TCB */
    size_t send_control_packet_(const TcbNonOwningPtr &tcb, uint8_t syn, uint8_t ack, uint8_t rst, uint32_t seq_num,
                                uint32_t ack_num, uint16_t window_size = 65535) noexcept;

    /** 
     * Transmits a raw RST segment when no valid TCB exists (e.g. for CLOSED sockets or unknown 4-tuples).
     * @param src Source IPv4 address + port (local end transmitting the RST)
     * @param dst Destination IPv4 address + port (remote end receiving the RST)
     * @param seq_num Sequence number for the RST segment
     * @param ack_num Acknowledgment number for the RST segment (if ACK flag is set)
     * @param ack_flag 1 if ACK flag is set, 0 otherwise
     */
    size_t send_raw_rst_(const IPv4Address& src, const IPv4Address& dst, uint32_t seq_num, uint32_t ack_num, uint8_t ack_flag) noexcept;

    /** 
     * RFC 793 Reset Generation Rule (CLOSED / Non-Existent Connection):
     * If an incoming segment arrives for a non-existent connection, a RST is sent in response:
     * - If incoming has ACK: RST SEQ = incoming ACK, ACK flag = 0.
     * - If incoming has no ACK: RST SEQ = 0, RST ACK = incoming SEQ + SEG.LEN, ACK flag = 1.
     */
    size_t handle_closed_reset_response_(const IPv4Address& src, const IPv4Address& dst, const TcpPacketView& packet) noexcept;

    /** 
     * Handles passive open SYN arrival on a listening socket (LISTEN state).
     * Allocates a child TCB in SYN_RECEIVED state, registers connection 4-tuple, and transmits SYN-ACK.
     */
    size_t handle_passive_open_syn_(const IPv4Address& src, const IPv4Address& dst, const TcpPacketView& packet) noexcept;

    /** 
     * Handles state transitions and reset processing in SYN_SENT state (Active Open).
     * - Processes incoming RST (resets connection to CLOSED).
     * - Validates ACK field against SND.NXT (transmits RST if ACK is unacceptable).
     * - Transitions to ESTABLISHED upon receiving valid SYN-ACK and transmits final ACK.
     */
    size_t handle_syn_sent_state_(const TcbNonOwningPtr& tcb, const IPv4Address& src, const TcpPacketView& packet) noexcept;

    /** 
     * Handles state transitions and reset processing in SYN_RECEIVED state (Passive Open).
     * - Processes incoming RST (aborts connection and removes child TCB from active table).
     * - Validates final ACK from client and transitions connection state to ESTABLISHED.
     * - Pushes established child TCB onto parent listener's accept_queue.
     */
    size_t handle_syn_received_state_(const TcbNonOwningPtr& tcb, const IPv4Address& src, const TcpPacketView& packet) noexcept;

    /** 
     * Handles payload data delivery for ESTABLISHED connections.
     */
    size_t handle_established_state_(const TcbNonOwningPtr& tcb, const TcpPacketView& packet) noexcept;

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
    void add_dirty_tcb(TcbNonOwningPtr tcb);

    /** Thread-safe logging into the sockets table */
    TcbNonOwningPtr bind_socket(IPv4Address addr);

    /** Thread-safe, registers an established 4-tuple connection in the connections table */
    TcbNonOwningPtr register_connection(IPv4Address local_addr, IPv4Address remote_addr, TcbNonOwningPtr tcb);

    /** Returns local IPv4 address of the stack's network interface */
    IPv4Address local_address() const noexcept;

    /** Thread-safe allocation of an unused ephemeral port (49152..65535) */
    uint16_t allocate_ephemeral_port() noexcept;

#ifdef TCP_STACK_TESTING
public:
    using OutboundPacketCallback = std::function<void(IPv4Address src, IPv4Address dst, const TcpPacket& packet)>;

    /** In-memory packet injection helper for testing without TUN device */
    size_t inject_packet(std::span<const uint8_t> raw_bytes) noexcept {
        return process_incoming_packet_(raw_bytes);
    }

    /** Outbound packet interception helper for testing */
    void set_outbound_interceptor(OutboundPacketCallback callback) noexcept {
        outbound_interceptor_ = std::move(callback);
    }

    /** Simulates sudden node crash by wiping connection TCB without FIN/RST */
    void simulate_node_crash(IPv4Address remote_addr, IPv4Address local_addr) noexcept {
        connection_table_.erase(remote_addr, local_addr);
    }

private:
    OutboundPacketCallback outbound_interceptor_;
#endif
};


#endif //TCP_FROM_SCRATCH_TCPDEMULTIPLEXER_H
