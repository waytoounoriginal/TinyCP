#include <gtest/gtest.h>
#include "IPv4.h"

namespace {

TEST(IPv4AddressTest, ParsesValidIpAndPortString) {
    auto addr = IPv4Address::from_string("10.0.0.2:8080");
    EXPECT_EQ(addr.address, 0x0A000002u);
    EXPECT_EQ(addr.port, 8080u);
}

TEST(IPv4AddressTest, ParsesValidIpWithoutPortString) {
    auto addr = IPv4Address::from_string("10.0.0.2");
    EXPECT_EQ(addr.address, 0x0A000002u);
    EXPECT_EQ(addr.port, 0u);
}

TEST(IPv4AddressTest, ParsesStandardClassCIP) {
    auto addr = IPv4Address::from_string("192.168.1.1:80");
    EXPECT_EQ(addr.address, 0xC0A80101u);
    EXPECT_EQ(addr.port, 80u);
}

TEST(IPv4AddressTest, ReturnsZeroForInvalidStrings) {
    auto invalid1 = IPv4Address::from_string("invalid");
    EXPECT_EQ(invalid1.address, 0u);
    EXPECT_EQ(invalid1.port, 0u);

    auto invalid2 = IPv4Address::from_string("");
    EXPECT_EQ(invalid2.address, 0u);
    EXPECT_EQ(invalid2.port, 0u);

    auto invalid3 = IPv4Address::from_string("10.0.0");
    EXPECT_EQ(invalid3.address, 0u);
    EXPECT_EQ(invalid3.port, 0u);
}

TEST(IPv4AddressTest, EqualityComparison) {
    auto addr1 = IPv4Address::from_string("10.0.0.2:8080");
    auto addr2 = IPv4Address::from_string("10.0.0.2:8080");
    auto addr3 = IPv4Address::from_string("10.0.0.2:9090");

    EXPECT_EQ(addr1, addr2);
    EXPECT_NE(addr1, addr3);
}

} // namespace
