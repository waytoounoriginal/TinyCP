//
// Created by waytoounoriginal on 8/11/2026.
//

#ifndef TCP_FROM_SCRATCH_TCPSOCKET_H
#define TCP_FROM_SCRATCH_TCPSOCKET_H

#include "TunDevice.h"


class TcpSocket {
public:
    /** Creates (or attaches to) the TUN device this socket talks over. */
    explicit TcpSocket() {}

    /** Socket api */
    void bind();
    void connect();
    void listen();
    void accept();

    inline ssize_t send(std::span<const char> data) const noexcept {
        /* todo */
        return TunDevice::instance().tun_write(data.data(), data.size());
    }

    inline ssize_t recv(const std::span<char> data) const noexcept {
        /* todo */
        return TunDevice::instance().tun_read(data.data(), data.size());
    }
};


#endif //TCP_FROM_SCRATCH_TCPSOCKET_H