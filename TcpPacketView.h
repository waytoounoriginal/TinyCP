#ifndef TCP_FROM_SCRATCH_TCPPACKETVIEW_H
#define TCP_FROM_SCRATCH_TCPPACKETVIEW_H

#include <cstdint>
#include <cstddef>
#include <optional>
#include <ostream>
#include <span>
#include <arpa/inet.h>

/**
 * Non-owning view of a TCP segment (RFC 793).
 *
 * Parses the header fields directly from the raw wire bytes, with no
 * copy into a struct. This complements the concrete TcpHeader /
 * TcpPacket, which stay useful for building segments (e.g. when
 * writing to a socket).
 *
 * The getter API mirrors TcpHeader exactly, so the two can be used
 * interchangeably.
 */
class TcpPacketView {
public:
    /** The minimum length of a TCP header, in bytes (no options). */
    static constexpr size_t kMinLength = 20;

    explicit TcpPacketView(std::span<const uint8_t> bytes) noexcept
        : bytes_{bytes} {}

    /**
     * True when the wrapped bytes hold a plausible TCP segment: at
     * least a 20-byte header, and a Data Offset that fits inside the
     * buffer.
     */
    bool valid() const noexcept {
        return bytes_.size() >= kMinLength
            && header_length() >= kMinLength
            && header_length() <= bytes_.size();
    }

    /** The raw bytes this view wraps. */
    std::span<const uint8_t> bytes() const noexcept {
        return bytes_;
    }

    /*
     * Raw getters.
     *
     * These return the values in network byte order, mirroring the
     * concrete TcpHeader's raw getters.
     */

    uint16_t source_port() const noexcept {
        return ntohs(BE16(0));
    }

    uint16_t dest_port() const noexcept {
        return ntohs(BE16(2));
    }

    uint32_t seq_num() const noexcept {
        return ntohl(BE32(4));
    }

    uint32_t ack_num() const noexcept {
        return ntohl(BE32(8));
    }

    uint16_t data_offset_and_reserved_and_control_bits() const noexcept {
        return ntohs(BE16(12));
    }

    uint16_t window() const noexcept {
        return ntohs(BE16(14));
    }

    uint16_t checksum() const noexcept {
        return ntohs(BE16(16));
    }

    uint16_t urgent_pointer() const noexcept {
        return ntohs(BE16(18));
    }

    /*
     * Interpreted / host-order getters.
     */

    /** Returns the source port in host byte order. */
    uint16_t source_port_ntoh() const noexcept {
        return BE16(0);
    }

    /** Returns the destination port in host byte order. */
    uint16_t dest_port_ntoh() const noexcept {
        return BE16(2);
    }

    /** Returns the sequence number in host byte order. */
    uint32_t seq_num_ntoh() const noexcept {
        return BE32(4);
    }

    /** Returns the acknowledgement number in host byte order. */
    uint32_t ack_num_ntoh() const noexcept {
        return BE32(8);
    }

    /** Returns Data Offset + Reserved + Control Bits in host byte order. */
    uint16_t data_offset_and_reserved_and_control_bits_ntoh() const noexcept {
        return BE16(12);
    }

    /**
     * Returns the TCP Data Offset: the header length in
     * 32-bit words. The minimum value for a correct header is 5.
     */
    uint8_t data_offset() const noexcept {
        return static_cast<uint8_t>(BE16(12) >> 12);
    }

    /** Returns the TCP header length in bytes. */
    uint8_t header_length() const noexcept {
        return static_cast<uint8_t>(data_offset() * 4);
    }

    /** Returns the 6 control bits in host byte order. */
    uint16_t flags() const noexcept {
        return BE16(12) & 0x3F;
    }

    /** Returns true if the URG control bit is set. */
    bool urg() const noexcept {
        return (flags() >> 5) & 1;
    }

    /** Returns true if the ACK control bit is set. */
    bool ack() const noexcept {
        return (flags() >> 4) & 1;
    }

    /** Returns true if the PSH control bit is set. */
    bool psh() const noexcept {
        return (flags() >> 3) & 1;
    }

    /** Returns true if the RST control bit is set. */
    bool rst() const noexcept {
        return (flags() >> 2) & 1;
    }

    /** Returns true if the SYN control bit is set. */
    bool syn() const noexcept {
        return (flags() >> 1) & 1;
    }

    /** Returns true if the FIN control bit is set. */
    bool fin() const noexcept {
        return flags() & 1;
    }

    /** Returns Window in host byte order. */
    uint16_t window_ntoh() const noexcept {
        return BE16(14);
    }

    /** Returns Checksum in host byte order. */
    uint16_t checksum_ntoh() const noexcept {
        return BE16(16);
    }

    /** Returns Urgent Pointer in host byte order. */
    uint16_t urgent_pointer_ntoh() const noexcept {
        return BE16(18);
    }

    /**
     * The payload, starting at the end of the header (Data Offset).
     *
     * Precondition: valid().
     */
    std::span<const uint8_t> payload() const noexcept {
        return bytes_.subspan(header_length());
    }

    /**
     * Parses a TCP segment from raw bytes.
     *
     * Returns std::nullopt unless valid().
     */
    static std::optional<TcpPacketView> Parse(
        std::span<const uint8_t> bytes) noexcept {
        TcpPacketView view{bytes};
        if (!view.valid()) {
            return std::nullopt;
        }
        return view;
    }

    friend std::ostream& operator<<(
        std::ostream& os,
        const TcpPacketView& segment
    );

private:
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

std::ostream& operator<<(std::ostream& os, const TcpPacketView& segment);

#endif // TCP_FROM_SCRATCH_TCPPACKETVIEW_H