# T(iny)CP

> **TinyCP** is an educational, high-performance userspace TCP/IP stack implemented from scratch in modern C++20, operating over a Linux virtual TUN interface (`/dev/net/tun`).

---

## Features

### 1. RFC 793 State Machine & Connection Lifecycle
* **Active Open (`connect`) & Passive Open (`listen` / `accept`):** Full 3-way handshake (`SYN` $\to$ `SYN-ACK` $\to$ `ACK`) and simultaneous opening support.
* **Graceful Teardown (`close`):** Complete 4-way FIN handshake supporting `FIN_WAIT_1`, `FIN_WAIT_2`, `CLOSE_WAIT`, `LAST_ACK`, and `CLOSED`.
* **RFC 793 Reset Generation (`RST`):** Accurate reset generation for non-existent connections, port unreachability, and state discrepancies.

### 2. Asynchronous Streaming & Sliding Window Flow Control
* **Asynchronous `send()` Pipeline:** Non-blocking application transmission backed by thread-safe circular ring buffers ([`BlockingBuffer`](BlockingBuffer.h)).
* **Sliding Window Flow Control:** Usable window calculation ($\text{usable\_window} = \text{SND.WND} - (\text{SND.NXT} - \text{SND.UNA})$) dynamically bounding transmission to peer capacity and MSS (1460 bytes).
* **Dynamic Receive Window Advertising:** Continually advertises available `recv_buffer` capacity in every outgoing segment.
* **Autonomous Background Retransmissions:** Daemon RTO tracking with retransmission of oldest unacknowledged bytes on timer expiry.

### 3. Thread-Safe Modular Architecture
* **Decoupled Tables:**
  * [`SocketTable`](SocketTable.h): Owns and manages the lifecycle of `TransmissionControlBlock` (TCB) instances.
  * [`ListenerTable`](ListenerTable.h): Routes incoming SYN packets on bound ports to listening sockets.
  * [`ConnectionTable`](ConnectionTable.h): Fast 4-tuple lookup `(remote_ip:port, local_ip:port) -> socket_id`.
* **Lockless Daemon Event Loop:** Polling lifecycle loop in [`TcpStack`](TcpStack.h) draining transmission queues and network interfaces with sub-millisecond latency.

### 4. Zero-Copy Packet Serialization
* **Packet Views:** [`IPv4PacketView`](IPv4.h) and [`TcpPacketView`](TcpPacket.h) provide zero-copy inspection of raw network buffers.
* **Header Encoders:** [`IPv4Header`](IPv4.h) and [`TcpHeader`](TcpHeader.h) with automatic Internet checksum calculation.

---

## Applications & Tools

### HTTP GET Client (`http_client`)
An HTTP/1.1 GET client running directly on top of TinyCP:

```bash
# General Syntax
./http_client [IP] [PORT] [PATH] [HOST]

# Example: Fetching local HTTP server
sudo ./http_client 10.0.0.1 8080 / 10.0.0.1

# Example: Fetching example.com (via NAT)
sudo ./http_client 93.184.216.34 80 / example.com
```

### High-Concurrency Benchmarks & Stress Tests
* `SingleStreamBulkThroughput`: Pipelined high-volume bidirectional throughput testing.
* `ConcurrentMultiSocketStress`: Concurrent multi-client client/server stress testing with automated error and retry tracking.

---

## Quickstart & TUN Interface Setup

TinyCP communicates with the host operating system and external networks via a virtual TUN device (`tun0`).

### 1. Configure `tun0` and NAT Routing

Use the automated helper script to initialize `tun0`, configure IP forwarding, and set up `iptables` NAT / Masquerading:

```bash
# Basic setup (tun0 on 10.0.0.1/24 with outbound NAT):
sudo ./setup_tun.sh

# Optional: Also configure an inbound port forward (e.g. forward port 9090 to TinyCP):
sudo ./setup_tun.sh tun0 10.0.0.1/24 9090
```

---

## Building & Testing

### Prerequisites
* C++20 compliant compiler (`gcc` 11+, `clang` 13+)
* `cmake` 3.20+
* Linux environment or WSL2 with TUN/TAP support

### Build Commands

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)

# Run Test Suite
ctest --test-dir build --output-on-failure
```

---

## Project Structure

```
.
├── BlockingBuffer.h           # Thread-safe ring buffer with peek/discard
├── ConnectionTable.h/.cpp     # 4-tuple connection routing table
├── ListenerTable.h/.cpp       # Listening port routing table
├── SocketTable.h/.cpp         # TCB memory ownership table
├── TcpStack.h/.cpp            # TCP stack core, daemon event loop, state machine
├── TcpSocket.h/.cpp           # User-facing socket API (bind, connect, send, recv, close)
├── TransmissionControlBlock.h # RFC 793 TCB sequence and window state
├── IPv4.h                     # IPv4 headers, addresses, checksums, packet views
├── TcpHeader.h                # TCP header serialization and flags
├── TcpPacket.h                # TCP packet representations and views
├── TunDevice.h/.cpp           # Linux /dev/net/tun interface wrapper
├── http_client.cpp            # HTTP GET client application
├── setup_tun.sh               # TUN interface setup & iptables NAT script
└── tests/                     # Comprehensive Google Test suite
```

---

## License

MIT License
