//
// Created by waytoounoriginal on 8/13/2026.
//

#ifndef TCP_FROM_SCRATCH_TRANSMISSIONCONTROLBLOCK_H
#define TCP_FROM_SCRATCH_TRANSMISSIONCONTROLBLOCK_H

#include <assert.h>

#include "IPv4.h"
#include "BlockingBuffer.h"

constexpr size_t MAX_IPV4_PACKET_SIZE = 65535;

/* Simple X-Macro bc cpp does not have reflection */
#define STATES_MACRO        \
    X(CLOSED),              \
    X(LISTEN),              \
    X(SYN_SENT),            \
    X(SYN_RECEIVED),        \
    X(ESTABLISHED),         \
    X(FIN_WAIT_1),          \
    X(FIN_WAIT_2),          \
    X(CLOSE_WAIT),          \
    X(CLOSING),             \
    X(LAST_ACK),            \
    X(TIME_WAIT),

/** The states of a TCP connection, as per RFC 793 Section 3.2. */
enum TcpState {
#define X(x) x
    STATES_MACRO
#undef X
};

/** Util function for stringifying state */
constexpr const char* TCP_STATE_TO_STRING(TcpState state) noexcept {
    constexpr const char *stateNames[] = {
#define X(x) #x
        STATES_MACRO
#undef X
    };

    assert(state >= TcpState::CLOSED && state <= TcpState::LAST_ACK);
    return stateNames[state];
}

#undef STATES_MACRO

#include <condition_variable>
#include <mutex>
#include <queue>
#include <memory>

/** The control block structure of the socket
*   Holds the internal state of the socket
*/
struct TransmissionControlBlock {
    /** Current state in the TCP connection */
    TcpState current_state = TcpState::CLOSED;

    /** Synchronization for state transitions and blocking API calls */
    mutable std::mutex state_mutex;
    std::condition_variable state_cv;

    /** Queue of completed established connections for listening sockets */
    std::queue<std::shared_ptr<TransmissionControlBlock>> accept_queue;

    /** The source */
    IPv4Address src_address;

    /** The destination */
    IPv4Address dst_address;

    /** Send Sequence Variables */
    struct {
        uint32_t    UNA = 0,    /* Send unacknowledged */
                    NXT = 0,    /* Send next */
                    WND = 65535,/* Send window */
                    UP = 0,     /* Send urgent pointer */
                    WL1 = 0,    /* seg nr for last window update */
                    WL2 = 0,    /* seg ack nr for last window update */
                    ISS = 0;    /* Initial send sequence nr */
    } SND;

    /** Receive Sequence Variables */
    struct {
        uint32_t    NXT = 0, /* Receive next */
                    WND = 65535, /* Receive window */
                    UP = 0,  /* Receive urgent pointer */
                    IRS = 0; /* Initial receive sequence number */
    } RCV;

    /** Current Segment Variables */
    struct {
        uint32_t    SEQ = 0, /* Segment Seq nr */
                    ACK = 0, /* Segment Ack nr */
                    LEN = 0, /* Segment Len */
                    WND = 0, /* Segment Window */
                    UP = 0,  /* Segment urgent pointer */
                    PRC = 0; /* Segment precedence value */
    } SEG;

    /** Retransmission timeout */
    uint32_t RTO = 200;

    /** Timestamp of the retransmission timer */
    uint64_t retransmission_timer = 0;

    /** User timeout */
    uint64_t user_timeout = 0;

    /** Send buffer */
    BlockingBuffer<uint8_t, MAX_IPV4_PACKET_SIZE> send_buffer;

    /** Receive buffer */
    BlockingBuffer<uint8_t, MAX_IPV4_PACKET_SIZE> recv_buffer;

    TransmissionControlBlock() = default;

    TransmissionControlBlock(TcpState state, IPv4Address src = {}, IPv4Address dst = {})
        : current_state(state), src_address(src), dst_address(dst) {}

    void set_state(TcpState new_state) noexcept {
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            current_state = new_state;
        }
        state_cv.notify_all();
    }
};

#endif //TCP_FROM_SCRATCH_TRANSMISSIONCONTROLBLOCK_H
