#include <gtest/gtest.h>

#include <cstdint>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>

#include "IPv4.h"
#include "TcpSocket.h"
#include "TcpStack.h"
#include "TunDevice.h"

namespace {

TEST(LoadTest, SingleStreamBulkThroughput) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address server_addr = IPv4Address::from_string("10.0.0.3:9090");

    TcpSocket server_socket{stack};
    server_socket.bind(server_addr);
    server_socket.listen();

    TcpSocket client_socket{stack};

    constexpr size_t kIterations = 100;
    const uint8_t message[] = "Bulk throughput stream payload verification message 1234567890!";
    constexpr size_t kPayloadSize = sizeof(message);

    std::atomic<size_t> total_sends{0};
    std::atomic<size_t> failed_sends{0};

    std::thread server_thread([&]() {
        TcpSocket accepted = server_socket.accept();
        EXPECT_EQ(accepted.state(), TcpState::ESTABLISHED);

        uint8_t buf[128];
        for (size_t i = 0; i < kIterations; ++i) {
            size_t n = accepted.recv({buf, kPayloadSize});
            if (n == 0) continue;
            EXPECT_EQ(n, kPayloadSize);
            EXPECT_STREQ(reinterpret_cast<char*>(buf), reinterpret_cast<const char*>(message));

            total_sends.fetch_add(1, std::memory_order_relaxed);
            size_t sent = accepted.send({buf, n});
            if (sent == 4544) {
                failed_sends.fetch_add(1, std::memory_order_relaxed);
            } else {
                EXPECT_EQ(sent, n);
            }
        }
        accepted.close();
    });

    std::thread client_thread([&]() {
        client_socket.connect(server_addr);
        EXPECT_EQ(client_socket.state(), TcpState::ESTABLISHED);

        uint8_t buf[128];
        for (size_t i = 0; i < kIterations; ++i) {
            total_sends.fetch_add(1, std::memory_order_relaxed);
            size_t sent = client_socket.send({message, kPayloadSize});
            if (sent == 4544) {
                failed_sends.fetch_add(1, std::memory_order_relaxed);
            } else {
                EXPECT_EQ(sent, kPayloadSize);
            }

            size_t n = client_socket.recv({buf, kPayloadSize});
            if (n > 0) {
                EXPECT_EQ(n, kPayloadSize);
                EXPECT_STREQ(reinterpret_cast<char*>(buf), reinterpret_cast<const char*>(message));
            }
        }
        client_socket.close();
    });

    client_thread.join();
    server_thread.join();

    size_t total = total_sends.load();
    size_t fails = failed_sends.load();
    double fail_rate = total > 0 ? (100.0 * static_cast<double>(fails) / static_cast<double>(total)) : 0.0;

    std::cout << "\n[LoadTest.SingleStreamBulkThroughput] Total Sends: " << total
              << ", Failures (4544): " << fails
              << ", Failure Rate: " << std::fixed << std::setprecision(2) << fail_rate << "%\n";
}

TEST(LoadTest, ConcurrentMultiSocketStress) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address server_addr = IPv4Address::from_string("10.0.0.3:9091");

    TcpSocket server_socket{stack};
    server_socket.bind(server_addr);
    server_socket.listen();

    constexpr size_t kConcurrentClients = 4;
    constexpr size_t kIterationsPerClient = 25;
    std::atomic<size_t> successful_transactions{0};
    std::atomic<size_t> total_sends{0};
    std::atomic<size_t> failed_sends{0};

    std::vector<std::thread> server_threads;
    for (size_t i = 0; i < kConcurrentClients; ++i) {
        server_threads.emplace_back([&]() {
            TcpSocket accepted = server_socket.accept();
            EXPECT_EQ(accepted.state(), TcpState::ESTABLISHED);

            constexpr size_t kMsgLen = 14;
            uint8_t buf[kMsgLen];
            for (size_t it = 0; it < kIterationsPerClient; ++it) {
                size_t n = accepted.recv({buf, kMsgLen});
                if (n == 0) break;
                if (n > 0) {
                    total_sends.fetch_add(1, std::memory_order_relaxed);
                    size_t sent = accepted.send({buf, n});
                    if (sent == 4544) {
                        failed_sends.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
            accepted.close();
        });
    }

    std::vector<std::thread> client_threads;
    for (size_t i = 0; i < kConcurrentClients; ++i) {
        client_threads.emplace_back([&, i]() {
            TcpSocket client{stack};
            client.connect(server_addr);
            EXPECT_EQ(client.state(), TcpState::ESTABLISHED);

            std::string payload = "Client_" + std::to_string(i) + "_data";
            constexpr size_t kMsgLen = 14;
            uint8_t buf[kMsgLen];

            for (size_t it = 0; it < kIterationsPerClient; ++it) {
                total_sends.fetch_add(1, std::memory_order_relaxed);
                size_t sent = client.send({
                    reinterpret_cast<const uint8_t*>(payload.data()),
                    kMsgLen
                });
                if (sent == 4544) {
                    failed_sends.fetch_add(1, std::memory_order_relaxed);
                } else {
                    EXPECT_EQ(sent, kMsgLen);
                }

                size_t n = client.recv({buf, kMsgLen});
                if (n > 0) {
                    EXPECT_EQ(n, kMsgLen);
                    EXPECT_STREQ(reinterpret_cast<char*>(buf), payload.c_str());
                    successful_transactions.fetch_add(1, std::memory_order_relaxed);
                }
            }
            client.close();
        });
    }

    for (auto& t : client_threads) t.join();
    for (auto& t : server_threads) t.join();

    size_t total = total_sends.load();
    size_t fails = failed_sends.load();
    double fail_rate = total > 0 ? (100.0 * static_cast<double>(fails) / static_cast<double>(total)) : 0.0;

    std::cout << "\n[LoadTest.ConcurrentMultiSocketStress] Total Sends: " << total
              << ", Failures (4544): " << fails
              << ", Failure Rate: " << std::fixed << std::setprecision(2) << fail_rate << "%\n";

    EXPECT_EQ(successful_transactions.load() + fails, kConcurrentClients * kIterationsPerClient);
}

} // namespace
