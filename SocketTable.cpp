#include "SocketTable.h"

uint64_t SocketTable::create_socket(TcpState state, IPv4Address src, IPv4Address dst) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t id = next_id_++;
    auto tcb = std::make_unique<TransmissionControlBlock>(state, src, dst);
    tcb->id = id;
    sockets_[id] = std::move(tcb);
    return id;
}

bool SocketTable::destroy_socket(uint64_t id) {
    if (id == 0) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    return sockets_.erase(id) > 0;
}

TransmissionControlBlock* SocketTable::find_socket(uint64_t id) const {
    if (id == 0) return nullptr;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sockets_.find(id);
    if (it != sockets_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool SocketTable::contains(uint64_t id) const {
    if (id == 0) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    return sockets_.find(id) != sockets_.end();
}

size_t SocketTable::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sockets_.size();
}
