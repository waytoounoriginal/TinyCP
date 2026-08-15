#include "TunDevice.h"
#include <cstring>

TunDevice::TunDevice(const char* dev_name) {
    char name[256] = {};
    if (dev_name) {
        std::strncpy(name, dev_name, sizeof(name) - 1);
    } else {
        std::strcpy(name, "tun0");
    }
    fd = tun_alloc(name);
}

TunDevice::~TunDevice() {
#ifndef _WIN32
    if (fd >= 0) {
        ::close(fd);
    }
#endif
}

#ifndef _WIN32
#include <poll.h>

/** Polled read, detects if there is something to read in the fd. Returns 0 if not */
inline size_t polled_read(int fd, char* buf, size_t len) noexcept {
    pollfd pfd {};
    pfd.fd = fd;
    pfd.events = POLLIN;

    auto result = poll(&pfd, 1, 0);

    if (result == 0) {
        // no data available
        return 0;
    } else if (pfd.revents & POLLIN) {
        return static_cast<size_t>(read(fd, buf, len));
    }
    return 0;
}
#endif

size_t TunDevice::tun_read(char* buf, size_t len) const {
#ifndef _WIN32
    if (fd < 0) return 0;
    return polled_read(fd, buf, len);
#else
    (void)buf; (void)len;
    return 0;
#endif
}

size_t TunDevice::tun_write(const char* buf, size_t len) const {
#ifndef _WIN32
    if (fd < 0) return 0;
    auto bytes = ::write(fd, buf, len);
    return bytes > 0 ? static_cast<size_t>(bytes) : 0;
#else
    (void)buf; (void)len;
    return 0;
#endif
}

#ifndef _WIN32
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>

/*
 * Taken from Kernel Documentation/networking/tuntap.txt
 */
int TunDevice::tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd_out, err;

    if( (fd_out = open("/dev/net/tun", O_RDWR)) < 0 ) {
        perror("Cannot open TUN/TAP dev");
        exit(1);
    }

    // Clear memory
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if (dev && *dev) {
        strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
    }

    if( (err = ioctl(fd_out, TUNSETIFF, (void *) &ifr)) < 0 ){
        perror("ERR: Could not ioctl tun:\n");
        close(fd_out);
        return err;
    }

    strcpy(dev, ifr.ifr_name);
    return fd_out;
}
#else
int TunDevice::tun_alloc(char* /*dev*/) {
    return -1;
}
#endif
