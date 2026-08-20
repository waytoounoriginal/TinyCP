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

    IPv4Address addr_a = IPv4Address::from_string("10.0.0.2:8080");
    IPv4Address addr_b = IPv4Address::from_string("10.0.0.3:9090");

    // 1. Create and bind Socket A and Socket B
    TcpSocket socket_a{stack};
    socket_a.bind(addr_a);

    TcpSocket socket_b{stack};
    socket_b.bind(addr_b);

    stack.register_connection(addr_a, addr_b, socket_a.socket_id());
    stack.register_connection(addr_b, addr_a, socket_b.socket_id());

    auto* tcb_a = stack.get_tcb(socket_a.socket_id());
    auto* tcb_b = stack.get_tcb(socket_b.socket_id());
    ASSERT_NE(tcb_a, nullptr);
    ASSERT_NE(tcb_b, nullptr);

    tcb_a->set_state(ESTABLISHED);
    tcb_b->set_state(ESTABLISHED);

    // 3. Prepare test payload and write to Socket A's send buffer
    const uint8_t message[] = "Hello from Socket A!";
    size_t bytes_sent = socket_a.send({message, sizeof(message)}, 3);
    EXPECT_EQ(bytes_sent, sizeof(message));

    // 6. Socket B reads payload from its recv buffer
    uint8_t read_buf[128] = {};
    size_t bytes_read = socket_b.recv({read_buf, sizeof(read_buf)});

    EXPECT_EQ(bytes_read, sizeof(message));
    EXPECT_STREQ(reinterpret_cast<char*>(read_buf), "Hello from Socket A!");
}

TEST(TcpSocketTest, ThreeWayHandshakeStateTransitions) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address server_addr = IPv4Address::from_string("10.0.0.3:9090");

    TcpSocket server_socket{stack};
    server_socket.bind(server_addr);
    server_socket.listen();

    EXPECT_EQ(server_socket.state(), TcpState::LISTEN);

    TcpSocket client_socket{stack};

    std::thread server_thread([&]() {
        TcpSocket accepted_socket = server_socket.accept();
        EXPECT_EQ(accepted_socket.state(), TcpState::ESTABLISHED);
    });

    std::thread client_thread([&]() {
        client_socket.connect(server_addr);
        EXPECT_EQ(client_socket.state(), TcpState::ESTABLISHED);
    });

    client_thread.join();
    server_thread.join();
}

TEST(TcpSocketTest, FullLifecycleConnectSendRecvClose) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address server_addr = IPv4Address::from_string("10.0.0.3:9090");

    TcpSocket server_socket{stack};
    server_socket.bind(server_addr);
    server_socket.listen();

    EXPECT_EQ(server_socket.state(), TcpState::LISTEN);

    TcpSocket client_socket{stack};

    const uint8_t client_msg[] = "Hello from client!";
    const uint8_t server_msg[] = "Hello from server!";

    std::thread server_thread([&]() {
        // 1. Accept incoming connection
        TcpSocket accepted_socket = server_socket.accept();
        EXPECT_EQ(accepted_socket.state(), TcpState::ESTABLISHED);

        // 2. Receive payload from client
        uint8_t server_recv_buf[128] = {};
        size_t bytes_read = accepted_socket.recv({server_recv_buf, sizeof(server_recv_buf)});
        EXPECT_EQ(bytes_read, sizeof(client_msg));
        EXPECT_STREQ(reinterpret_cast<char*>(server_recv_buf), reinterpret_cast<const char*>(client_msg));

        // 3. Send response back to client
        size_t bytes_sent = accepted_socket.send({server_msg, sizeof(server_msg)});
        EXPECT_EQ(bytes_sent, sizeof(server_msg));

        // 5. Close accepted socket
        accepted_socket.close();
    });

    std::thread client_thread([&]() {
        // 1. Connect to server
        client_socket.connect(server_addr);
        EXPECT_EQ(client_socket.state(), TcpState::ESTABLISHED);

        // 2. Send payload to server
        size_t bytes_sent = client_socket.send({client_msg, sizeof(client_msg)});
        EXPECT_EQ(bytes_sent, sizeof(client_msg));

        // 3. Receive response from server
        uint8_t client_recv_buf[128] = {};
        size_t bytes_read = client_socket.recv({client_recv_buf, sizeof(client_recv_buf)});
        EXPECT_EQ(bytes_read, sizeof(server_msg));
        EXPECT_STREQ(reinterpret_cast<char*>(client_recv_buf), reinterpret_cast<const char*>(server_msg));

        // 4. Close client connection
        client_socket.close();
    });

    client_thread.join();
    server_thread.join();
}

TEST(TcpSocketTest, RetransmissionExhaustionOnUnresponsivePeer) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address client_addr = IPv4Address::from_string("10.0.0.2:8080");
    IPv4Address dead_peer_addr = IPv4Address::from_string("10.0.0.99:9999");

    TcpSocket socket{stack};
    socket.bind(client_addr);
    stack.register_connection(client_addr, dead_peer_addr, socket.socket_id());

    auto* tcb = stack.get_tcb(socket.socket_id());
    ASSERT_NE(tcb, nullptr);
    tcb->set_state(ESTABLISHED);

    // Short RTO for fast test execution (20ms per attempt)
    tcb->RTO = std::chrono::milliseconds(20);

    std::atomic<int> send_attempts{0};
    stack.set_outbound_interceptor([&](IPv4Address src, IPv4Address dst, const TcpPacket& packet) {
        if (packet.payload.size() > 0) {
            send_attempts++;
        }
    });

    const uint8_t message[] = "Unacknowledged payload";
    size_t result = socket.send({message, sizeof(message)});
    EXPECT_EQ(result, sizeof(message));

    // Wait for 3 RTO cycles (initial + retransmissions)
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    EXPECT_GE(send_attempts.load(), 3);
}

TEST(TcpSocketTest, RetransmissionSuccessAfterTransientDrop) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address addr_a = IPv4Address::from_string("10.0.0.2:8080");
    IPv4Address addr_b = IPv4Address::from_string("10.0.0.3:9090");

    TcpSocket socket_a{stack};
    socket_a.bind(addr_a);

    TcpSocket socket_b{stack};
    socket_b.bind(addr_b);

    // Register bidirectional routes
    stack.register_connection(addr_a, addr_b, socket_a.socket_id());
    stack.register_connection(addr_b, addr_a, socket_b.socket_id());

    auto* tcb_a = stack.get_tcb(socket_a.socket_id());
    auto* tcb_b = stack.get_tcb(socket_b.socket_id());
    ASSERT_NE(tcb_a, nullptr);
    ASSERT_NE(tcb_b, nullptr);

    tcb_a->set_state(ESTABLISHED);
    tcb_b->set_state(ESTABLISHED);

    // Initialize aligned sequence numbers
    tcb_a->RCV.IRS = 1000;
    tcb_a->RCV.NXT = 1000;
    tcb_a->SND.ISS = 2000;
    tcb_a->SND.UNA = 2000;
    tcb_a->SND.NXT = 2000;

    tcb_b->RCV.IRS = 2000;
    tcb_b->RCV.NXT = 2000;
    tcb_b->SND.ISS = 1000;
    tcb_b->SND.UNA = 1000;
    tcb_b->SND.NXT = 1000;

    // Fast RTO (30ms) for snappy test execution
    tcb_a->RTO = std::chrono::milliseconds(30);
    tcb_b->RTO = std::chrono::milliseconds(30);

    const uint8_t message[] = "Retransmitted successfully!";
    std::atomic<int> transmission_count{0};

    // Drop only the 1st transmission attempt of the payload on the wire
    stack.set_packet_drop_predicate([&](IPv4Address src, IPv4Address dst, const TcpPacket& packet) {
        if (packet.payload.size() > 0) {
            int count = ++transmission_count;
            if (count == 1) {
                // Drop attempt 1
                return true;
            }
        }
        return false;
    });

    size_t bytes_sent = socket_a.send({message, sizeof(message)});
    EXPECT_EQ(bytes_sent, sizeof(message));

    // Socket B receives the payload once background retransmission succeeds
    uint8_t read_buf[128] = {};
    size_t bytes_read = socket_b.recv({read_buf, sizeof(read_buf)});
    EXPECT_EQ(bytes_read, sizeof(message));
    EXPECT_STREQ(reinterpret_cast<char*>(read_buf), reinterpret_cast<const char*>(message));
    EXPECT_GE(transmission_count.load(), 2);
}

} // namespace
