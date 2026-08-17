//
// Created by waytoounoriginal on 8/11/2026.
//

#ifndef TCP_FROM_SCRATCH_TCPSOCKET_H
#define TCP_FROM_SCRATCH_TCPSOCKET_H

#include <cstdint>
#include <memory>
#include <span>

#include "utils/Platform.h"
#include "TcpStack.h"
#include "TunDevice.h"
#include "utils/Logger.h"
#include "IPv4.h"
#include "TransmissionControlBlock.h"


/** Primary userspace TCP socket interface */
class TcpSocket {
private:
    TcpStack& stack_;

    /** Non-owning reference to the Transmission Control Block owned by the TcpStack */
    TcbNonOwningPtr tcb_{nullptr};

public:
    /** Constructs an unbound TCP socket */
    explicit TcpSocket(TcpStack& stack) noexcept : stack_(stack) {}

    /** Constructs a TCP socket wrapping an existing TCB (used by accept()) */
    explicit TcpSocket(TcpStack& stack, TcbNonOwningPtr tcb) noexcept : stack_(stack), tcb_(tcb) {}

    /** Returns current TCP state (per RFC 793) */
    TcpState state() const noexcept {
        return tcb_ ? tcb_->current_state : TcpState::CLOSED;
    }

    /** Returns non-owning underlying TCB pointer */
    TcbNonOwningPtr tcb() const noexcept {
        return tcb_;
    }

    /** Binds socket to a local address/port (auto-allocates if 0) */
    void bind(IPv4Address addr) noexcept;

    /** Transitions socket to LISTEN state for passive open */
    void listen() noexcept;

    /** Initiates active 3-way handshake to remote address */
    void connect(IPv4Address addr) noexcept;

    /** Blocks until an incoming connection is established and returns a connected TcpSocket */
    TcpSocket accept();

    /** Writes payload data to send buffer and notifies stack */
    size_t send(std::span<const uint8_t> data) const noexcept;

    /** Reads payload data from receive buffer */
    size_t recv(std::span<uint8_t> data) const noexcept;
};


#endif //TCP_FROM_SCRATCH_TCPSOCKET_H
