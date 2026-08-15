#include "utils/Checksum.h"

uint16_t OnesComplementChecksum(const uint8_t* data, size_t length) {
    uint32_t sum = 0;

    while (length >= 2) {
        sum += static_cast<uint16_t>((data[0] << 8) | data[1]);
        data += 2;
        length -= 2;
    }

    // Odd-length input: the last octet is padded on the right with zeros.
    if (length == 1) {
        sum += static_cast<uint16_t>(data[0] << 8);
    }

    // Fold any 16-bit carries back into the sum.
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum & 0xFFFF);
}

#include "utils/Platform.h"

uint16_t ComputeTcpChecksum(IPv4Address src, IPv4Address dst,
                           const uint8_t* header_bytes, size_t header_len,
                           const uint8_t* payload_bytes, size_t payload_len) {
    return ComputeTcpChecksum(htonl(src.address), htonl(dst.address),
                              header_bytes, header_len,
                              payload_bytes, payload_len);
}

uint16_t ComputeTcpChecksum(uint32_t src_ip_net, uint32_t dst_ip_net,
                           const uint8_t* header_bytes, size_t header_len,
                           const uint8_t* payload_bytes, size_t payload_len) {
    uint32_t sum = 0;

    // 1. Pseudo-Header: Source IP (4 bytes), Destination IP (4 bytes),
    // Zero (1 byte), Protocol TCP (1 byte = 6), TCP Length (2 bytes)
    const uint8_t* src_p = reinterpret_cast<const uint8_t*>(&src_ip_net);
    sum += static_cast<uint16_t>((src_p[0] << 8) | src_p[1]);
    sum += static_cast<uint16_t>((src_p[2] << 8) | src_p[3]);

    const uint8_t* dst_p = reinterpret_cast<const uint8_t*>(&dst_ip_net);
    sum += static_cast<uint16_t>((dst_p[0] << 8) | dst_p[1]);
    sum += static_cast<uint16_t>((dst_p[2] << 8) | dst_p[3]);

    sum += static_cast<uint16_t>(6); // Protocol TCP
    const uint16_t tcp_len = static_cast<uint16_t>(header_len + payload_len);
    sum += tcp_len;

    // 2. TCP Header bytes
    for (size_t i = 0; i < header_len; i += 2) {
        uint16_t word = static_cast<uint16_t>((header_bytes[i] << 8) | header_bytes[i + 1]);
        sum += word;
    }

    // 3. Payload bytes
    size_t i = 0;
    while (i + 1 < payload_len) {
        uint16_t word = static_cast<uint16_t>((payload_bytes[i] << 8) | payload_bytes[i + 1]);
        sum += word;
        i += 2;
    }
    if (i < payload_len) {
        sum += static_cast<uint16_t>(payload_bytes[i] << 8);
    }

    // Fold carries
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum & 0xFFFF);
}