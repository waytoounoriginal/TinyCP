#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

#include "TcpSocket.h"
#include "TcpStack.h"

int main() {
    auto& stack = TcpStack::instance();

    IPv4Address addr_a{ 0x0A000002 /* 10.0.0.2 */, 8080 };
    IPv4Address addr_b{ 0x0A000003 /* 10.0.0.3 */, 9090 };

    // 1. Create and bind Socket A and Socket B
    TcpSocket socket_a;
    socket_a.bind(addr_a);

    TcpSocket socket_b;
    socket_b.bind(addr_b);

    // 2. Register connection 4-tuples in TcpStack
    stack.register_connection(addr_a, addr_b, socket_a.tcb());
    stack.register_connection(addr_b, addr_a, socket_b.tcb());

    std::cout << "Socket A (10.0.0.2:8080) <---> Socket B (10.0.0.3:9090) initialized." << std::endl;

    // 3. Socket A sends payload data (pushes to send_buffer and notifies dirty blocks)
    const char message[] = "Hello from Socket A via automatic TcpStack dirty blocks!";
    std::cout << "\n[Socket A] Sending payload: \"" << message << "\"" << std::endl;
    socket_a.send({reinterpret_cast<const uint8_t*>(message), sizeof(message)});

    // 4. Socket B reads the payload directly from its receive buffer
    uint8_t recv_buf[128] = {0};
    size_t bytes_recvd = socket_b.recv({recv_buf, sizeof(recv_buf)});

    std::cout << "[Socket B] Received payload (" << bytes_recvd << " bytes): \""
              << reinterpret_cast<char*>(recv_buf) << "\"\n" << std::endl;

    return 0;
}