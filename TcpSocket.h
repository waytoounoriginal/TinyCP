//
// Created by waytoounoriginal on 8/11/2026.
//

#ifndef TCP_FROM_SCRATCH_TCPSOCKET_H
#define TCP_FROM_SCRATCH_TCPSOCKET_H

#include <cstdint>
#include <memory>
#include <span>

#include "utils/Platform.h"
#include "TcpStack.h"
#include "TunDevice.h"
#include "utils/Logger.h"
#include "IPv4.h"
#include "TransmissionControlBlock.h"


/** Primary userspace TCP socket interface */
class TcpSocket {
private:
    TcpStack& stack_;

    /** Unique identifier of the underlying TCB owned by SocketTable */
    uint64_t socket_id_{0};

public:
    /** Constructs an unbound TCP socket */
    explicit TcpSocket(TcpStack& stack) : stack_(stack) {}

    /** Constructs a TCP socket wrapping an existing socket ID (used by accept()) */
    explicit TcpSocket(TcpStack& stack, uint64_t socket_id) : stack_(stack), socket_id_(socket_id) {}

    /** Returns current TCP state (per RFC 793) */
    TcpState state() const;

    /** Returns unique socket ID */
    uint64_t socket_id() const {
        return socket_id_;
    }

    /** Binds socket to a local address/port (auto-allocates if 0) */
    void bind(IPv4Address addr);

    /** Transitions socket to LISTEN state for passive open */
    void listen();

    /** Initiates active 3-way handshake to remote address */
    void connect(IPv4Address addr);

    /** Blocks until an incoming connection is established and returns a connected TcpSocket */
    TcpSocket accept();

    /** Writes payload data to send buffer and notifies stack */
    size_t send(std::span<const uint8_t> data, int32_t retries = 3) const;

    /** Reads payload data from receive buffer */
    size_t recv(std::span<uint8_t> data) const;

    /** Closes connection and initiates active FIN teardown */
    void close();
};


#endif //TCP_FROM_SCRATCH_TCPSOCKET_H
