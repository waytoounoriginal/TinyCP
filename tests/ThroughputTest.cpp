#include <gtest/gtest.h>

#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <iomanip>

#include "IPv4.h"
#include "TcpSocket.h"
#include "TcpStack.h"
#include "TunDevice.h"

namespace {

uint32_t CalculateChecksum(const uint8_t* data, size_t len, uint32_t seed = 0) {
    uint32_t sum = seed;
    for (size_t i = 0; i < len; ++i) {
        sum = (sum * 31) + data[i];
    }
    return sum;
}

TEST(ThroughputTest, BulkTransfer5MB) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address server_addr = IPv4Address::from_string("10.0.0.2:9098"); // same ip

    TcpSocket server_socket{stack};
    server_socket.bind(server_addr);
    server_socket.listen();

    TcpSocket client_socket{stack};

    constexpr size_t kTotalBytes = 5 * 1024 * 1024; // 5 MB
    constexpr size_t kChunkSize = 16384;            // 16 KB chunks

    std::vector<uint8_t> tx_chunk(kChunkSize);
    for (size_t i = 0; i < kChunkSize; ++i) {
        tx_chunk[i] = static_cast<uint8_t>((i % 251) + 1);
    }

    std::atomic<size_t> bytes_received{0};
    std::atomic<uint32_t> rx_checksum{0};
    std::atomic<uint32_t> tx_checksum{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    std::thread server_thread([&]() {
        TcpSocket accepted = server_socket.accept();
        EXPECT_EQ(accepted.state(), TcpState::ESTABLISHED);

        std::vector<uint8_t> rx_buf(kChunkSize * 2);
        size_t total_rx = 0;
        uint32_t csum = 0;

        while (total_rx < kTotalBytes) {
            size_t to_read = std::min(rx_buf.size(), kTotalBytes - total_rx);
            size_t n = accepted.recv({rx_buf.data(), to_read});
            if (n == 0) break;

            csum = CalculateChecksum(rx_buf.data(), n, csum);
            total_rx += n;
            bytes_received.store(total_rx);
        }

        rx_checksum.store(csum);
        accepted.close();
    });

    std::thread client_thread([&]() {
        client_socket.connect(server_addr);
        EXPECT_EQ(client_socket.state(), TcpState::ESTABLISHED);

        size_t total_tx = 0;
        uint32_t csum = 0;

        while (total_tx < kTotalBytes) {
            size_t to_send = std::min(kChunkSize, kTotalBytes - total_tx);
            size_t sent = client_socket.send({tx_chunk.data(), to_send});
            if (sent > 0) {
                csum = CalculateChecksum(tx_chunk.data(), sent, csum);
                total_tx += sent;
            }
        }

        tx_checksum.store(csum);
        client_socket.close();
    });

    client_thread.join();
    server_thread.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end_time - start_time).count();
    double mb_transferred = static_cast<double>(bytes_received.load()) / (1024.0 * 1024.0);
    double mbps = mb_transferred / elapsed_sec;

    std::cout << "\n[ThroughputTest.BulkTransfer5MB] Transferred: " << std::fixed << std::setprecision(2)
              << mb_transferred << " MB in " << elapsed_sec * 1000.0 << " ms | Speed: "
              << mbps << " MB/s (" << mbps * 8.0 / 1000.0 << " Gbps)\n";

    EXPECT_EQ(bytes_received.load(), kTotalBytes);
    EXPECT_EQ(rx_checksum.load(), tx_checksum.load());
}

} // namespace
