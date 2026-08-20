#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <cstdint>
#include <numeric>

#include "IPv4.h"
#include "TcpSocket.h"
#include "TcpStack.h"
#include "TunDevice.h"

namespace {

struct BenchmarkConfig {
    size_t total_mb = 50;           // Default: 50 MB
    size_t chunk_size = 16384;      // 16 KB chunk writes
    bool bidirectional = false;
};

void PrintUsage(const char* prog) {
    std::cout << "TinyCP High-Speed Throughput Benchmark\n"
              << "Usage:\n"
              << "  " << prog << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --mb <N>              Total data to transfer in Megabytes (default: 50)\n"
              << "  --gb <N>              Total data to transfer in Gigabytes (e.g. --gb 1)\n"
              << "  --chunk <bytes>       Application buffer chunk size in bytes (default: 16384)\n"
              << "  --bidirectional       Enable bidirectional concurrent echo transfer\n"
              << "  --help, -h            Show this help message\n\n";
}

// Fast rolling 32-bit checksum for streaming data verification
uint32_t FastChecksum(const uint8_t* data, size_t len, uint32_t seed = 0) {
    uint32_t sum = seed;
    for (size_t i = 0; i < len; ++i) {
        sum = (sum * 31) + data[i];
    }
    return sum;
}

} // namespace

int main(int argc, char* argv[]) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mb" && i + 1 < argc) {
            config.total_mb = std::stoul(argv[++i]);
        } else if (arg == "--gb" && i + 1 < argc) {
            config.total_mb = std::stoul(argv[++i]) * 1024;
        } else if (arg == "--chunk" && i + 1 < argc) {
            config.chunk_size = std::stoul(argv[++i]);
        } else if (arg == "--bidirectional") {
            config.bidirectional = true;
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    const size_t total_bytes = config.total_mb * 1024 * 1024;

    std::cout << "=======================================================\n"
              << "         TinyCP High-Volume Throughput Benchmark       \n"
              << "=======================================================\n"
              << "Target Volume:  " << config.total_mb << " MB (" << total_bytes << " bytes)\n"
              << "Chunk Size:     " << config.chunk_size << " bytes\n"
              << "Mode:           " << (config.bidirectional ? "Bidirectional Echo" : "Unidirectional Streaming") << "\n"
              << "-------------------------------------------------------\n";

    TunDevice tun{"tun0"};
    TcpStack stack{tun};

    IPv4Address server_addr = IPv4Address::from_string("10.0.0.3:9095");

    TcpSocket server_socket{stack};
    server_socket.bind(server_addr);
    server_socket.listen();

    TcpSocket client_socket{stack};

    // Pre-fill repeating test pattern in chunk
    std::vector<uint8_t> tx_chunk(config.chunk_size);
    for (size_t i = 0; i < config.chunk_size; ++i) {
        tx_chunk[i] = static_cast<uint8_t>((i % 251) + 1);
    }

    std::atomic<size_t> server_bytes_received{0};
    std::atomic<size_t> client_bytes_sent{0};
    std::atomic<uint32_t> rx_checksum{0};
    std::atomic<uint32_t> tx_checksum{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    // Server receiver thread
    std::thread server_thread([&]() {
        TcpSocket accepted = server_socket.accept();
        std::vector<uint8_t> rx_buf(config.chunk_size * 2);
        size_t total_rx = 0;
        uint32_t csum = 0;

        while (total_rx < total_bytes) {
            size_t to_read = std::min(rx_buf.size(), total_bytes - total_rx);
            size_t n = accepted.recv({rx_buf.data(), to_read});
            if (n == 0) break; // EOF

            csum = FastChecksum(rx_buf.data(), n, csum);
            total_rx += n;
            server_bytes_received.store(total_rx, std::memory_order_relaxed);

            if (config.bidirectional) {
                accepted.send({rx_buf.data(), n});
            }
        }

        rx_checksum.store(csum);
        accepted.close();
    });

    // Client sender thread
    std::thread client_thread([&]() {
        client_socket.connect(server_addr);
        size_t total_tx = 0;
        uint32_t csum = 0;

        while (total_tx < total_bytes) {
            size_t to_send = std::min(config.chunk_size, total_bytes - total_tx);
            size_t sent = client_socket.send({tx_chunk.data(), to_send});
            if (sent > 0) {
                csum = FastChecksum(tx_chunk.data(), sent, csum);
                total_tx += sent;
                client_bytes_sent.store(total_tx, std::memory_order_relaxed);
            }
        }

        tx_checksum.store(csum);
        client_socket.close();
    });

    // Main thread progress monitor
    while (server_bytes_received.load() < total_bytes && client_bytes_sent.load() < total_bytes) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        size_t rx = server_bytes_received.load();
        auto now = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration<double>(now - start_time).count();
        if (sec > 0.05) {
            double current_mb = static_cast<double>(rx) / (1024.0 * 1024.0);
            double current_speed = current_mb / sec;
            double progress = (static_cast<double>(rx) / static_cast<double>(total_bytes)) * 100.0;
            std::cout << "\r[Progress] " << std::fixed << std::setprecision(1) << progress << "% ("
                      << current_mb << " / " << config.total_mb << " MB) | Speed: "
                      << std::setprecision(2) << current_speed << " MB/s ("
                      << current_speed * 8.0 / 1000.0 << " Gbps)" << std::flush;
        }
    }

    client_thread.join();
    server_thread.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_elapsed_sec = std::chrono::duration<double>(end_time - start_time).count();
    size_t rx_final = server_bytes_received.load();
    double total_mb_transferred = static_cast<double>(rx_final) / (1024.0 * 1024.0);
    double avg_throughput_mbps = total_mb_transferred / total_elapsed_sec;
    double avg_throughput_gbps = (avg_throughput_mbps * 8.0) / 1000.0;

    bool checksum_match = (tx_checksum.load() == rx_checksum.load()) && (rx_final == total_bytes);

    std::cout << "\n\n=======================================================\n"
              << "                   Benchmark Summary                   \n"
              << "=======================================================\n"
              << "Total Transferred:   " << std::fixed << std::setprecision(2) << total_mb_transferred << " MB (" << rx_final << " bytes)\n"
              << "Elapsed Time:        " << total_elapsed_sec << " seconds (" << total_elapsed_sec * 1000.0 << " ms)\n"
              << "Average Throughput:  " << avg_throughput_mbps << " MB/s (" << avg_throughput_gbps << " Gbps)\n"
              << "Data Integrity:      " << (checksum_match ? "VERIFIED (Checksums Match 100%)" : "FAILED / CORRUPTED") << "\n"
              << "Status:              " << (checksum_match ? "SUCCESS" : "FAILED") << "\n"
              << "=======================================================\n";

    return checksum_match ? 0 : 1;
}
