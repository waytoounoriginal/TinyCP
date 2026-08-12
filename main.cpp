#include <iostream>

#include "IPv4Packet.h"
#include "TcpSocket.h"




int main() {

    char interface[255] = "tun0";

    TcpSocket socket{interface};
    std:: cout << interface << std::endl;

    while (true) {
        alignas(IPv4Header)char buf[1024] = {0};

        auto bytesRead = socket.device().tun_read(buf, sizeof(buf));

        if (bytesRead < 0) {
            perror("tun_read");
            break;
        }

        std::cout << "Read " << bytesRead << " bytes" << std::endl;
        std::cout << *reinterpret_cast<IPv4Header*>(&buf) << std::endl;
    }

    return 0;
}