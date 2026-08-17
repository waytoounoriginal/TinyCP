#include "SocketTable.h"

TransmissionControlBlock* SocketTable::create_socket(TcpState state, IPv4Address src, IPv4Address dst) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t id = next_id_++;
    auto tcb = std::make_unique<TransmissionControlBlock>(state, src, dst);
    TransmissionControlBlock* raw_ptr = tcb.get();
    sockets_[id] = std::move(tcb);
    return raw_ptr;
}

bool SocketTable::destroy_socket(TransmissionControlBlock* tcb) {
    if (!tcb) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = sockets_.begin(); it != sockets_.end(); ++it) {
        if (it->second.get() == tcb) {
            sockets_.erase(it);
            return true;
        }
    }
    return false;
}

TransmissionControlBlock* SocketTable::find_socket(uint64_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sockets_.find(id);
    if (it != sockets_.end()) {
        return it->second.get();
    }
    return nullptr;
}

size_t SocketTable::size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return sockets_.size();
}
