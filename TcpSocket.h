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


class TcpSocket {
private:
    TcpStack& stack_;

    /** Access to the block owned by the TcpStack */
    TcbSharedResource tcb_;

public:
    explicit TcpSocket(TcpStack& stack) noexcept : stack_(stack) {}
    explicit TcpSocket(TcpStack& stack, TcbSharedResource tcb) noexcept : stack_(stack), tcb_(tcb) {}

    /** Returns the connection state, per RFC 793. */
    TcpState state() const noexcept {
        return tcb_ ? tcb_->current_state : TcpState::CLOSED;
    }

    TcbSharedResource tcb() const noexcept {
        return tcb_;
    }

    /** Registers an entry in the demultiplexer */
    void bind(IPv4Address addr) noexcept;

    /** Puts the socket in listening mode */
    void listen() noexcept;

    /** Initialize the connection */
    void connect(IPv4Address addr) noexcept;

    /** Passive accept logic, returns a new connected socket */
    TcpSocket accept();

    size_t send(std::span<const uint8_t> data) const noexcept;

    size_t recv(std::span<uint8_t> data) const noexcept;
};


#endif //TCP_FROM_SCRATCH_TCPSOCKET_H
