#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "IPv4.h"
#include "TcpSocket.h"
#include "TcpStack.h"
#include "TunDevice.h"

namespace {

TEST(HttpClientTest, HttpGetEndToEndTransaction) {
    TunDevice tun;
    TcpStack stack{tun};

    IPv4Address server_addr = IPv4Address::from_string("10.0.0.3:80");

    TcpSocket server_socket{stack};
    server_socket.bind(server_addr);
    server_socket.listen();

    EXPECT_EQ(server_socket.state(), TcpState::LISTEN);

    const std::string http_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 46\r\n"
        "Connection: close\r\n\r\n"
        "<html><body><h1>Hello TinyCP!</h1></body></html>";

    std::thread server_thread([&]() {
        // 1. Accept incoming HTTP client
        TcpSocket accepted_socket = server_socket.accept();
        EXPECT_EQ(accepted_socket.state(), TcpState::ESTABLISHED);

        // 2. Read HTTP request from client
        uint8_t req_buf[512] = {};
        size_t bytes_read = accepted_socket.recv({req_buf, sizeof(req_buf)});
        EXPECT_GT(bytes_read, 0u);

        std::string request_str(reinterpret_cast<char*>(req_buf), bytes_read);
        EXPECT_NE(request_str.find("GET /index.html HTTP/1.1"), std::string::npos);

        // 3. Send HTTP 200 OK response
        size_t bytes_sent = accepted_socket.send({
            reinterpret_cast<const uint8_t*>(http_response.data()),
            http_response.size()
        });
        EXPECT_EQ(bytes_sent, http_response.size());

        // 4. Close accepted socket
        accepted_socket.close();
    });

    std::thread client_thread([&]() {
        TcpSocket client_socket{stack};

        // 1. Connect to HTTP server
        client_socket.connect(server_addr);
        EXPECT_EQ(client_socket.state(), TcpState::ESTABLISHED);

        // 2. Transmit HTTP/1.1 GET request
        const std::string get_request =
            "GET /index.html HTTP/1.1\r\n"
            "Host: 10.0.0.3\r\n"
            "User-Agent: TinyCP/1.0\r\n"
            "Connection: close\r\n\r\n";

        size_t sent = client_socket.send({
            reinterpret_cast<const uint8_t*>(get_request.data()),
            get_request.size()
        });
        EXPECT_EQ(sent, get_request.size());

        // 3. Stream response until EOF
        std::vector<uint8_t> received_bytes;
        uint8_t resp_buf[256];
        while (true) {
            size_t n = client_socket.recv({resp_buf, sizeof(resp_buf)});
            if (n == 0) break;
            received_bytes.insert(received_bytes.end(), resp_buf, resp_buf + n);
        }

        std::string received_str(received_bytes.begin(), received_bytes.end());
        EXPECT_NE(received_str.find("HTTP/1.1 200 OK"), std::string::npos);
        EXPECT_NE(received_str.find("Hello TinyCP!"), std::string::npos);

        // 4. Close client socket
        client_socket.close();
    });

    client_thread.join();
    server_thread.join();
}

} // namespace
