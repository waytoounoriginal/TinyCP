#ifndef TCP_FROM_SCRATCH_LISTENERTABLE_H
#define TCP_FROM_SCRATCH_LISTENERTABLE_H

#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "IPv4.h"
#include "SocketTable.h"
#include "TransmissionControlBlock.h"

/**
 * Thread-safe table mapping listening ports to non-owning TCB references.
 * Depends on SocketTable for creating/owning TCB resources.
 */
class ListenerTable {
private:
    SocketTable& socket_table_;
    mutable std::mutex mutex_;
    std::unordered_map<uint16_t, uint64_t> listeners_;

public:
    explicit ListenerTable(SocketTable& socket_table) : socket_table_(socket_table) {}
    ~ListenerTable() = default;

    ListenerTable(const ListenerTable&) = delete;
    ListenerTable& operator=(const ListenerTable&) = delete;

    /** Binds a listening port to a socket ID */
    void bind(uint16_t port, uint64_t socket_id);

    /** Creates a new CLOSED socket via SocketTable, binds it to the given port, and returns its socket ID */
    uint64_t bind_new(IPv4Address addr);

    /** Looks up a listening socket ID by port, checking existence in SocketTable */
    uint64_t find(uint16_t port) const;

    /** Removes a listening port mapping and destroys its socket in SocketTable */
    bool unbind(uint16_t port);

    /** Returns true if a listening port is bound and valid in SocketTable */
    bool contains(uint16_t port) const;
};

#endif // TCP_FROM_SCRATCH_LISTENERTABLE_H
