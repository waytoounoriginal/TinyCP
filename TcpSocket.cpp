//
// Created by waytoounoriginal on 8/11/2026.
//

#include "TcpSocket.h"
#include "utils/Logger.h"
#include "utils/Utils.h"

TcpState TcpSocket::state() const {
    auto* tcb = stack_.get_tcb(socket_id_);
    return tcb ? tcb->current_state : TcpState::CLOSED;
}

void TcpSocket::bind(IPv4Address addr) {
    if (addr.address == 0) {
        addr.address = stack_.local_address().address;
    }
    if (addr.port == 0) {
        addr.port = stack_.allocate_ephemeral_port();
    }
    INFO << "Binding socket to port: " << addr.port;
    socket_id_ = stack_.bind_socket(addr);
    auto* tcb = stack_.get_tcb(socket_id_);
    if (!tcb) return;

    tcb->SND.ISS = generate_random_uint32();
    tcb->SND.NXT = tcb->SND.ISS;
}

void TcpSocket::listen() {
    auto* tcb = stack_.get_tcb(socket_id_);
    if (!tcb || tcb->current_state != TcpState::CLOSED) {
        WARN << "Trying to LISTEN, but socket state is invalid";
        return;
    }

    INFO << "Moving state to LISTEN";
    tcb->current_state = TcpState::LISTEN;
}


void TcpSocket::connect(IPv4Address addr) {
    auto* tcb = stack_.get_tcb(socket_id_);
    IPv4Address local_addr = tcb ? tcb->src_address : IPv4Address{0, 0};
    socket_id_ = stack_.register_connection(local_addr, addr, socket_id_);
    tcb = stack_.get_tcb(socket_id_);
    if (!tcb) return;

    INFO << "[TcpSocket] Initiating 3-Way Handshake (SYN) to "
         << ((addr.address >> 24) & 0xFF) << "."
         << ((addr.address >> 16) & 0xFF) << "."
         << ((addr.address >> 8) & 0xFF) << "."
         << (addr.address & 0xFF) << ":" << addr.port;

    tcb->SND.ISS = generate_random_uint32();
    tcb->SND.UNA = tcb->SND.ISS;
    tcb->SND.NXT = tcb->SND.ISS + 1;
    tcb->set_state(TcpState::SYN_SENT);

    // Queue SYN transmission
    stack_.add_dirty_tcb(socket_id_);

    // Wait until 3-way handshake finishes (ESTABLISHED) or fails
    std::unique_lock<std::mutex> lock(tcb->state_mutex);
    tcb->state_cv.wait(lock, [this] {
        auto* t = stack_.get_tcb(socket_id_);
        return t && (t->current_state == TcpState::ESTABLISHED || t->current_state == TcpState::CLOSED);
    });

    tcb = stack_.get_tcb(socket_id_);
    if (tcb && tcb->current_state == TcpState::ESTABLISHED) {
        INFO << "[TcpSocket] Connection established successfully with " << addr.port;
    }
}

TcpSocket TcpSocket::accept() {
    auto* tcb = stack_.get_tcb(socket_id_);
    if (!tcb || tcb->current_state != TcpState::LISTEN) {
        WARN << "Calling accept() on socket that is not in LISTEN state";
        return TcpSocket{stack_};
    }

    INFO << "[TcpSocket] Waiting in accept() for incoming connection on port " << tcb->src_address.port << "...";

    std::unique_lock<std::mutex> lock(tcb->state_mutex);
    tcb->state_cv.wait(lock, [this] {
        auto* t = stack_.get_tcb(socket_id_);
        return t && !t->accept_queue.empty();
    });

    tcb = stack_.get_tcb(socket_id_);
    if (!tcb || tcb->accept_queue.empty()) {
        ERROR << "[TcpStack] Accept queue was empty.";
        return TcpSocket{stack_};
    }

    uint64_t child_id = tcb->accept_queue.front();
    tcb->accept_queue.pop();

    auto* child_tcb = stack_.get_tcb(child_id);
    if (child_tcb) {
        INFO << "[TcpSocket] Accepted connection from " << child_tcb->dst_address.port;
    }

    return TcpSocket{stack_, child_id};
}

size_t TcpSocket::send(std::span<const uint8_t> data, int32_t retries) const {
    auto* tcb = stack_.get_tcb(socket_id_);
    if (!tcb) return 0;

    auto sent_data = tcb->send_buffer.write(data);

    tcb->has_recived_ack = false;
    stack_.add_dirty_tcb(socket_id_);

    {
        std::unique_lock lock(tcb->state_mutex);

        auto acked = tcb->state_cv.wait_for(
            lock,
            tcb->RTO,
            [&] {
                return tcb->has_recived_ack;
            }
        );

        if (acked) {
            INFO << "[TcpSocket] Send data!";
            return sent_data;
        }

    }

    // Retry write
    if (retries == 0) {
        INFO << "[TcpSocket] Retries out!!!";
        return 4544; // fix later
    }

    return send(data, retries - 1);
}

size_t TcpSocket::recv(std::span<uint8_t> data) const {
    auto* tcb = stack_.get_tcb(socket_id_);
    if (!tcb) return 0;

    auto bytes_read = tcb->recv_buffer.read(data);
    if (bytes_read == 0 && (tcb->current_state == TcpState::CLOSE_WAIT ||
                            tcb->current_state == TcpState::LAST_ACK ||
                            tcb->current_state == TcpState::CLOSED)) {
        return 0; // EOF
    }
    return bytes_read;
}

void TcpSocket::close() {
    if (socket_id_ == 0) return;
    stack_.close_connection(socket_id_);
}
