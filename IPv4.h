//
// Created by waytoounoriginal on 8/12/2026.
//

#ifndef TCP_FROM_SCRATCH_IPV4_H
#define TCP_FROM_SCRATCH_IPV4_H

#include <cstdint>
#include <string_view>
#include <sstream>
#include <string>

/** A simplified IPv4 address API */
struct IPv4Address {
    uint32_t address{0};
    uint16_t port{0};

    bool operator==(const IPv4Address& other) const = default;

    /** Parses IP address string (e.g. "10.0.0.2" or "10.0.0.2:8080") */
    static IPv4Address from_string(std::string_view str) noexcept {
        uint32_t a = 0, b = 0, c = 0, d = 0;
        uint16_t p = 0;

        auto colon_pos = str.find(':');
        std::string_view ip_part = (colon_pos != std::string_view::npos) ? str.substr(0, colon_pos) : str;

        std::stringstream ss{std::string(ip_part)};
        char dot1 = 0, dot2 = 0, dot3 = 0;
        if (ss >> a >> dot1 >> b >> dot2 >> c >> dot3 >> d && dot1 == '.' && dot2 == '.' && dot3 == '.') {
            uint32_t ip = (a << 24) | (b << 16) | (c << 8) | d;
            if (colon_pos != std::string_view::npos) {
                std::stringstream p_ss{std::string(str.substr(colon_pos + 1))};
                p_ss >> p;
            }
            return { ip, p };
        }
        return { 0, 0 };
    }
};

#endif //TCP_FROM_SCRATCH_IPV4_H
