#define TCP_STACK_TESTING 1

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include <span>

#include "IPv4.h"
#include "IPv4Packet.h"
#include "TcpPacket.h"
#include "TcpSocket.h"
#include "TcpStack.h"

namespace {

TEST(Rfc793ScenariosTest, Figure8_SimultaneousOpen) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address addr_a = IPv4Address::from_string("10.0.0.2:8080");
    IPv4Address addr_b = IPv4Address::from_string("10.0.0.3:9090");

    TcpSocket socket_a{stack};
    socket_a.bind(addr_a);

    TcpSocket socket_b{stack};
    socket_b.bind(addr_b);

    // Both sockets initiate active open (SYN_SENT)
    std::thread thread_a([&]() {
        socket_a.connect(addr_b);
    });

    std::thread thread_b([&]() {
        socket_b.connect(addr_a);
    });

    thread_a.join();
    thread_b.join();

    EXPECT_EQ(socket_a.state(), TcpState::ESTABLISHED);
    EXPECT_EQ(socket_b.state(), TcpState::ESTABLISHED);
}

TEST(Rfc793ScenariosTest, Figure9_OldDuplicateSynRecovery) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address server_addr = IPv4Address::from_string("10.0.0.3:9090");
    IPv4Address client_addr = IPv4Address::from_string("10.0.0.2:49152");

    TcpSocket server_socket{stack};
    server_socket.bind(server_addr);
    server_socket.listen();

    std::vector<TcpPacket> captured_packets;
    stack.set_outbound_interceptor([&](IPv4Address src, IPv4Address dst, const TcpPacket& packet) {
        captured_packets.push_back(packet);
    });

    // Synthesize an old duplicate SYN with SEQ = 90
    TcpHeader syn_header{};
    syn_header.set_source_port(client_addr.port);
    syn_header.set_dest_port(server_addr.port);
    syn_header.set_data_offset(5);
    syn_header.set_syn(1);
    syn_header.set_seq_num(90);

    TcpPacket syn_packet{ syn_header, {} };
    uint8_t tcp_buf[512];
    auto tcp_len = syn_packet.write({tcp_buf, sizeof(tcp_buf)}, client_addr, server_addr);

    IPv4Header ip_header{};
    ip_header.set_version(4);
    ip_header.set_ihl(5);
    ip_header.set_ttl(255);
    ip_header.set_source_address(client_addr.address);
    ip_header.set_destination_address(server_addr.address);
    ip_header.set_protocol(IPPROTO_TCP);

    IPv4Packet ip_packet{ ip_header, {tcp_buf, tcp_len} };
    uint8_t ip_buf[1024];
    auto ip_len = ip_packet.write({ip_buf, sizeof(ip_buf)});

    // Inject old duplicate SYN into server stack
    stack.inject_packet({ip_buf, ip_len});

    // Server B should respond with SYN-ACK (ACK = 91)
    ASSERT_FALSE(captured_packets.empty());
    auto syn_ack = captured_packets.back();
    EXPECT_TRUE(syn_ack.header.syn());
    EXPECT_TRUE(syn_ack.header.ack());
    EXPECT_EQ(syn_ack.header.ack_num_ntoh(), 91u);
}

} // namespace
