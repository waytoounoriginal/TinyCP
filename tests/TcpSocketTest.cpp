#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <span>

#include "IPv4.h"
#include "IPv4Packet.h"
#include "TcpPacket.h"
#include "TcpSocket.h"
#include "TcpStack.h"

namespace {

TEST(TcpSocketTest, TwoSocketsCommunicatingViaInFlightPackets) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address addr_a{ 0x0A000002 /* 10.0.0.2 */, 8080 };
    IPv4Address addr_b{ 0x0A000003 /* 10.0.0.3 */, 9090 };

    // 1. Create and bind Socket A and Socket B
    TcpSocket socket_a{stack};
    socket_a.bind(addr_a);

    TcpSocket socket_b{stack};
    socket_b.bind(addr_b);

    stack.register_connection(addr_a, addr_b, socket_a.tcb());
    stack.register_connection(addr_b, addr_a, socket_b.tcb());

    // 3. Prepare test payload and write to Socket A's send buffer
    const uint8_t message[] = "Hello from Socket A!";
    size_t bytes_sent = socket_a.send({message, sizeof(message)});
    EXPECT_EQ(bytes_sent, sizeof(message));


    // 6. Socket B reads payload from its recv buffer
    uint8_t read_buf[128] = {};
    size_t bytes_read = socket_b.recv({read_buf, sizeof(read_buf)});

    EXPECT_EQ(bytes_read, sizeof(message));
    EXPECT_STREQ(reinterpret_cast<char*>(read_buf), "Hello from Socket A!");
}

} // namespace
