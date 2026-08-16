#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

#include "TcpSocket.h"
#include "TcpStack.h"

int main() {
    TunDevice tun{"tun0"};
    TcpStack stack{tun};

    IPv4Address addr_a{ 0x0A000002 /* 10.0.0.2 */, 8080 };
    IPv4Address addr_b{ 0x0A000003 /* 10.0.0.3 */, 9090 };

    // 1. Create and bind Socket A and Socket B
    TcpSocket socket_a{stack};
    socket_a.bind(addr_a);

    TcpSocket socket_b{stack};
    socket_b.bind(addr_b);

    // 2. Register connection 4-tuples in TcpStack
    stack.register_connection(addr_a, addr_b, socket_a.tcb());
    stack.register_connection(addr_b, addr_a, socket_b.tcb());

    std::cout << "Socket A (10.0.0.2:8080) <---> Socket B (10.0.0.3:9090) initialized." << std::endl;

    constexpr size_t kIterations = 100000;
    std::cout << "\nStarting workload loop (" << kIterations << " iterations)..." << std::endl;

    const char message[] = "Hello from Socket A via automatic TcpStack dirty blocks!";
    uint8_t recv_buf[128] = {0};

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < kIterations; ++i) {
        socket_a.send({reinterpret_cast<const uint8_t*>(message), sizeof(message)});
        auto size_recved = socket_b.recv({recv_buf, sizeof(recv_buf)});
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Completed " << kIterations << " iterations in " << elapsed << " ms." << std::endl;

    return 0;
}