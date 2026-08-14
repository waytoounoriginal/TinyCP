//
// Created by waytoounoriginal on 8/10/2026.
//

#include "IPv4Packet.h"
#include "IPv4PacketView.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <ostream>
#include "utils/Platform.h"

/** Shared dump format for IPv4Header and IPv4PacketView. */
template <typename Header>
static void PrintIPv4Header(std::ostream& os, const Header& header) {
    const uint8_t version = header.version();
    const uint8_t ihl = header.IHL();

    const uint16_t total_length = header.total_length_ntoh();
    const uint16_t identification = header.identification_ntoh();

    const uint8_t flags = header.flags();
    const uint16_t fragment_offset = header.fragment_offset();

    const uint16_t checksum = header.checksum_ntoh();

    const auto print_ip = [&os](uint32_t address) {
        address = ntohl(address);

        os << ((address >> 24) & 0xFF) << '.'
           << ((address >> 16) & 0xFF) << '.'
           << ((address >> 8) & 0xFF) << '.'
           << (address & 0xFF);
    };

    os << "IPv4 Header {\n"
       << "  Version:            " << static_cast<int>(version) << '\n'
       << "  IHL:                " << static_cast<int>(ihl)
       << " (" << static_cast<int>(header.header_length()) << " bytes)\n"
       << "  Type of Service:    0x"
       << std::hex
       << std::setw(2)
       << std::setfill('0')
       << static_cast<int>(header.type_of_service())
       << std::dec
       << std::setfill(' ')
       << '\n'
       << "  Total Length:       " << total_length << '\n'
       << "  Identification:     0x"
       << std::hex
       << std::setw(4)
       << std::setfill('0')
       << identification
       << std::dec
       << std::setfill(' ')
       << '\n'
       << "  Flags:              0b"
       << ((flags >> 2) & 1)
       << ((flags >> 1) & 1)
       << (flags & 1)
       << '\n'
       << "  Fragment Offset:    " << fragment_offset << '\n'
       << "  TTL:                " << static_cast<int>(header.ttl()) << '\n'
       << "  Protocol:           " << static_cast<int>(header.protocol()) << '\n'
       << "  Checksum:           0x"
       << std::hex
       << std::setw(4)
       << std::setfill('0')
       << checksum
       << std::dec
       << std::setfill(' ')
       << '\n'
       << "  Source:             ";

    print_ip(header.source_address());

    os << "\n"
       << "  Destination:        ";

    print_ip(header.destination_address());

    os << "\n}";
}

std::ostream& operator<<(std::ostream& os, const IPv4Header& header) {
    PrintIPv4Header(os, header);
    return os;
}

std::ostream& operator<<(std::ostream& os, const IPv4PacketView& packet) {
    PrintIPv4Header(os, packet);
    return os;
}

void IPv4Header::serialize(std::span<uint8_t> out) const noexcept {
    std::memcpy(out.data(), this, sizeof(IPv4Header));
}

uint16_t IPv4Header::compute_checksum() const noexcept {
    uint8_t wire[20];
    serialize(wire);
    wire[10] = 0;
    wire[11] = 0;
    return OnesComplementChecksum(wire, sizeof(wire));
}

size_t WriteIPv4Packet(std::span<uint8_t> out, const IPv4Packet& packet) {
    const size_t header_len = packet.header.header_length() > 0 ? packet.header.header_length() : sizeof(IPv4Header);
    const size_t total = header_len + packet.payload.size();
    if (out.size() < total) {
        return 0;
    }

    IPv4Header header = packet.header;
    header.set_total_length(static_cast<uint16_t>(total));
    header.set_checksum(header.compute_checksum());

    uint8_t wire[20];
    header.serialize(wire);
    std::copy_n(wire, sizeof(wire), out.data());
    std::copy_n(packet.payload.data(), packet.payload.size(),
                out.data() + header_len);
    return total;
}

