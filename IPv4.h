//
// Created by waytoounoriginal on 8/12/2026.
//

#ifndef TCP_FROM_SCRATCH_IPV4_H
#define TCP_FROM_SCRATCH_IPV4_H

#include <cstdint>

/** A simplified IPv4 address API */
struct IPv4Address {
    uint32_t address;
    uint16_t port;

    bool operator==(const IPv4Address& other) const = default;
};

#endif //TCP_FROM_SCRATCH_IPV4_H
