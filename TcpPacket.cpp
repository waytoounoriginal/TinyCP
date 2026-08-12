//
// Created by waytoounoriginal on 8/11/2026.
//

#include "TcpPacket.h"
#include "TcpPacketView.h"

#include <iomanip>
#include <ostream>
#include <arpa/inet.h>

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
