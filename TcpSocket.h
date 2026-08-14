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
    /** Access to the block owned by the TcpStack */
    TcbSharedResource tcb_;

public:
    /** Returns the connection state, per RFC 793. */
    TcpState state() const noexcept {
        return tcb_->current_state;
    }

    TcbSharedResource tcb() const noexcept {
        return tcb_;
    }

    /** Registers an entry in the demultiplexer */
    inline void bind(IPv4Address addr) noexcept {
        Logger::instance().info() << "Binding socket to port: " << addr.port;

        tcb_ = TcpStack::instance().bind_socket(addr);
    };

    /** Puts the socket in listening mode */
    inline void listen() noexcept {
        if (tcb_->current_state != TcpState::CLOSED) {
            Logger::instance().warn()
                << "Trying to LISTEN, but socket is "
            << TCP_STATE_TO_STRING(tcb_->current_state);
            return;
        }

        Logger::instance().info() << "Moving state to LISTEN";

        tcb_->current_state = TcpState::LISTEN;
    }

    /** Initialize the connection */
    inline void connect(IPv4Address addr) noexcept {
    }

    void accept();

    inline size_t send(std::span<const uint8_t> data) const noexcept {
        // Write into the send buffer and notify the stack
        auto sent_data = tcb_->send_buffer.write(data);
        TcpStack::instance().add_dirty_tcb(tcb_);

        return sent_data;
    }

    inline size_t recv(std::span<uint8_t> data) const noexcept {
        return tcb_->recv_buffer.read(data);
    }
};


#endif //TCP_FROM_SCRATCH_TCPSOCKET_H
