//
// Created by waytoounoriginal on 8/10/2026.
//

#ifndef TCP_FROM_SCRATCH_TUNREADER_H
#define TCP_FROM_SCRATCH_TUNREADER_H
#include <cstddef>
#include "utils/Platform.h"
#include "IPv4.h"

using file_descriptor_t = int;

/**
 * Instance-based Virtual TUN Network Device.
 * Opens and manages a Linux TUN interface (e.g. /dev/net/tun) for reading/writing raw IP packets.
 */
class TunDevice {
public:
    /** Opens and initializes a TUN device with the specified interface name (default: "tun0") */
    explicit TunDevice(const char* dev_name = "tun0");
    ~TunDevice();

    /** Wrapper of read over the TUN file descriptor */
    size_t tun_read(char* buf, size_t len) const;

    /** Wrapper of write over the TUN file descriptor */
    size_t tun_write(const char* buf, size_t len) const;

    /** Retrieves local IP address of the TUN device. Returns default (10.0.0.2) if unassigned */
    IPv4Address get_usable_ip_address() const noexcept;

    TunDevice(const TunDevice&) = delete;
    TunDevice& operator=(const TunDevice&) = delete;
    TunDevice(TunDevice&&) = delete;
    TunDevice& operator=(TunDevice&&) = delete;

private:
    file_descriptor_t fd{-1};
    char name_[256] = {};

    file_descriptor_t tun_alloc(char* dev);
};


#endif //TCP_FROM_SCRATCH_TUNREADER_H