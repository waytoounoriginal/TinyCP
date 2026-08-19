#include "ConnectionTable.h"

void ConnectionTable::insert(IPv4Address remote_addr, IPv4Address local_addr, uint64_t socket_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[ConnectionKey{remote_addr, local_addr}] = socket_id;
}

uint64_t ConnectionTable::register_connection(IPv4Address local_addr, IPv4Address remote_addr, uint64_t socket_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (socket_id == 0 || !socket_table_.contains(socket_id)) {
        socket_id = socket_table_.create_socket(TcpState::CLOSED, local_addr, remote_addr);
    } else {
        auto* tcb = socket_table_.find_socket(socket_id);
        if (tcb) {
            tcb->src_address = local_addr;
            tcb->dst_address = remote_addr;
        }
    }
    connections_[ConnectionKey{remote_addr, local_addr}] = socket_id;
    return socket_id;
}

uint64_t ConnectionTable::find(IPv4Address remote_addr, IPv4Address local_addr) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(ConnectionKey{remote_addr, local_addr});
    if (it != connections_.end()) {
        uint64_t socket_id = it->second;
        if (socket_table_.contains(socket_id)) {
            return socket_id;
        }
    }
    return 0;
}

bool ConnectionTable::erase(IPv4Address remote_addr, IPv4Address local_addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_.erase(ConnectionKey{remote_addr, local_addr}) > 0;
}

bool ConnectionTable::destroy_connection(IPv4Address remote_addr, IPv4Address local_addr) {
    uint64_t socket_id = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(ConnectionKey{remote_addr, local_addr});
        if (it != connections_.end()) {
            socket_id = it->second;
            connections_.erase(it);
        }
    }
    if (socket_id != 0) {
        return socket_table_.destroy_socket(socket_id);
    }
    return false;
}
