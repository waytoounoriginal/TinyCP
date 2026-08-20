#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>

#include "IPv4.h"
#include "TcpSocket.h"
#include "TcpStack.h"
#include "TunDevice.h"

namespace {

void PrintUsage(const char* prog) {
    std::cout << "TinyCP HTTP GET Client\n"
              << "Usage:\n"
              << "  " << prog << " [IP] [PORT] [PATH] [HOST]\n"
              << "Example:\n"
              << "  " << prog << " 172.217.171.174 80 / google.com\n"
              << "  " << prog << " 10.0.0.1 8080 /index.html 10.0.0.1\n\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string ip_str = "172.217.171.174"; // Default: google.com
    uint16_t port = 80;
    std::string path = "/";
    std::string host_header = "google.com";

    if (argc > 1) {
        if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            PrintUsage(argv[0]);
            return 0;
        }
        ip_str = argv[1];
    }
    if (argc > 2) port = static_cast<uint16_t>(std::stoi(argv[2]));
    if (argc > 3) path = argv[3];
    if (argc > 4) host_header = argv[4];
    else if (argc > 1) host_header = ip_str;

    std::cout << "========================================\n"
              << "       TinyCP HTTP/1.1 GET Client       \n"
              << "========================================\n"
              << "Target IP:   " << ip_str << ":" << port << "\n"
              << "Path:        " << path << "\n"
              << "Host Header: " << host_header << "\n"
              << "Interface:   tun0\n"
              << "----------------------------------------\n";

    TunDevice tun{"tun0"};
    TcpStack stack{tun};
    TcpSocket socket{stack};

    // 1. Parse target IP
    std::string target_endpoint = ip_str + ":" + std::to_string(port);
    IPv4Address server_addr = IPv4Address::from_string(target_endpoint.c_str());

    std::cout << "[1/4] Connecting to " << target_endpoint << "..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    socket.connect(server_addr);

    if (socket.state() != TcpState::ESTABLISHED) {
        std::cerr << "Failed to establish TCP connection. State: " 
                  << TCP_STATE_TO_STRING(socket.state()) << std::endl;
        return 1;
    }
    std::cout << "[2/4] TCP Handshake ESTABLISHED!" << std::endl;

    // 2. Build HTTP GET Request
    std::ostringstream request_stream;
    request_stream << "GET " << path << " HTTP/1.1\r\n"
                   << "Host: " << host_header << "\r\n"
                   << "User-Agent: TinyCP/1.0 (Educational TCP Stack)\r\n"
                   << "Accept: */*\r\n"
                   << "Connection: close\r\n\r\n";

    std::string request = request_stream.str();

    std::cout << "[3/4] Sending HTTP GET Request (" << request.size() << " bytes):\n"
              << "----------------------------------------\n"
              << request
              << "----------------------------------------\n";

    size_t sent = socket.send({reinterpret_cast<const uint8_t*>(request.data()), request.size()});
    if (sent != request.size()) {
        std::cerr << "Warning: Only " << sent << " of " << request.size() << " bytes sent." << std::endl;
    }

    // 3. Receive HTTP Response Stream
    std::cout << "[4/4] Receiving HTTP Response..." << std::endl;
    std::vector<uint8_t> response_data;
    uint8_t buffer[4096];

    while (true) {
        size_t bytes_read = socket.recv({buffer, sizeof(buffer)});
        if (bytes_read == 0) {
            // EOF: Peer closed connection
            break;
        }
        response_data.insert(response_data.end(), buffer, buffer + bytes_read);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // 4. Close socket
    socket.close();

    std::cout << "\n========================================\n"
              << "           HTTP Response Body           \n"
              << "========================================\n";

    std::string response_str(response_data.begin(), response_data.end());
    std::cout << response_str << "\n";

    std::cout << "========================================\n"
              << "Total Bytes Received: " << response_data.size() << " bytes\n"
              << "Total Elapsed Time:   " << elapsed_ms << " ms\n"
              << "Connection Status:    CLOSED\n"
              << "========================================\n";

    return 0;
}
