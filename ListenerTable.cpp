#include "ListenerTable.h"

void ListenerTable::bind(uint16_t port, uint64_t socket_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_[port] = socket_id;
}

uint64_t ListenerTable::bind_new(IPv4Address addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t socket_id = socket_table_.create_socket(TcpState::CLOSED, addr);
    listeners_[addr.port] = socket_id;
    return socket_id;
}

uint64_t ListenerTable::find(uint16_t port) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = listeners_.find(port);
    if (it != listeners_.end()) {
        uint64_t socket_id = it->second;
        if (socket_table_.contains(socket_id)) {
            return socket_id;
        }
    }
    return 0;
}

bool ListenerTable::unbind(uint16_t port) {
    uint64_t socket_id = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = listeners_.find(port);
        if (it != listeners_.end()) {
            socket_id = it->second;
            listeners_.erase(it);
        }
    }
    if (socket_id != 0) {
        return socket_table_.destroy_socket(socket_id);
    }
    return false;
}

bool ListenerTable::contains(uint16_t port) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = listeners_.find(port);
    if (it != listeners_.end()) {
        return socket_table_.contains(it->second);
    }
    return false;
}
