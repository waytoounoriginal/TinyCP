//
// Created by waytoounoriginal on 8/10/2026.
//

#ifndef TCP_FROM_SCRATCH_TUNREADER_H
#define TCP_FROM_SCRATCH_TUNREADER_H
#include <cstddef>
#include "utils/Platform.h"

using file_descriptor_t = int;

/**
 * The process's single TUN device.
 *
 * A singleton: the device is opened exactly once, on first use of
 * instance(), and shared by every socket in the stack. Creating more
 * than one TUN device (or racing readers over the same one) would
 * split the traffic between readers, so construction is private and
 * non-copyable.
 */
class TunDevice {
public:
    /** Returns the one TUN device, creating it on first use. */
    inline static TunDevice& instance() {
        static TunDevice device{};
        return device;
    }

    /** Wrapper of read over the TUN file descriptor */
    inline size_t tun_read(char* buf, size_t len) const {
        return static_cast<size_t>(read(fd, buf, len));
    }

    /** Wrapper of write over the TUN file descriptor */
    inline size_t tun_write(const char* buf, size_t len) const {
        return static_cast<size_t>(write(fd, buf, len));
    }

    TunDevice(const TunDevice&) = delete;
    TunDevice& operator=(const TunDevice&) = delete;
    TunDevice(TunDevice&&) = delete;
    TunDevice& operator=(TunDevice&&) = delete;

private:
    /* Private: only instance() may create the device. */
    TunDevice() {
        char name[] = "tun0";
        fd = tun_alloc(name);
    }

    file_descriptor_t fd;

    file_descriptor_t tun_alloc(char* dev);
};


#endif //TCP_FROM_SCRATCH_TUNREADER_H