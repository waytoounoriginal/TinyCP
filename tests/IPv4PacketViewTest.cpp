#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <span>
#include <vector>

#include "IPv4Packet.h"
#include "IPv4PacketView.h"

namespace {

/** The canonical test header: 45 00 00 1C 1C 46 40 00 40 06 B1 E6
 *  AC D9 17 0A C0 A8 01 03 plus an 8-byte payload (28 bytes total). */
std::vector<uint8_t> CanonicalPacket() {
    return {
        0x45, 0x00, 0x00, 0x1C, 0x1C, 0x46, 0x40, 0x00,
        0x40, 0x06, 0xB1, 0xE6, 0xAC, 0xD9, 0x17, 0x0A,
        0xC0, 0xA8, 0x01, 0x03, 0xDE, 0xAD, 0xBE, 0xEF,
        0x01, 0x02, 0x03, 0x04,
    };
}

IPv4Header ParseConcrete(const std::vector<uint8_t>& bytes) {
    IPv4Header header;
    std::memcpy(&header, bytes.data(), sizeof(header));
    return header;
}

} // namespace

TEST(IPv4PacketViewTest, ParsesCanonicalPacket) {
    const std::vector<uint8_t> wire = CanonicalPacket();
    const auto view = IPv4PacketView::Parse(std::span<const uint8_t>(wire));

    ASSERT_TRUE(view.has_value());
    EXPECT_TRUE(view->valid());

    EXPECT_EQ(view->version(), 4);
    EXPECT_EQ(view->IHL(), 5);
    EXPECT_EQ(view->header_length(), 20);
    EXPECT_EQ(view->type_of_service(), 0x00);
    EXPECT_EQ(view->total_length_ntoh(), 28);
    EXPECT_EQ(view->identification_ntoh(), 0x1C46);
    EXPECT_EQ(view->flags(), 0b010);
    EXPECT_EQ(view->fragment_offset(), 0);
    EXPECT_EQ(view->ttl(), 64);
    EXPECT_EQ(view->protocol(), 6);
    EXPECT_EQ(view->checksum_ntoh(), 0xB1E6);
    EXPECT_EQ(view->source_address_ntoh(), 0xACD9170A);
    EXPECT_EQ(view->destination_address_ntoh(), 0xC0A80103);
}

TEST(IPv4PacketViewTest, MatchesConcreteHeader) {
    const std::vector<uint8_t> wire = CanonicalPacket();
    const IPv4Header concrete = ParseConcrete(wire);
    const auto view = IPv4PacketView::Parse(std::span<const uint8_t>(wire));
    ASSERT_TRUE(view.has_value());

    EXPECT_EQ(view->version_ihl(), concrete.version_ihl());
    EXPECT_EQ(view->type_of_service(), concrete.type_of_service());
    EXPECT_EQ(view->total_length(), concrete.total_length());
    EXPECT_EQ(view->identification(), concrete.identification());
    EXPECT_EQ(view->flags_fragment_offset(),
              concrete.flags_fragment_offset());
    EXPECT_EQ(view->ttl(), concrete.ttl());
    EXPECT_EQ(view->protocol(), concrete.protocol());
    EXPECT_EQ(view->checksum(), concrete.checksum());
    EXPECT_EQ(view->source_address(), concrete.source_address());
    EXPECT_EQ(view->destination_address(), concrete.destination_address());

    EXPECT_EQ(view->version(), concrete.version());
    EXPECT_EQ(view->IHL(), concrete.IHL());
    EXPECT_EQ(view->header_length(), concrete.header_length());
    EXPECT_EQ(view->total_length_ntoh(), concrete.total_length_ntoh());
    EXPECT_EQ(view->identification_ntoh(), concrete.identification_ntoh());
    EXPECT_EQ(view->flags(), concrete.flags());
    EXPECT_EQ(view->fragment_offset(), concrete.fragment_offset());
    EXPECT_EQ(view->flags_fragment_offset_ntoh(),
              concrete.flags_fragment_offset_ntoh());
    EXPECT_EQ(view->checksum_ntoh(), concrete.checksum_ntoh());
    EXPECT_EQ(view->source_address_ntoh(), concrete.source_address_ntoh());
    EXPECT_EQ(view->destination_address_ntoh(),
              concrete.destination_address_ntoh());
}

TEST(IPv4PacketViewTest, StreamsIdenticalToConcrete) {
    const std::vector<uint8_t> wire = CanonicalPacket();
    const IPv4Header concrete = ParseConcrete(wire);
    const auto view = IPv4PacketView::Parse(std::span<const uint8_t>(wire));
    ASSERT_TRUE(view.has_value());

    std::ostringstream concrete_out;
    concrete_out << concrete;

    std::ostringstream view_out;
    view_out << *view;

    EXPECT_EQ(view_out.str(), concrete_out.str());
}

TEST(IPv4PacketViewTest, SlicesPayloadAfterHeader) {
    const std::vector<uint8_t> wire = CanonicalPacket();
    const auto view = IPv4PacketView::Parse(std::span<const uint8_t>(wire));
    ASSERT_TRUE(view.has_value());

    EXPECT_EQ(view->payload().size(), 8);
    EXPECT_EQ(view->payload()[0], 0xDE);
    EXPECT_EQ(view->payload()[7], 0x04);
}

TEST(IPv4PacketViewTest, RejectsTruncatedBuffer) {
    std::vector<uint8_t> wire = CanonicalPacket();
    wire.resize(19);
    EXPECT_FALSE(IPv4PacketView(std::span<const uint8_t>(wire)).valid());
    EXPECT_FALSE(IPv4PacketView::Parse(std::span<const uint8_t>(wire)));
}

TEST(IPv4PacketViewTest, RejectsWrongVersion) {
    std::vector<uint8_t> wire = CanonicalPacket();
    wire[0] = 0x65; // version 6
    const auto view = IPv4PacketView::Parse(std::span<const uint8_t>(wire));
    EXPECT_FALSE(view.has_value());
}

TEST(IPv4PacketViewTest, RejectsTotalLengthLargerThanBuffer) {
    std::vector<uint8_t> wire = CanonicalPacket();
    wire[3] = 0xFF; // total length 0x1CFF, larger than the buffer
    const auto view = IPv4PacketView::Parse(std::span<const uint8_t>(wire));
    EXPECT_FALSE(view.has_value());
}

TEST(IPv4PacketViewTest, RejectsHeaderLengthLargerThanTotalLength) {
    std::vector<uint8_t> wire = CanonicalPacket();
    wire[0] = 0x4F; // IHL 15 -> 60-byte header, total length only 28
    const auto view = IPv4PacketView::Parse(std::span<const uint8_t>(wire));
    EXPECT_FALSE(view.has_value());
}