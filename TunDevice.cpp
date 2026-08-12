//
// Created by waytoounoriginal on 8/10/2026.
//

#include "TunDevice.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>

/*
 * Taken from Kernel Documentation/networking/tuntap.txt
 */
int TunDevice::tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd, err;

    if( (fd = open("/dev/net/tun", O_RDWR)) < 0 ) {
        perror("Cannot open TUN/TAP dev");
        exit(1);
    }

    // Clear memory
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if (dev && *dev) {
        strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
    }

    if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
        perror("ERR: Could not ioctl tun:\n");
        close(fd);
        return err;
    }

    strcpy(dev, ifr.ifr_name);
    return fd;

}
