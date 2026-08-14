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
constexpr const char *TCP_STATE_TO_STRING(TcpState state) noexcept {
    constexpr const char *stateNames[] = {
#define X(x) #x
        STATES_MACRO
#undef X
    };

    assert(state >= TcpState::CLOSED && state <= TcpState::LAST_ACK);

    return stateNames[state];
}

#undef STATES_MACRO

/** The control block structure of the socket
*   Holds the internal state of the socket
*/
struct TransmissionControlBlock {
    /** Current state in the TCP connection */
    TcpState current_state = TcpState::CLOSED;

    /** The source */
    IPv4Address src_address;

    /** The destination */
    IPv4Address dst_address;

    /** Send Sequence Variables */
    struct {
        uint32_t    UNA,    /* Send unacknowleged */
                    NXT,    /* Send next */
                    WND,    /* Send window */
                    UP,     /* Send urgent pointer */
                    WL1,    /* ss nr for last window update */
                    WL2,    /* seg. ack nr for last window update */
                    ISS;    /* Initial send sequence nr */
    } SND;

    /** Receive Sequence Variables */
    struct {
        uint32_t    NXT, /* Receive next */
                    WND, /* Receive window */
                    UP, /* Receive urgent pointer */
                    IRS; /* Initial receive sequence number */
    } RCV;

    /** Retransmission timeout */
    uint32_t RTO;

    /** Timestamp of the retransmission timer */
    uint64_t retransmission_timer;

    /** User timeout */
    uint64_t user_timeout;

    /** Send buffer */
    BlockingBuffer<uint8_t, MAX_IPV4_PACKET_SIZE> send_buffer;

    /** Receive buffer */
    BlockingBuffer<uint8_t, MAX_IPV4_PACKET_SIZE> recv_buffer;

};

#endif //TCP_FROM_SCRATCH_TRANSMISSIONCONTROLBLOCK_H
