//
// Created by waytoounoriginal on 8/11/2026.
//

#include "TcpSocket.h"
#include "utils/Logger.h"
#include "utils/Utils.h"

void TcpSocket::bind(IPv4Address addr) noexcept {
    if (addr.address == 0) {
        addr.address = stack_.local_address().address;
    }
    if (addr.port == 0) {
        addr.port = stack_.allocate_ephemeral_port();
    }
    Logger::instance().info() << "Binding socket to port: " << addr.port;
    tcb_ = stack_.bind_socket(addr);
    if (!tcb_) return;

    tcb_->SND.ISS = generate_random_uint32();
    tcb_->SND.NXT = tcb_->SND.ISS;
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
    IPv4Address local_addr = tcb_ ? tcb_->src_address : IPv4Address{0, 0};
    tcb_ = stack_.register_connection(local_addr, addr, tcb_);
    if (!tcb_) return;

    Logger::instance().info() << "[TcpSocket] Initiating 3-Way Handshake (SYN) to "
                              << ((addr.address >> 24) & 0xFF) << "."
                              << ((addr.address >> 16) & 0xFF) << "."
                              << ((addr.address >> 8) & 0xFF) << "."
                              << (addr.address & 0xFF) << ":" << addr.port;

    tcb_->SND.ISS = generate_random_uint32();
    tcb_->SND.UNA = tcb_->SND.ISS;
    tcb_->SND.NXT = tcb_->SND.ISS + 1;
    tcb_->set_state(TcpState::SYN_SENT);

    // Queue SYN transmission
    stack_.add_dirty_tcb(tcb_);

    // Wait until 3-way handshake finishes (ESTABLISHED) or fails
    std::unique_lock<std::mutex> lock(tcb_->state_mutex);
    tcb_->state_cv.wait(lock, [this] {
        return tcb_ && (tcb_->current_state == TcpState::ESTABLISHED || tcb_->current_state == TcpState::CLOSED);
    });

    if (tcb_ && tcb_->current_state == TcpState::ESTABLISHED) {
        Logger::instance().info() << "[TcpSocket] Connection established successfully with " << addr.port;
    }
}

TcpSocket TcpSocket::accept() {
    if (!tcb_ || tcb_->current_state != TcpState::LISTEN) {
        Logger::instance().warn() << "Calling accept() on socket that is not in LISTEN state";
        return TcpSocket{stack_};
    }

    Logger::instance().info() << "[TcpSocket] Waiting in accept() for incoming connection on port " << tcb_->src_address.port << "...";

    std::unique_lock<std::mutex> lock(tcb_->state_mutex);
    tcb_->state_cv.wait(lock, [this] {
        return !tcb_->accept_queue.empty();
    });

    auto child_tcb = tcb_->accept_queue.front();
    tcb_->accept_queue.pop();

    Logger::instance().info() << "[TcpSocket] Accepted connection from " << child_tcb->dst_address.port;

    return TcpSocket{stack_, child_tcb};
}

size_t TcpSocket::send(std::span<const uint8_t> data) const noexcept {
    if (!tcb_) return 0;

    auto sent_data = tcb_->send_buffer.write(data);
    stack_.add_dirty_tcb(tcb_);
    return sent_data;
}

size_t TcpSocket::recv(std::span<uint8_t> data) const noexcept {
    if (!tcb_) return 0;

    // Aslo check that the connection was established

    return tcb_->recv_buffer.read(data);
}
