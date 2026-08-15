//
// Created by waytoounoriginal on 8/11/2026.
//

#include "TcpSocket.h"
#include "utils/Logger.h"

void TcpSocket::bind(IPv4Address addr) noexcept {
    Logger::instance().info() << "Binding socket to port: " << addr.port;
    tcb_ = stack_.bind_socket(addr);
}

void TcpSocket::listen() noexcept {
    if (!tcb_ || tcb_->current_state != TcpState::CLOSED) {
        Logger::instance().warn()
            << "Trying to LISTEN, but socket state is invalid";
        return;
    }

    Logger::instance().info() << "Moving state to LISTEN";
    tcb_->current_state = TcpState::LISTEN;
}

void TcpSocket::connect(IPv4Address addr) noexcept {
    // Connection handshake logic
}

void TcpSocket::accept() {
    // Passive accept logic
}

size_t TcpSocket::send(std::span<const uint8_t> data) const noexcept {
    if (!tcb_) return 0;
    auto sent_data = tcb_->send_buffer.write(data);
    stack_.add_dirty_tcb(tcb_);
    return sent_data;
}

size_t TcpSocket::recv(std::span<uint8_t> data) const noexcept {
    if (!tcb_) return 0;
    return tcb_->recv_buffer.read(data);
}
