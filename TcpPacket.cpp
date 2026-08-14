//
// Created by waytoounoriginal on 8/11/2026.
//

#include "TcpPacket.h"

#include <cstring>

#include "TcpPacketView.h"

#include <iomanip>
#include <ostream>
#include "utils/Platform.h"

/** Shared dump format for TcpHeader and TcpPacketView. */
template <typename Header>
static void PrintTcpHeader(std::ostream& os, const Header& header) {
    const uint16_t flags = header.flags();

    os << "TCP Header {\n"
       << "  Source Port:        " << header.source_port_ntoh() << '\n'
       << "  Destination Port:   " << header.dest_port_ntoh() << '\n'
       << "  Sequence Number:    0x"
       << std::hex
       << std::setw(8)
       << std::setfill('0')
       << header.seq_num_ntoh()
       << std::dec
       << std::setfill(' ')
       << '\n'
       << "  Ack Number:         0x"
       << std::hex
       << std::setw(8)
       << std::setfill('0')
       << header.ack_num_ntoh()
       << std::dec
       << std::setfill(' ')
       << '\n'
       << "  Data Offset:        " << static_cast<int>(header.data_offset())
       << " (" << static_cast<int>(header.header_length()) << " bytes)\n"
       << "  Flags:              ";

    bool printed = false;
    const auto print_flag = [&os, &printed](bool set, const char* name) {
        if (!set) {
            return;
        }
        if (printed) {
            os << ' ';
        }
        os << name;
        printed = true;
    };

    print_flag(header.urg(), "URG");
    print_flag(header.ack(), "ACK");
    print_flag(header.psh(), "PSH");
    print_flag(header.rst(), "RST");
    print_flag(header.syn(), "SYN");
    print_flag(header.fin(), "FIN");

    if (!printed) {
        os << "None";
    }

    os << '\n'
       << "  Window:             0x"
       << std::hex
       << std::setw(4)
       << std::setfill('0')
       << header.window_ntoh()
       << std::dec
       << std::setfill(' ')
       << '\n'
       << "  Checksum:           0x"
       << std::hex
       << std::setw(4)
       << std::setfill('0')
       << header.checksum_ntoh()
       << std::dec
       << std::setfill(' ')
       << '\n'
       << "  Urgent Pointer:     0x"
       << std::hex
       << std::setw(4)
       << std::setfill('0')
       << header.urgent_pointer_ntoh()
       << std::dec
       << std::setfill(' ')
       << "\n}";
}

std::ostream& operator<<(std::ostream& os, const TcpHeader& header) {
    PrintTcpHeader(os, header);
    return os;
}

std::ostream& operator<<(std::ostream& os, const TcpPacketView& segment) {
    PrintTcpHeader(os, segment);
    return os;
}

void TcpHeader::serialize(std::span<uint8_t> out) const noexcept {
    std::memcpy(out.data(), this, sizeof(TcpHeader));
}

uint16_t TcpHeader::compute_checksum() const noexcept {
    uint8_t wire[20];
    serialize(wire);
    wire[16] = 0;
    wire[17] = 0;
    return OnesComplementChecksum(wire, sizeof(wire));
}

uint16_t TcpHeader::compute_checksum(uint32_t src_ip_net, uint32_t dst_ip_net,
                             std::span<const uint8_t> payload) const noexcept {
    uint8_t wire[20];
    serialize(wire);
    return ComputeTcpChecksum(src_ip_net, dst_ip_net, wire, sizeof(wire),
                              payload.data(), payload.size());
}

size_t WriteTcpPacket(std::span<uint8_t> out, uint32_t src_ip_net,
                        uint32_t dst_ip_net, const TcpPacket& packet) {
    const size_t header_len = packet.header.header_length() > 0 ? packet.header.header_length() : sizeof(TcpHeader);
    const size_t total = header_len + packet.payload.size();
    if (out.size() < total) {
        return 0;
    }

    TcpHeader header = packet.header;
    if (header.data_offset() == 0) {
        header.set_data_offset(5);
    }
    header.set_checksum(0);

    const uint16_t chk = header.compute_checksum(src_ip_net, dst_ip_net, packet.payload);
    header.set_checksum(chk);

    uint8_t wire[20];
    header.serialize(wire);
    std::copy_n(wire, sizeof(wire), out.data());
    if (!packet.payload.empty()) {
        std::copy_n(packet.payload.data(), packet.payload.size(),
                    out.data() + header_len);
    }
    return total;
}

size_t WriteTcpPacket(std::span<uint8_t> out, const TcpPacket& packet) {
    const size_t header_len = packet.header.header_length() > 0 ? packet.header.header_length() : sizeof(TcpHeader);
    const size_t total = header_len + packet.payload.size();
    if (out.size() < total) {
        return 0;
    }

    TcpHeader header = packet.header;
    if (header.data_offset() == 0) {
        header.set_data_offset(5);
    }

    uint8_t wire[20];
    header.serialize(wire);
    std::copy_n(wire, sizeof(wire), out.data());
    if (!packet.payload.empty()) {
        std::copy_n(packet.payload.data(), packet.payload.size(),
                    out.data() + header_len);
    }
    return total;
}

