#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <span>
#include <vector>

#include "TcpPacket.h"
#include "TcpPacketView.h"

namespace {

/** A canonical SYN segment: ports 1234 -> 80, seq 0x11223344, window
 *  0x7210, checksum 0xABCD, plus an 8-byte payload. */
std::vector<uint8_t> CanonicalSegment() {
    return {
        0x04, 0xD2, 0x00, 0x50, 0x11, 0x22, 0x33, 0x44,
        0x00, 0x00, 0x00, 0x00, 0x50, 0x02, 0x72, 0x10,
        0xAB, 0xCD, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF,
        0x01, 0x02, 0x03, 0x04,
    };
}

TcpHeader ParseConcrete(const std::vector<uint8_t>& bytes) {
    TcpHeader header;
    std::memcpy(&header, bytes.data(), sizeof(header));
    return header;
}

} // namespace

TEST(TcpPacketViewTest, ParsesCanonicalSegment) {
    const std::vector<uint8_t> wire = CanonicalSegment();
    const auto view = TcpPacketView::Parse(std::span<const uint8_t>(wire));

    ASSERT_TRUE(view.has_value());
    EXPECT_TRUE(view->valid());

    EXPECT_EQ(view->source_port_ntoh(), 1234);
    EXPECT_EQ(view->dest_port_ntoh(), 80);
    EXPECT_EQ(view->seq_num_ntoh(), 0x11223344);
    EXPECT_EQ(view->ack_num_ntoh(), 0x00000000);
    EXPECT_EQ(view->data_offset(), 5);
    EXPECT_EQ(view->header_length(), 20);
    EXPECT_TRUE(view->syn());
    EXPECT_FALSE(view->ack());
    EXPECT_EQ(view->window_ntoh(), 0x7210);
    EXPECT_EQ(view->checksum_ntoh(), 0xABCD);
    EXPECT_EQ(view->urgent_pointer_ntoh(), 0x0000);
}

TEST(TcpPacketViewTest, MatchesConcreteHeader) {
    const std::vector<uint8_t> wire = CanonicalSegment();
    const TcpHeader concrete = ParseConcrete(wire);
    const auto view = TcpPacketView::Parse(std::span<const uint8_t>(wire));
    ASSERT_TRUE(view.has_value());

    EXPECT_EQ(view->source_port(), concrete.source_port());
    EXPECT_EQ(view->dest_port(), concrete.dest_port());
    EXPECT_EQ(view->seq_num(), concrete.seq_num());
    EXPECT_EQ(view->ack_num(), concrete.ack_num());
    EXPECT_EQ(view->data_offset_and_reserved_and_control_bits(),
              concrete.data_offset_and_reserved_and_control_bits());
    EXPECT_EQ(view->window(), concrete.window());
    EXPECT_EQ(view->checksum(), concrete.checksum());
    EXPECT_EQ(view->urgent_pointer(), concrete.urgent_pointer());

    EXPECT_EQ(view->source_port_ntoh(), concrete.source_port_ntoh());
    EXPECT_EQ(view->dest_port_ntoh(), concrete.dest_port_ntoh());
    EXPECT_EQ(view->seq_num_ntoh(), concrete.seq_num_ntoh());
    EXPECT_EQ(view->ack_num_ntoh(), concrete.ack_num_ntoh());
    EXPECT_EQ(view->data_offset_and_reserved_and_control_bits_ntoh(),
              concrete.data_offset_and_reserved_and_control_bits_ntoh());
    EXPECT_EQ(view->data_offset(), concrete.data_offset());
    EXPECT_EQ(view->header_length(), concrete.header_length());
    EXPECT_EQ(view->flags(), concrete.flags());
    EXPECT_EQ(view->urg(), concrete.urg());
    EXPECT_EQ(view->ack(), concrete.ack());
    EXPECT_EQ(view->psh(), concrete.psh());
    EXPECT_EQ(view->rst(), concrete.rst());
    EXPECT_EQ(view->syn(), concrete.syn());
    EXPECT_EQ(view->fin(), concrete.fin());
    EXPECT_EQ(view->window_ntoh(), concrete.window_ntoh());
    EXPECT_EQ(view->checksum_ntoh(), concrete.checksum_ntoh());
    EXPECT_EQ(view->urgent_pointer_ntoh(), concrete.urgent_pointer_ntoh());
}

TEST(TcpPacketViewTest, StreamsIdenticalToConcrete) {
    const std::vector<uint8_t> wire = CanonicalSegment();
    const TcpHeader concrete = ParseConcrete(wire);
    const auto view = TcpPacketView::Parse(std::span<const uint8_t>(wire));
    ASSERT_TRUE(view.has_value());

    std::ostringstream concrete_out;
    concrete_out << concrete;

    std::ostringstream view_out;
    view_out << *view;

    EXPECT_EQ(view_out.str(), concrete_out.str());
}

TEST(TcpPacketViewTest, SlicesPayloadAfterHeader) {
    const std::vector<uint8_t> wire = CanonicalSegment();
    const auto view = TcpPacketView::Parse(std::span<const uint8_t>(wire));
    ASSERT_TRUE(view.has_value());

    EXPECT_EQ(view->payload().size(), 8);
    EXPECT_EQ(view->payload()[0], 0xDE);
    EXPECT_EQ(view->payload()[7], 0x04);
}

TEST(TcpPacketViewTest, SlicesPayloadAfterOptions) {
    // Data Offset 6 -> 24-byte header: the first 4 payload bytes become
    // option bytes (0xDE AD BE EF occupies the options area).
    std::vector<uint8_t> wire = CanonicalSegment();
    wire[12] = 0x60;

    const auto view = TcpPacketView::Parse(std::span<const uint8_t>(wire));
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->header_length(), 24);
    EXPECT_EQ(view->payload().size(), 4);
    EXPECT_EQ(view->payload()[0], 0x01);
    EXPECT_EQ(view->payload()[3], 0x04);
}

TEST(TcpPacketViewTest, RejectsTruncatedBuffer) {
    std::vector<uint8_t> wire = CanonicalSegment();
    wire.resize(19);
    EXPECT_FALSE(TcpPacketView(std::span<const uint8_t>(wire)).valid());
    EXPECT_FALSE(TcpPacketView::Parse(std::span<const uint8_t>(wire)));
}

TEST(TcpPacketViewTest, RejectsDataOffsetBeyondBuffer) {
    std::vector<uint8_t> wire = CanonicalSegment();
    wire[12] = 0xF0; // Data Offset 15 -> 60-byte header, buffer is 28
    const auto view = TcpPacketView::Parse(std::span<const uint8_t>(wire));
    EXPECT_FALSE(view.has_value());
}