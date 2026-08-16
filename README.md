# T(iny)CP

> **Work in Progress**: T(iny)CP is an educational, userspace TCP/IP stack implemented from scratch in C++20. The codebase is under active development.

## Overview

T(iny)CP is a lightweight, userspace implementation of the Transmission Control Protocol (RFC 793) operating over a virtual Linux TUN interface (`/dev/net/tun`).

## Features

- **Instance-Based Design**: Decoupled, instance-driven architecture (`TunDevice`, `TcpStack`, `TcpSocket`).
- **RFC 793 Headers & Views**: Zero-copy packet view abstractions (`IPv4PacketView`, `TcpPacketView`) and header serialization (`IPv4Header`, `TcpHeader`).
- **3-Way Handshake**: Active (`connect()`) and passive (`listen()` / `accept()`) connection establishment.
- **RST Handling**: Reset generation and connection abort logic for unsynchronized and closed states.
- **String IP Parsing**: Address parsing via `IPv4Address::from_string("10.0.0.2:8080")`.
- **Test Harness**: In-memory packet injection and outbound packet interception (`TCP_STACK_TESTING`).

## Final Deliverables

- **High-Traffic Concurrent Test**: A test suite simulating high-volume concurrent socket traffic.
- **HTTP Client / Server Application**: A small HTTP server or client application capable of fetching a webpage over an online HTTP connection.

## Building & Testing

```bash
# Configure
cmake -B cmake-build-debug -S .

# Build
cmake --build cmake-build-debug --config Debug

# Test
ctest --test-dir cmake-build-debug -C Debug --output-on-failure
```

## Roadmap

- Advanced RFC 793 connection synchronization (Simultaneous Open & Half-Open recovery)
- Connection teardown state machine (`FIN_WAIT_1`, `FIN_WAIT_2`, `TIME_WAIT`, `CLOSE_WAIT`, `LAST_ACK`)
- Sliding window flow control and retransmission timers
