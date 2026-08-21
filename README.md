# T(iny)CP

> A short implementation of an userspace TCP/IP stack, strictly covering RFC 793

---

## Project Deliverables

| Deliverable | Type | Description |
| :--- | :--- | :--- |
| **tinycp_core** | Static Library | Core RFC 793 state machine, sliding window flow control, RTO retransmissions, and socket table routing. |
| **http_client** | Executable | HTTP/1.1 GET client running on userspace TCP, fetching web pages over TUN/NAT. |
| **throughput_benchmark** | Executable | Bandwidth tool measuring MB/s and Gbps with streaming checksum verification. |
| **load_test** | Executable | High-concurrency multi-client stress test with automated error tracking. |
| **packet_dump** | Executable | Real-time TCP/IPv4 packet sniffer and header analyzer on tun0. |
| **setup_tun.sh** | Script | Automated tun0 interface setup, IP forwarding, and iptables NAT masquerading. |
| **GoogleTest Suite** | 12 Test Targets | Automated test suite validating RFC 793 states, teardown, loss recovery, and queues. |

---

## Key Features and Architecture

* **RFC 793 Connection Lifecycle:** 3-way handshake (SYN -> SYN-ACK -> ACK), simultaneous open, 4-way graceful teardown (FIN), and RST generation.
* **Asynchronous Sliding Window:**
  * Usable window calculation: usable_window = SND.WND - (SND.NXT - SND.UNA).
  * Non-blocking send() backed by BlockingBuffer with peek() (unsent data) and discard() (ACKed data).
  * Dynamic RCV.WND advertising and MSS (1460-byte) segmentation.
* **Autonomous Loss Recovery:** Per-socket RTO tracking with retransmission of unacknowledged data (offset = 0, SND.UNA) on timeout.
* **Zero-Copy Packet Views:** Lightweight TcpPacketView and IPv4PacketView over raw byte buffers with Internet Checksum validation.

---

## Quickstart

### 1. Configure the TUN Interface

```bash
# Setup tun0 on 10.0.0.1/24 with outbound NAT:
sudo ./setup_tun.sh

# Optional: Also configure an inbound port forward (e.g. forward host port 9090 to TinyCP):
sudo ./setup_tun.sh tun0 10.0.0.1/24 9090
```

### 2. Build the Project

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 3. Run Applications and Benchmarks

```bash
# Fetch a webpage via userspace TCP HTTP client:
sudo ./build/http_client 93.184.216.34 80 / example.com

# Run 100 MB Throughput Benchmark:
sudo ./build/throughput_benchmark --mb 100

# Run Multi-Stream Concurrency Stress Test:
sudo ./build/load_test --streams 8 --per-stream 100
```

### 4. Run Automated Test Suite

```bash
ctest --test-dir build --output-on-failure
```

---

## Source Tree

```
.
|-- BlockingBuffer.h           # Thread-safe circular ring buffer (peek/discard)
|-- ConnectionTable.h/.cpp     # 4-tuple connection routing table
|-- ListenerTable.h/.cpp       # Listening port routing table
|-- SocketTable.h/.cpp         # TCB lifetime management
|-- TcpStack.h/.cpp            # TCP stack core, daemon event loop, state machine
|-- TcpSocket.h/.cpp           # POSIX-like socket API (bind, connect, send, recv, close)
|-- TransmissionControlBlock.h # RFC 793 TCB sequence and window state
|-- IPv4.h                     # IPv4 headers, addresses, checksums, packet views
|-- TcpHeader.h / TcpPacket.h  # TCP header encoders and packet views
|-- TunDevice.h/.cpp           # Linux /dev/net/tun interface wrapper
|-- http_client.cpp            # HTTP GET client application
|-- throughput_benchmark.cpp   # Bulk bandwidth and integrity benchmark tool
|-- load_test.cpp              # Multi-client load test binary
|-- PacketDump.cpp             # Packet analyzer tool
|-- setup_tun.sh               # TUN interface setup and iptables NAT script
|-- utils/                     # Logger, Checksum, and FixedQueue utilities
`-- tests/                     # 12 Google Test targets
```

---

## License

MIT License
