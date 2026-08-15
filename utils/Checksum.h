#ifndef TCP_FROM_SCRATCH_CHECKSUM_H
#define TCP_FROM_SCRATCH_CHECKSUM_H

#include <cstddef>
#include <cstdint>

/**
 * Computes the 16-bit one's complement checksum (RFC 1071) over a
 * buffer of network-order bytes.
 *
 * The result is the checksum value to place in the header: the sum
 * of all 16-bit words (including the checksum word) of a valid
 * header complements to zero. Odd-length input is padded on the
 * right with a zero byte, per RFC 791.
 */
#include "IPv4.h"

/**
 * Computes the 16-bit one's complement checksum (RFC 1071) over a
 * buffer of network-order bytes.
 *
 * The result is the checksum value to place in the header: the sum
 * of all 16-bit words (including the checksum word) of a valid
 * header complements to zero. Odd-length input is padded on the
 * right with a zero byte, per RFC 791.
 */
uint16_t OnesComplementChecksum(const uint8_t* data, size_t length);

/**
 * Computes the RFC 793 TCP checksum over the IPv4 pseudo-header,
 * TCP header, and TCP payload.
 */
uint16_t ComputeTcpChecksum(IPv4Address src, IPv4Address dst,
                           const uint8_t* header_bytes, size_t header_len,
                           const uint8_t* payload_bytes, size_t payload_len);

uint16_t ComputeTcpChecksum(uint32_t src_ip_net, uint32_t dst_ip_net,
                           const uint8_t* header_bytes, size_t header_len,
                           const uint8_t* payload_bytes, size_t payload_len);

#endif // TCP_FROM_SCRATCH_CHECKSUM_H