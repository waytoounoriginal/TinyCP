#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <sstream>

#include "IPv4Packet.h"

namespace {

/** Copies raw wire bytes into an IPv4Header, as a packet parser would. */
IPv4Header Parse(const uint8_t* wire) {
    IPv4Header header;
    std::memcpy(&header, wire, sizeof(header));
    return header;
}

/** Reads a 16-bit field from the wire verbatim, as memcpy would. */
uint16_t Wire16(const uint8_t* p) {
    uint16_t value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

/** Reads a 32-bit field from the wire verbatim, as memcpy would. */
uint32_t Wire32(const uint8_t* p) {
    uint32_t value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

} // namespace

/**
 * A well-known IPv4 header: 45 00 00 3C 1C 46 40 00 40 06 B1 E6
 * AC D9 17 0A C0 A8 01 03, i.e. a 60-byte TCP datagram from
 * 172.217.23.10 to 192.168.1.3 with DF set.
 */
TEST(IPv4HeaderTest, ParsesCanonicalHeader) {
    const uint8_t wire[20] = {
        0x45, 0x00, 0x00, 0x3C, 0x1C, 0x46, 0x40, 0x00,
        0x40, 0x06, 0xB1, 0xE6, 0xAC, 0xD9, 0x17, 0x0A,
        0xC0, 0xA8, 0x01, 0x03,
    };

    const IPv4Header header = Parse(wire);

    EXPECT_EQ(header.version(), 4);
    EXPECT_EQ(header.IHL(), 5);
    EXPECT_EQ(header.header_length(), 20);

    EXPECT_EQ(header.type_of_service(), 0x00);

    EXPECT_EQ(header.total_length_ntoh(), 60);
    EXPECT_EQ(header.identification_ntoh(), 0x1C46);

    EXPECT_EQ(header.flags(), 0b010);
    EXPECT_EQ(header.fragment_offset(), 0);

    EXPECT_EQ(header.ttl(), 64);
    EXPECT_EQ(header.protocol(), 6);

    EXPECT_EQ(header.checksum_ntoh(), 0xB1E6);

    EXPECT_EQ(header.source_address_ntoh(), 0xACD9170A);
    EXPECT_EQ(header.destination_address_ntoh(), 0xC0A80103);
}

TEST(IPv4HeaderTest, RawGettersPreserveWireBytes) {
    const uint8_t wire[20] = {
        0x45, 0x00, 0x12, 0x34, 0xAB, 0xCD, 0x21, 0x11,
        0x2A, 0x11, 0xBE, 0xEF, 0x0A, 0x00, 0x00, 0x01,
        0x0A, 0x00, 0x00, 0x02,
    };

    const IPv4Header header = Parse(wire);

    EXPECT_EQ(header.version_ihl(), wire[0]);
    EXPECT_EQ(header.type_of_service(), wire[1]);
    EXPECT_EQ(header.total_length(), Wire16(wire + 2));
    EXPECT_EQ(header.identification(), Wire16(wire + 4));
    EXPECT_EQ(header.flags_fragment_offset(), Wire16(wire + 6));
    EXPECT_EQ(header.ttl(), wire[8]);
    EXPECT_EQ(header.protocol(), wire[9]);
    EXPECT_EQ(header.checksum(), Wire16(wire + 10));
    EXPECT_EQ(header.source_address(), Wire32(wire + 12));
    EXPECT_EQ(header.destination_address(), Wire32(wire + 16));
}

TEST(IPv4HeaderTest, NtohGettersConvertToHostOrder) {
    const uint8_t wire[20] = {
        0x45, 0x00, 0x12, 0x34, 0xAB, 0xCD, 0x21, 0x11,
        0x2A, 0x11, 0xBE, 0xEF, 0x0A, 0x00, 0x00, 0x01,
        0x0A, 0x00, 0x00, 0x02,
    };

    const IPv4Header header = Parse(wire);

    EXPECT_EQ(header.total_length_ntoh(), 0x1234);
    EXPECT_EQ(header.identification_ntoh(), 0xABCD);
    EXPECT_EQ(header.flags_fragment_offset_ntoh(), 0x2111);
    EXPECT_EQ(header.checksum_ntoh(), 0xBEEF);
    EXPECT_EQ(header.source_address_ntoh(), 0x0A000001);
    EXPECT_EQ(header.destination_address_ntoh(), 0x0A000002);
}

TEST(IPv4HeaderTest, ParsesFlagsAndFragmentOffset) {
    const uint8_t wire[20] = {
        0x45, 0x00, 0x00, 0x3C, 0x00, 0x01, 0x24, 0x1A,
        0x40, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    const IPv4Header header = Parse(wire);

    const uint16_t field = header.flags_fragment_offset_ntoh();
    EXPECT_EQ(header.flags(), ((field >> 13) & 0x7));
    EXPECT_EQ(header.fragment_offset(), (field & 0x1FFF));
    EXPECT_EQ(header.flags(), 0b001);
    EXPECT_EQ(header.fragment_offset(), 1050);

    EXPECT_EQ(header.version(), 4);
    EXPECT_EQ(header.IHL(), 5);
}

TEST(IPv4HeaderTest, ParsesMaximumFlagsAndOffset) {
    const uint8_t wire[20] = {
        0x45, 0x00, 0x00, 0x3C, 0x00, 0x01, 0xFF, 0xFF,
        0x40, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    const IPv4Header header = Parse(wire);

    EXPECT_EQ(header.flags(), 0b111);
    EXPECT_EQ(header.fragment_offset(), 0x1FFF);
}

TEST(IPv4HeaderTest, HeaderLengthScalesWithIhl) {
    const uint8_t wire[20] = {
        0x4F, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00,
        0x40, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    const IPv4Header header = Parse(wire);

    EXPECT_EQ(header.version(), 4);
    EXPECT_EQ(header.IHL(), 15);
    EXPECT_EQ(header.header_length(), 60);
}

TEST(IPv4HeaderTest, VersionAndIhlAreIndependent) {
    const uint8_t wire[20] = {
        0x95, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00,
        0x40, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    const IPv4Header header = Parse(wire);

    EXPECT_EQ(header.version(), 9);
    EXPECT_EQ(header.IHL(), 5);
}

TEST(IPv4HeaderTest, SizeIsTwentyBytes) {
    EXPECT_EQ(sizeof(IPv4Header), 20);
}

TEST(IPv4HeaderTest, StreamOperatorPrintsReadableHeader) {
    const uint8_t wire[20] = {
        0x45, 0x00, 0x00, 0x3C, 0x1C, 0x46, 0x40, 0x00,
        0x40, 0x06, 0xB1, 0xE6, 0xAC, 0xD9, 0x17, 0x0A,
        0xC0, 0xA8, 0x01, 0x03,
    };

    const IPv4Header header = Parse(wire);

    std::ostringstream out;
    out << header;

    EXPECT_EQ(out.str(),
              "IPv4 Header {\n"
              "  Version:            4\n"
              "  IHL:                5 (20 bytes)\n"
              "  Type of Service:    0x00\n"
              "  Total Length:       60\n"
              "  Identification:     0x1c46\n"
              "  Flags:              0b010\n"
              "  Fragment Offset:    0\n"
              "  TTL:                64\n"
              "  Protocol:           6\n"
              "  Checksum:           0xb1e6\n"
              "  Source:             172.217.23.10\n"
              "  Destination:        192.168.1.3\n"
              "}");
}