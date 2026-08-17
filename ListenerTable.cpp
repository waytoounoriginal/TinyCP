#include "ListenerTable.h"

void ListenerTable::bind(uint16_t port, TransmissionControlBlock* tcb) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_[port] = tcb;
}

TransmissionControlBlock* ListenerTable::bind_new(IPv4Address addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    TransmissionControlBlock* tcb = socket_table_.create_socket(TcpState::CLOSED, addr);
    listeners_[addr.port] = tcb;
    return tcb;
}

TransmissionControlBlock* ListenerTable::find(uint16_t port) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = listeners_.find(port);
    if (it != listeners_.end()) {
        return it->second;
    }
    return nullptr;
}

bool ListenerTable::unbind(uint16_t port) {
    std::lock_guard<std::mutex> lock(mutex_);
    return listeners_.erase(port) > 0;
}

bool ListenerTable::contains(uint16_t port) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return listeners_.find(port) != listeners_.end();
}
