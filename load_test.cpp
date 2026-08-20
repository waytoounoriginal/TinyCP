#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <cstring>

#include "IPv4.h"
#include "TcpSocket.h"
#include "TcpStack.h"
#include "TunDevice.h"

namespace {

struct LoadTestConfig {
    size_t single_stream_iterations = 1000;
    size_t concurrent_streams = 8;
    size_t concurrent_iterations_per_stream = 100;
    size_t payload_size = 64;
};

void PrintUsage(const char* prog) {
    std::cout << "TinyCP Load Testing Suite\n"
              << "Usage:\n"
              << "  " << prog << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --iterations <N>      Number of single-stream iterations (default: 1000)\n"
              << "  --streams <K>         Number of concurrent socket streams (default: 8)\n"
              << "  --per-stream <M>      Iterations per concurrent stream (default: 100)\n"
              << "  --payload <bytes>     Payload size in bytes (default: 64)\n"
              << "  --help, -h            Show this help message\n\n";
}

void RunSingleStreamThroughputTest(TcpStack& stack, size_t iterations, size_t payload_size) {
    std::cout << "\n=======================================================\n"
              << " [Phase 1] High-Volume Single-Stream Throughput Test   \n"
              << "=======================================================\n"
              << "Iterations:   " << iterations << "\n"
              << "Payload Size: " << payload_size << " bytes\n"
              << "Total Data:   " << (iterations * payload_size * 2) / 1024.0 << " KB (bidirectional)\n"
              << "-------------------------------------------------------\n";

    IPv4Address server_addr = IPv4Address::from_string("10.0.0.3:9090");

    TcpSocket server_socket{stack};
    server_socket.bind(server_addr);
    server_socket.listen();

    TcpSocket client_socket{stack};

    std::vector<uint8_t> send_payload(payload_size, 'A');
    std::vector<uint8_t> recv_buf(payload_size + 64);

    std::atomic<size_t> total_sends{0};
    std::atomic<size_t> failed_sends{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    std::thread server_thread([&]() {
        TcpSocket accepted = server_socket.accept();
        for (size_t i = 0; i < iterations; ++i) {
            size_t n = accepted.recv({recv_buf.data(), recv_buf.size()});
            if (n > 0) {
                total_sends.fetch_add(1, std::memory_order_relaxed);
                size_t sent = accepted.send({send_payload.data(), send_payload.size()});
                if (sent == 4544) {
                    failed_sends.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        accepted.close();
    });

    std::thread client_thread([&]() {
        client_socket.connect(server_addr);
        for (size_t i = 0; i < iterations; ++i) {
            total_sends.fetch_add(1, std::memory_order_relaxed);
            size_t sent = client_socket.send({send_payload.data(), send_payload.size()});
            if (sent == 4544) {
                failed_sends.fetch_add(1, std::memory_order_relaxed);
            }
            size_t n = client_socket.recv({recv_buf.data(), recv_buf.size()});
            (void)n;
        }
        client_socket.close();
    });

    client_thread.join();
    server_thread.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end_time - start_time).count();
    double total_mb = static_cast<double>(iterations * payload_size * 2) / (1024.0 * 1024.0);
    double mbps = total_mb / elapsed_sec;
    double rps = static_cast<double>(iterations) / elapsed_sec;

    size_t total = total_sends.load();
    size_t fails = failed_sends.load();
    double fail_rate = total > 0 ? (100.0 * static_cast<double>(fails) / static_cast<double>(total)) : 0.0;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Results:\n"
              << "  Elapsed Time: " << elapsed_sec * 1000.0 << " ms\n"
              << "  Throughput:   " << mbps << " MB/s\n"
              << "  Requests/sec: " << rps << " RPS\n"
              << "  Total Sends:  " << total << "\n"
              << "  Failed Sends: " << fails << " (4544)\n"
              << "  Failure Rate: " << fail_rate << "%\n"
              << "  Status:       " << (fails == 0 ? "SUCCESS" : "DEGRADED") << "\n";
}

void RunConcurrentMultiSocketTest(TcpStack& stack, size_t num_streams, size_t iterations_per_stream, size_t payload_size) {
    std::cout << "\n=======================================================\n"
              << " [Phase 2] Concurrent Multi-Socket Scaling Test        \n"
              << "=======================================================\n"
              << "Concurrent Streams:    " << num_streams << "\n"
              << "Iterations/Stream:     " << iterations_per_stream << "\n"
              << "Total Transactions:    " << num_streams * iterations_per_stream << "\n"
              << "Payload Size:          " << payload_size << " bytes\n"
              << "-------------------------------------------------------\n";

    IPv4Address server_addr = IPv4Address::from_string("10.0.0.3:9091");

    TcpSocket server_socket{stack};
    server_socket.bind(server_addr);
    server_socket.listen();

    std::atomic<size_t> completed_transactions{0};
    std::atomic<size_t> total_sends{0};
    std::atomic<size_t> failed_sends{0};
    auto start_time = std::chrono::high_resolution_clock::now();

    // Server worker loop accepting connections
    std::vector<std::thread> server_workers;
    std::atomic<bool> server_running{true};

    for (size_t i = 0; i < num_streams; ++i) {
        server_workers.emplace_back([&, i]() {
            (void)i;
            TcpSocket accepted = server_socket.accept();
            std::vector<uint8_t> buf(payload_size);
            for (size_t it = 0; it < iterations_per_stream; ++it) {
                size_t n = accepted.recv({buf.data(), payload_size});
                if (n > 0) {
                    total_sends.fetch_add(1, std::memory_order_relaxed);
                    size_t sent = accepted.send({buf.data(), n});
                    if (sent == 4544) {
                        failed_sends.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
            accepted.close();
        });
    }

    // Client workers
    std::vector<std::thread> client_workers;
    std::vector<uint8_t> client_payload(payload_size, 'X');

    for (size_t i = 0; i < num_streams; ++i) {
        client_workers.emplace_back([&, i]() {
            (void)i;
            TcpSocket client{stack};
            client.connect(server_addr);
            std::vector<uint8_t> buf(payload_size);

            for (size_t it = 0; it < iterations_per_stream; ++it) {
                total_sends.fetch_add(1, std::memory_order_relaxed);
                size_t sent = client.send({client_payload.data(), client_payload.size()});
                if (sent == 4544) {
                    failed_sends.fetch_add(1, std::memory_order_relaxed);
                }
                size_t n = client.recv({buf.data(), payload_size});
                if (n > 0) {
                    completed_transactions.fetch_add(1, std::memory_order_relaxed);
                }
            }
            client.close();
        });
    }

    for (auto& t : client_workers) t.join();
    for (auto& t : server_workers) t.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end_time - start_time).count();
    size_t total_trans = completed_transactions.load();
    double total_mb = static_cast<double>(total_trans * payload_size * 2) / (1024.0 * 1024.0);
    double mbps = total_mb / elapsed_sec;
    double rps = static_cast<double>(total_trans) / elapsed_sec;

    size_t total = total_sends.load();
    size_t fails = failed_sends.load();
    double fail_rate = total > 0 ? (100.0 * static_cast<double>(fails) / static_cast<double>(total)) : 0.0;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Results:\n"
              << "  Elapsed Time:           " << elapsed_sec * 1000.0 << " ms\n"
              << "  Completed Transactions: " << total_trans << " / " << num_streams * iterations_per_stream << "\n"
              << "  Aggregate Throughput:   " << mbps << " MB/s\n"
              << "  Total Request Rate:     " << rps << " RPS\n"
              << "  Total Sends:            " << total << "\n"
              << "  Failed Sends:           " << fails << " (4544)\n"
              << "  Failure Rate:           " << fail_rate << "%\n"
              << "  Status:                 " << (fails == 0 ? "SUCCESS" : "DEGRADED") << "\n"
              << "=======================================================\n";
}

} // namespace

int main(int argc, char* argv[]) {
    LoadTestConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--iterations" && i + 1 < argc) {
            config.single_stream_iterations = std::stoul(argv[++i]);
        } else if (arg == "--streams" && i + 1 < argc) {
            config.concurrent_streams = std::stoul(argv[++i]);
        } else if (arg == "--per-stream" && i + 1 < argc) {
            config.concurrent_iterations_per_stream = std::stoul(argv[++i]);
        } else if (arg == "--payload" && i + 1 < argc) {
            config.payload_size = std::stoul(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    TunDevice tun{"tun0"};
    TcpStack stack{tun};

    std::cout << "=======================================================\n"
              << "          TinyCP Automated Load Testing Suite          \n"
              << "=======================================================\n";

    RunSingleStreamThroughputTest(stack, config.single_stream_iterations, config.payload_size);
    RunConcurrentMultiSocketTest(stack, config.concurrent_streams, config.concurrent_iterations_per_stream, config.payload_size);

    return 0;
}
