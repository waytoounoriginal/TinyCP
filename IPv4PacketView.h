#ifndef TCP_FROM_SCRATCH_IPV4PACKETVIEW_H
#define TCP_FROM_SCRATCH_IPV4PACKETVIEW_H

#include <cstdint>
#include <cstddef>
#include <optional>
#include <ostream>
#include <span>
#include <arpa/inet.h>

/**
 * Non-owning view of an IPv4 packet (RFC 791).
 *
 * Parses the header fields directly from the raw wire bytes, with no
 * copy into a struct. This complements the concrete IPv4Header /
 * IPv4Packet, which stay useful for building packets (e.g. when
 * writing to a socket).
 *
 * The getter API mirrors IPv4Header exactly, so the two can be used
 * interchangeably. 
 */
class IPv4PacketView {
public:
    /** The minimum length of a valid IPv4 header, in bytes. */
    static constexpr size_t kMinLength = 20;

    explicit IPv4PacketView(std::span<const uint8_t> bytes) noexcept
        : bytes_{bytes} {}

    /**
     * True when the wrapped bytes hold a complete, valid IPv4
     * datagram: at least a 20-byte header, IP version 4, and a total
     * length that fits inside the buffer.
     */
    bool valid() const noexcept {
        return bytes_.size() >= kMinLength
            && version() == 4
            && total_length_ntoh() >= header_length()
            && total_length_ntoh() <= bytes_.size();
    }

    /** The raw bytes this view wraps. */
    std::span<const uint8_t> bytes() const noexcept {
        return bytes_;
    }

    /*
     * Raw getters.
     *
     * These return the values in network byte order, mirroring the
     * concrete IPv4Header's raw getters.
     */

    uint8_t version_ihl() const noexcept {
        return bytes_[0];
    }

    uint8_t type_of_service() const noexcept {
        return bytes_[1];
    }

    uint16_t total_length() const noexcept {
        return ntohs(BE16(2));
    }

    uint16_t identification() const noexcept {
        return ntohs(BE16(4));
    }

    uint16_t flags_fragment_offset() const noexcept {
        return ntohs(BE16(6));
    }

    uint8_t ttl() const noexcept {
        return bytes_[8];
    }

    uint8_t protocol() const noexcept {
        return bytes_[9];
    }

    uint16_t checksum() const noexcept {
        return ntohs(BE16(10));
    }

    uint32_t source_address() const noexcept {
        return ntohl(BE32(12));
    }

    uint32_t destination_address() const noexcept {
        return ntohl(BE32(16));
    }

    /*
     * Interpreted / host-order getters.
     */

    /** Returns the IP version. */
    uint8_t version() const noexcept {
        return bytes_[0] >> 4;
    }

    /** Returns the Internet Header Length in 32-bit words. */
    uint8_t IHL() const noexcept {
        return bytes_[0] & 0x0F;
    }

    /** Returns the Internet Header Length in bytes. */
    uint8_t header_length() const noexcept {
        return IHL() * 4;
    }

    /** Returns Total Length in host byte order. */
    uint16_t total_length_ntoh() const noexcept {
        return BE16(2);
    }

    /** Returns Identification in host byte order. */
    uint16_t identification_ntoh() const noexcept {
        return BE16(4);
    }

    /** Returns the 3-bit Flags field in host byte order. */
    uint8_t flags() const noexcept {
        return static_cast<uint8_t>(BE16(6) >> 13);
    }

    /** Returns Fragment Offset in host byte order. */
    uint16_t fragment_offset() const noexcept {
        return BE16(6) & 0x1FFF;
    }

    /** Returns Flags + Fragment Offset in host byte order. */
    uint16_t flags_fragment_offset_ntoh() const noexcept {
        return BE16(6);
    }

    /** Returns Checksum in host byte order. */
    uint16_t checksum_ntoh() const noexcept {
        return BE16(10);
    }

    /** Returns source address in host byte order. */
    uint32_t source_address_ntoh() const noexcept {
        return BE32(12);
    }

    /** Returns destination address in host byte order. */
    uint32_t destination_address_ntoh() const noexcept {
        return BE32(16);
    }

    /**
     * The payload, starting at the end of the header.
     *
     * Precondition: valid().
     */
    std::span<const uint8_t> payload() const noexcept {
        return bytes_.subspan(header_length(),
                              total_length_ntoh() - header_length());
    }

    /**
     * Parses an IPv4 datagram from raw bytes.
     *
     * Returns std::nullopt unless valid().
     */
    static std::optional<IPv4PacketView> Parse(
        std::span<const uint8_t> bytes) noexcept {
        IPv4PacketView view{bytes};
        if (!view.valid()) {
            return std::nullopt;
        }
        return view;
    }

    friend std::ostream& operator<<(
        std::ostream& os,
        const IPv4PacketView& packet
    );

private:
    /** Helpers for reading big endian values */
    
    uint16_t BE16(size_t offset) const noexcept {
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(bytes_[offset]) << 8) | bytes_[offset + 1]
        );
    }

    uint32_t BE32(size_t offset) const noexcept {
        return (static_cast<uint32_t>(bytes_[offset]) << 24)
             | (static_cast<uint32_t>(bytes_[offset + 1]) << 16)
             | (static_cast<uint32_t>(bytes_[offset + 2]) << 8)
             | static_cast<uint32_t>(bytes_[offset + 3]);
    }

    std::span<const uint8_t> bytes_;
};

std::ostream& operator<<(std::ostream& os, const IPv4PacketView& packet);

#endif // TCP_FROM_SCRATCH_IPV4PACKETVIEW_H