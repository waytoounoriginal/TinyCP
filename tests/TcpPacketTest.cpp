#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <sstream>

#include "TcpPacket.h"

namespace {

/** Copies raw wire bytes into a TcpHeader, as a packet parser would. */
TcpHeader Parse(const uint8_t* wire) {
    TcpHeader header;
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
 * A canonical TCP SYN segment from port 1234 (0x04D2) to port 80 (0x0050)
 * with sequence number 0x11223344, window 0x7210 and checksum 0xABCD.
 */
TEST(TcpHeaderTest, ParsesCanonicalSynHeader) {
    const uint8_t wire[20] = {
        0x04, 0xD2, 0x00, 0x50, 0x11, 0x22, 0x33, 0x44,
        0x00, 0x00, 0x00, 0x00, 0x50, 0x02, 0x72, 0x10,
        0xAB, 0xCD, 0x00, 0x00,
    };

    const TcpHeader header = Parse(wire);

    EXPECT_EQ(header.source_port_ntoh(), 1234);
    EXPECT_EQ(header.dest_port_ntoh(), 80);

    EXPECT_EQ(header.seq_num_ntoh(), 0x11223344);
    EXPECT_EQ(header.ack_num_ntoh(), 0x00000000);

    EXPECT_EQ(header.data_offset(), 5);
    EXPECT_EQ(header.header_length(), 20);

    EXPECT_TRUE(header.syn());
    EXPECT_FALSE(header.ack());
    EXPECT_FALSE(header.fin());
    EXPECT_FALSE(header.rst());
    EXPECT_FALSE(header.psh());
    EXPECT_FALSE(header.urg());

    EXPECT_EQ(header.window_ntoh(), 0x7210);
    EXPECT_EQ(header.checksum_ntoh(), 0xABCD);
    EXPECT_EQ(header.urgent_pointer_ntoh(), 0x0000);
}

TEST(TcpHeaderTest, ParsesAckPshFinHeader) {
    const uint8_t wire[20] = {
        0x04, 0xD2, 0x00, 0x50, 0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88, 0x50, 0x19, 0x10, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    const TcpHeader header = Parse(wire);

    EXPECT_EQ(header.ack_num_ntoh(), 0x55667788);

    EXPECT_EQ(header.data_offset(), 5);

    EXPECT_TRUE(header.ack());
    EXPECT_TRUE(header.psh());
    EXPECT_TRUE(header.fin());
    EXPECT_FALSE(header.syn());
    EXPECT_FALSE(header.rst());

    EXPECT_EQ(header.flags(), 0b011001);
}

TEST(TcpHeaderTest, ParsesAllSixControlBits) {
    const uint8_t wire[20] = {
        0x04, 0xD2, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x50, 0x3F, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    const TcpHeader header = Parse(wire);

    EXPECT_EQ(header.flags(), 0x3F);
    EXPECT_TRUE(header.urg());
    EXPECT_TRUE(header.ack());
    EXPECT_TRUE(header.psh());
    EXPECT_TRUE(header.rst());
    EXPECT_TRUE(header.syn());
    EXPECT_TRUE(header.fin());
}

TEST(TcpHeaderTest, HeaderLengthScalesWithDataOffset) {
    const uint8_t wire[20] = {
        0x04, 0xD2, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xF0, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    const TcpHeader header = Parse(wire);

    EXPECT_EQ(header.data_offset(), 15);
    EXPECT_EQ(header.header_length(), 60);
}

TEST(TcpHeaderTest, ReservedBitsDoNotAffectGetters) {
    const uint8_t wire[20] = {
        0x04, 0xD2, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x5E, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    const TcpHeader header = Parse(wire);

    EXPECT_EQ(header.data_offset(), 5);
    EXPECT_EQ(header.flags(), 0x02);
    EXPECT_TRUE(header.syn());
    EXPECT_FALSE(header.ack());
}

TEST(TcpHeaderTest, RawGettersPreserveWireBytes) {
    const uint8_t wire[20] = {
        0x12, 0x34, 0x56, 0x78, 0xAB, 0xCD, 0xEF, 0x01,
        0x23, 0x45, 0x67, 0x89, 0x5A, 0x1B, 0xFE, 0xDC,
        0xBA, 0x98, 0x76, 0x54,
    };

    const TcpHeader header = Parse(wire);

    EXPECT_EQ(header.source_port(), Wire16(wire));
    EXPECT_EQ(header.dest_port(), Wire16(wire + 2));
    EXPECT_EQ(header.seq_num(), Wire32(wire + 4));
    EXPECT_EQ(header.ack_num(), Wire32(wire + 8));
    EXPECT_EQ(header.data_offset_and_reserved_and_control_bits(),
              Wire16(wire + 12));
    EXPECT_EQ(header.window(), Wire16(wire + 14));
    EXPECT_EQ(header.checksum(), Wire16(wire + 16));
    EXPECT_EQ(header.urgent_pointer(), Wire16(wire + 18));
}

TEST(TcpHeaderTest, NtohGettersConvertToHostOrder) {
    const uint8_t wire[20] = {
        0x12, 0x34, 0x56, 0x78, 0xAB, 0xCD, 0xEF, 0x01,
        0x23, 0x45, 0x67, 0x89, 0x5A, 0x1B, 0xFE, 0xDC,
        0xBA, 0x98, 0x76, 0x54,
    };

    const TcpHeader header = Parse(wire);

    EXPECT_EQ(header.source_port_ntoh(), 0x1234);
    EXPECT_EQ(header.dest_port_ntoh(), 0x5678);
    EXPECT_EQ(header.seq_num_ntoh(), 0xABCDEF01);
    EXPECT_EQ(header.ack_num_ntoh(), 0x23456789);
    EXPECT_EQ(header.data_offset_and_reserved_and_control_bits_ntoh(),
              0x5A1B);
    EXPECT_EQ(header.window_ntoh(), 0xFEDC);
    EXPECT_EQ(header.checksum_ntoh(), 0xBA98);
    EXPECT_EQ(header.urgent_pointer_ntoh(), 0x7654);
}

TEST(TcpHeaderTest, SizeIsTwentyBytes) {
    EXPECT_EQ(sizeof(TcpHeader), 20);
}

TEST(TcpHeaderTest, StreamOperatorPrintsReadableHeader) {
    const uint8_t wire[20] = {
        0x04, 0xD2, 0x00, 0x50, 0x11, 0x22, 0x33, 0x44,
        0x00, 0x00, 0x00, 0x00, 0x50, 0x02, 0x72, 0x10,
        0xAB, 0xCD, 0x00, 0x00,
    };

    const TcpHeader header = Parse(wire);

    std::ostringstream out;
    out << header;

    EXPECT_EQ(out.str(),
              "TCP Header {\n"
              "  Source Port:        1234\n"
              "  Destination Port:   80\n"
              "  Sequence Number:    0x11223344\n"
              "  Ack Number:         0x00000000\n"
              "  Data Offset:        5 (20 bytes)\n"
              "  Flags:              SYN\n"
              "  Window:             0x7210\n"
              "  Checksum:           0xabcd\n"
              "  Urgent Pointer:     0x0000\n"
              "}");
}

TEST(TcpHeaderTest, StreamOperatorPrintsMultipleFlags) {
    const uint8_t wire[20] = {
        0x04, 0xD2, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x50, 0x19, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    const TcpHeader header = Parse(wire);

    std::ostringstream out;
    out << header;

    EXPECT_NE(out.str().find("Flags:              ACK PSH FIN"),
              std::string::npos);
}

/** The payload span must start at the end of the header (Data Offset). */
TEST(TcpPacketTest, PayloadStartsAtEndOfHeader) {
    const uint8_t wire[20] = {
        0x04, 0xD2, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x50, 0x18, 0x10, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    uint8_t buf[26] = {};
    std::memcpy(buf, wire, sizeof(wire));
    const uint8_t payload[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x42};
    std::memcpy(buf + sizeof(wire), payload, sizeof(payload));

    TcpHeader& header = *reinterpret_cast<TcpHeader*>(buf);
    TcpPacket packet{
        header,
        std::span<const uint8_t>(buf + header.header_length(),
                                 sizeof(buf) - header.header_length()),
    };

    EXPECT_EQ(packet.payload.size(), 6);
    EXPECT_EQ(packet.payload[0], 0xDE);
    EXPECT_EQ(packet.payload[5], 0x42);
}

TEST(TcpPacketTest, ComputesFullPseudoHeaderChecksum) {
    // 10.0.0.1 (0x0A000001) -> 10.0.0.2 (0x0A000002) in network order
    const uint32_t src_ip = htonl(0x0A000001);
    const uint32_t dst_ip = htonl(0x0A000002);

    TcpHeader header{};
    header.set_source_port(1234);
    header.set_dest_port(80);
    header.set_seq_num(100);
    header.set_ack_num(0);
    header.set_data_offset(5);
    header.set_syn(true);

    const uint8_t payload[4] = {0x11, 0x22, 0x33, 0x44};
    TcpPacket packet{header, std::span<const uint8_t>(payload, 4)};

    uint8_t out[128] = {};
    size_t written = packet.write(out, src_ip, dst_ip);
    EXPECT_EQ(written, 24);

    // Verify written TCP checksum at offset 16-17 is non-zero and valid
    uint16_t written_chk = static_cast<uint16_t>((out[16] << 8) | out[17]);
    EXPECT_NE(written_chk, 0);

    // Verifying checksum over (pseudo-header + header + payload) with written checksum yields 0
    uint16_t recomputed = ComputeTcpChecksum(src_ip, dst_ip, out, 20, out + 20, 4);
    EXPECT_EQ(recomputed, 0);
}