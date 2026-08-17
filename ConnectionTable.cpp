#include "ConnectionTable.h"

void ConnectionTable::insert(IPv4Address remote_addr, IPv4Address local_addr, TransmissionControlBlock* tcb) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[ConnectionKey{remote_addr, local_addr}] = tcb;
}

TransmissionControlBlock* ConnectionTable::register_connection(IPv4Address local_addr, IPv4Address remote_addr, TransmissionControlBlock* tcb) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tcb) {
        tcb = socket_table_.create_socket(TcpState::CLOSED, local_addr, remote_addr);
    } else {
        tcb->src_address = local_addr;
        tcb->dst_address = remote_addr;
    }
    connections_[ConnectionKey{remote_addr, local_addr}] = tcb;
    return tcb;
}

TransmissionControlBlock* ConnectionTable::find(IPv4Address remote_addr, IPv4Address local_addr) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(ConnectionKey{remote_addr, local_addr});
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

bool ConnectionTable::erase(IPv4Address remote_addr, IPv4Address local_addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_.erase(ConnectionKey{remote_addr, local_addr}) > 0;
}
