#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "IPv4PacketView.h"
#include "TcpSocket.h"

namespace {

constexpr size_t kBufferSize = 65536;

/** Prints the payload as a hex dump, capped for readability. */
void PrintPayload(std::span<const uint8_t> payload) {
    constexpr size_t kMaxBytes = 64;

    const size_t shown = std::min(payload.size(), kMaxBytes);
    std::cout << "  Payload:         " << payload.size() << " bytes"
              << (shown < payload.size() ? " (showing first 64)" : "")
              << '\n';

    for (size_t i = 0; i < shown; ++i) {
        if (i % 16 == 0) {
            std::cout << "    ";
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(payload[i]) << ' ';
        if (i % 16 == 15 || i == shown - 1) {
            std::cout << '\n';
        }
    }
    std::cout << std::dec << std::setfill(' ');
}

void DumpPacket(const IPv4PacketView& packet) {
    std::cout << packet << '\n';
    PrintPayload(packet.payload());
    std::cout << '\n';
}

/** Reads packets until EOF, one full IP datagram at a time. */
void DumpFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open file: " << path << '\n';
        std::exit(1);
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

    size_t offset = 0;
    size_t packet_count = 0;
    while (offset < data.size()) {
        const auto packet = IPv4PacketView::Parse(
            std::span<const uint8_t>(data).subspan(offset));
        if (!packet) {
            std::cerr << "Skipping " << (data.size() - offset)
                      << " trailing bytes: not a valid IPv4 packet\n";
            break;
        }
        ++packet_count;
        DumpPacket(*packet);
        offset += packet->total_length_ntoh();
    }

    std::cout << "Read " << packet_count << " packet(s)\n";
}

/** Reads packets from the TUN device until interrupted. */
void DumpFromTun(const std::string& interface) {
    char name[255] = {};
    std::strncpy(name, interface.c_str(), sizeof(name) - 1);

    TcpSocket socket;
    std::cout << "Listening on " << name << "...\n";

    uint8_t buf[kBufferSize] = {};

    while (true) {
        const ssize_t bytes_read = TunDevice::instance().tun_read(
            reinterpret_cast<char*>(buf), sizeof(buf));

        if (bytes_read < 0) {
            perror("tun_read");
            break;
        }

        std::cout << "Read " << bytes_read << " bytes\n";

        const auto packet = IPv4PacketView::Parse(
            std::span<const uint8_t>(buf, static_cast<size_t>(bytes_read)));
        if (!packet) {
            std::cerr << "Not a valid IPv4 packet, skipping\n";
            continue;
        }

        DumpPacket(*packet);
    }
}

void PrintUsage(const char* program) {
    std::cout << "Usage:\n"
              << "  " << program << "                    "
              << "read packets from TUN device (tun0)\n"
              << "  " << program << " --tun <name>       "
              << "read packets from the given TUN device\n"
              << "  " << program << " --file <path>      "
              << "replay packets from a binary capture file\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        DumpFromTun("tun0");
        return 0;
    }

    if ((argc == 3) && std::strcmp(argv[1], "--file") == 0) {
        DumpFromFile(argv[2]);
        return 0;
    }

    if ((argc == 3) && std::strcmp(argv[1], "--tun") == 0) {
        DumpFromTun(argv[2]);
        return 0;
    }

    PrintUsage(argv[0]);
    return 1;
}