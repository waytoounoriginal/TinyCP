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

/** Helper function to build a raw IPv4+TCP packet for packet injection */
std::vector<uint8_t> BuildRawTcpPacket(IPv4Address src, IPv4Address dst,
                                        uint32_t seq, uint32_t ack,
                                        bool syn, bool ack_flag, bool fin, bool rst) {
    TcpHeader tcp_hdr{};
    tcp_hdr.set_source_port(src.port);
    tcp_hdr.set_dest_port(dst.port);
    tcp_hdr.set_data_offset(5);
    tcp_hdr.set_seq_num(seq);
    tcp_hdr.set_ack_num(ack);
    tcp_hdr.set_syn(syn);
    tcp_hdr.set_ack(ack_flag);
    tcp_hdr.set_fin(fin);
    tcp_hdr.set_rst(rst);
    tcp_hdr.set_window(65535);

    TcpPacket tcp_pkt{ tcp_hdr, {} };
    uint8_t tcp_buf[MAX_IPV4_PACKET_SIZE];
    auto tcp_len = tcp_pkt.write({tcp_buf, sizeof(tcp_buf)}, src, dst);

    IPv4Header ip_hdr{};
    ip_hdr.set_version(4);
    ip_hdr.set_ihl(5);
    ip_hdr.set_ttl(64);
    ip_hdr.set_source_address(src.address);
    ip_hdr.set_destination_address(dst.address);
    ip_hdr.set_protocol(IPPROTO_TCP);

    IPv4Packet ip_pkt{ ip_hdr, {tcp_buf, tcp_len} };
    uint8_t ip_buf[MAX_IPV4_PACKET_SIZE];
    auto ip_len = ip_pkt.write({ip_buf, sizeof(ip_buf)});

    return std::vector<uint8_t>(ip_buf, ip_buf + ip_len);
}

// -----------------------------------------------------------------------------
// RFC 793 Figure 13: Normal Close Sequence
// -----------------------------------------------------------------------------
TEST(TcpTeardownTest, Figure13_NormalCloseSequence) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address addr_a = IPv4Address::from_string("10.0.0.2:8080"); // TCP A
    IPv4Address addr_b = IPv4Address::from_string("10.0.0.3:9090"); // TCP B

    // 1. ESTABLISHED
    TcpSocket socket_a{stack};
    socket_a.bind(addr_a);
    uint64_t id_a = stack.register_connection(addr_a, addr_b, socket_a.socket_id());
    auto tcb_a = stack.get_tcb(id_a);
    ASSERT_NE(tcb_a, nullptr);
    tcb_a->set_state(TcpState::ESTABLISHED);
    tcb_a->SND.NXT = 100;
    tcb_a->RCV.NXT = 300;

    TcpSocket socket_b{stack};
    socket_b.bind(addr_b);
    uint64_t id_b = stack.register_connection(addr_b, addr_a, socket_b.socket_id());
    auto tcb_b = stack.get_tcb(id_b);
    ASSERT_NE(tcb_b, nullptr);
    tcb_b->set_state(TcpState::ESTABLISHED);
    tcb_b->SND.NXT = 300;
    tcb_b->RCV.NXT = 100;

    // 2. TCP A initiates Close: FIN-WAIT-1 --> <SEQ=100><ACK=300><CTL=FIN,ACK> --> CLOSE-WAIT
    socket_a.close();
    EXPECT_EQ(socket_a.state(), TcpState::FIN_WAIT_1);

    // Inject TCP A's FIN into TCP B
    auto packet_2 = BuildRawTcpPacket(addr_a, addr_b, /*seq=*/100, /*ack=*/300, /*syn=*/false, /*ack=*/true, /*fin=*/true, /*rst=*/false);
    stack.inject_packet(packet_2);
    EXPECT_EQ(socket_b.state(), TcpState::CLOSE_WAIT);

    // 3. TCP B sends ACK: FIN-WAIT-2 <-- <SEQ=300><ACK=101><CTL=ACK> <-- CLOSE-WAIT
    auto packet_3 = BuildRawTcpPacket(addr_b, addr_a, /*seq=*/300, /*ack=*/101, /*syn=*/false, /*ack=*/true, /*fin=*/false, /*rst=*/false);
    stack.inject_packet(packet_3);
    EXPECT_EQ(socket_a.state(), TcpState::FIN_WAIT_2);

    // 4. TCP B initiates Close: CLOSED <-- <SEQ=300><ACK=101><CTL=FIN,ACK> <-- LAST-ACK
    socket_b.close();
    EXPECT_EQ(socket_b.state(), TcpState::LAST_ACK);

    // Inject TCP B's FIN into TCP A
    auto packet_4 = BuildRawTcpPacket(addr_b, addr_a, /*seq=*/300, /*ack=*/101, /*syn=*/false, /*ack=*/true, /*fin=*/true, /*rst=*/false);
    stack.inject_packet(packet_4);

    // 5. TCP A transitions to CLOSED
    EXPECT_EQ(socket_a.state(), TcpState::CLOSED);

    // 6. TCP B receives final ACK: <SEQ=101><ACK=301><CTL=ACK> --> CLOSED
    auto packet_5 = BuildRawTcpPacket(addr_a, addr_b, /*seq=*/101, /*ack=*/301, /*syn=*/false, /*ack=*/true, /*fin=*/false, /*rst=*/false);
    stack.inject_packet(packet_5);
    EXPECT_EQ(socket_b.state(), TcpState::CLOSED);
}

// -----------------------------------------------------------------------------
// RFC 793 Figure 14: Simultaneous Close Sequence
// -----------------------------------------------------------------------------
TEST(TcpTeardownTest, Figure14_SimultaneousCloseSequence) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address addr_a = IPv4Address::from_string("10.0.0.2:8080"); // TCP A
    IPv4Address addr_b = IPv4Address::from_string("10.0.0.3:9090"); // TCP B

    // 1. ESTABLISHED
    TcpSocket socket_a{stack};
    socket_a.bind(addr_a);
    uint64_t id_a = stack.register_connection(addr_a, addr_b, socket_a.socket_id());
    auto tcb_a = stack.get_tcb(id_a);
    ASSERT_NE(tcb_a, nullptr);
    tcb_a->set_state(TcpState::ESTABLISHED);
    tcb_a->SND.NXT = 100;
    tcb_a->RCV.NXT = 300;

    TcpSocket socket_b{stack};
    socket_b.bind(addr_b);
    uint64_t id_b = stack.register_connection(addr_b, addr_a, socket_b.socket_id());
    auto tcb_b = stack.get_tcb(id_b);
    ASSERT_NE(tcb_b, nullptr);
    tcb_b->set_state(TcpState::ESTABLISHED);
    tcb_b->SND.NXT = 300;
    tcb_b->RCV.NXT = 100;

    // 2. Both sides initiate Close: FIN-WAIT-1
    socket_a.close();
    EXPECT_EQ(socket_a.state(), TcpState::FIN_WAIT_1);

    socket_b.close();
    EXPECT_EQ(socket_b.state(), TcpState::FIN_WAIT_1);

    // Inject TCP B's FIN into TCP A (<SEQ=300><ACK=100><CTL=FIN,ACK>) -> CLOSING
    auto fin_b = BuildRawTcpPacket(addr_b, addr_a, /*seq=*/300, /*ack=*/100, /*syn=*/false, /*ack=*/true, /*fin=*/true, /*rst=*/false);
    stack.inject_packet(fin_b);
    EXPECT_EQ(socket_a.state(), TcpState::CLOSING);

    // Inject TCP A's FIN into TCP B (<SEQ=100><ACK=300><CTL=FIN,ACK>) -> CLOSING
    auto fin_a = BuildRawTcpPacket(addr_a, addr_b, /*seq=*/100, /*ack=*/300, /*syn=*/false, /*ack=*/true, /*fin=*/true, /*rst=*/false);
    stack.inject_packet(fin_a);
    EXPECT_EQ(socket_b.state(), TcpState::CLOSING);

    // 3. Inject ACK for A's FIN into TCP A (<SEQ=301><ACK=101><CTL=ACK>) -> CLOSED
    auto ack_for_a = BuildRawTcpPacket(addr_b, addr_a, /*seq=*/301, /*ack=*/101, /*syn=*/false, /*ack=*/true, /*fin=*/false, /*rst=*/false);
    stack.inject_packet(ack_for_a);
    EXPECT_EQ(socket_a.state(), TcpState::CLOSED);

    // Inject ACK for B's FIN into TCP B (<SEQ=101><ACK=301><CTL=ACK>) -> CLOSED
    auto ack_for_b = BuildRawTcpPacket(addr_a, addr_b, /*seq=*/101, /*ack=*/301, /*syn=*/false, /*ack=*/true, /*fin=*/false, /*rst=*/false);
    stack.inject_packet(ack_for_b);
    EXPECT_EQ(socket_b.state(), TcpState::CLOSED);
}

} // namespace
