#ifndef TCP_FROM_SCRATCH_CONNECTIONTABLE_H
#define TCP_FROM_SCRATCH_CONNECTIONTABLE_H

#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "IPv4.h"
#include "SocketTable.h"
#include "TransmissionControlBlock.h"

/**
 * Thread-safe table mapping active 4-tuple connections to non-owning TCB references.
 * Depends on SocketTable for creating/owning TCB resources.
 */
class ConnectionTable {
public:
    struct ConnectionKey {
        IPv4Address src_address; // Remote client address
        IPv4Address dst_address; // Local server address

        bool operator==(const ConnectionKey& other) const {
            return src_address == other.src_address && dst_address == other.dst_address;
        }
    };

    struct ConnectionKeyHash {
        std::size_t operator()(const ConnectionKey& key) const noexcept {
            std::size_t h{0};
            h ^= std::hash<uint32_t>{}(key.src_address.address) + 0x71be114 + (h << 6) + (h >> 2);
            h ^= std::hash<uint16_t>{}(key.src_address.port) + 0x71be114 + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>{}(key.dst_address.address) + 0x71be114 + (h << 6) + (h >> 2);
            h ^= std::hash<uint16_t>{}(key.dst_address.port) + 0x71be114 + (h << 6) + (h >> 2);
            return h;
        }
    };

private:
    SocketTable& socket_table_;
    mutable std::mutex mutex_;
    std::unordered_map<ConnectionKey, TransmissionControlBlock*, ConnectionKeyHash> connections_;

public:
    explicit ConnectionTable(SocketTable& socket_table) : socket_table_(socket_table) {}
    ~ConnectionTable() = default;

    ConnectionTable(const ConnectionTable&) = delete;
    ConnectionTable& operator=(const ConnectionTable&) = delete;

    /** Inserts an existing TCB into the 4-tuple connection routing table */
    void insert(IPv4Address remote_addr, IPv4Address local_addr, TransmissionControlBlock* tcb);

    /** Registers a connection: uses existing TCB if non-null, or allocates a new TCB via SocketTable */
    TransmissionControlBlock* register_connection(IPv4Address local_addr, IPv4Address remote_addr, TransmissionControlBlock* tcb);

    /** Looks up an active connection TransmissionControlBlock by 4-tuple */
    TransmissionControlBlock* find(IPv4Address remote_addr, IPv4Address local_addr) const;

    /** Removes a 4-tuple connection mapping */
    bool erase(IPv4Address remote_addr, IPv4Address local_addr);
};

#endif // TCP_FROM_SCRATCH_CONNECTIONTABLE_H
