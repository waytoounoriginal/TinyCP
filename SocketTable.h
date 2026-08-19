#ifndef TCP_FROM_SCRATCH_SOCKETTABLE_H
#define TCP_FROM_SCRATCH_SOCKETTABLE_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "TransmissionControlBlock.h"

/**
 * Thread-safe table owning all TransmissionControlBlock instances in the TCP stack.
 */
class SocketTable {
private:
    mutable std::mutex mutex_;
    uint64_t next_id_{1};
    std::unordered_map<uint64_t, std::unique_ptr<TransmissionControlBlock>> sockets_;

public:
    SocketTable() = default;
    ~SocketTable() = default;

    SocketTable(const SocketTable&) = delete;
    SocketTable& operator=(const SocketTable&) = delete;

    /** Allocates a new TransmissionControlBlock and returns its assigned unique socket ID */
    uint64_t create_socket(TcpState state = TcpState::CLOSED, IPv4Address src = {}, IPv4Address dst = {});

    /** Destroys and frees a TransmissionControlBlock owned by this table by its socket ID */
    bool destroy_socket(uint64_t id);

    /** Looks up a TransmissionControlBlock by its unique socket ID */
    TransmissionControlBlock* find_socket(uint64_t id) const;

    /** Checks if a socket ID exists in the table */
    bool contains(uint64_t id) const;

    /** Returns current number of active sockets */
    size_t size() const;
};

#endif // TCP_FROM_SCRATCH_SOCKETTABLE_H
