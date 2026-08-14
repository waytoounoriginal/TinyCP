//
// Created by waytoounoriginal on 8/11/2026.
//

#ifndef TCP_FROM_SCRATCH_TCPPACKET_H
#define TCP_FROM_SCRATCH_TCPPACKET_H

#include <cstdint>
#include <ostream>
#include <span>
#include "utils/Platform.h"
#include "utils/Checksum.h"

/** As per RFC 793 Documentation */
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
class
#ifndef _MSC_VER
__attribute__((packed))
#endif
TcpHeader {
private:

    /** The source port */
    uint16_t source_port_;

    /** The destination port */
    uint16_t dest_port_;

    /** The sequence number of the first data octet in this segment (except
    when SYN is present). If SYN is present the sequence number is the
    initial sequence number (ISN) and the first data octet is ISN+1. */
    uint32_t seq_num_;

    /** If the ACK control bit is set this field contains the value of the
    next sequence number the sender of the segment is expecting to
    receive.  Once a connection is established this is always sent. */
    uint32_t ack_num_;

    /**
     * Data offset: 4 bits
    *   The number of 32 bit words in the TCP Header.  This indicates where
        the data begins.  The TCP header (even one including options) is an
        integral number of 32 bits long.

     * Reserved: 6 bits: ALWAYS 0
     *
     * Control Bits: 6 bits
     *
        URG:  Urgent Pointer field significant
        ACK:  Acknowledgment field significant
        PSH:  Push Function
        RST:  Reset the connection
        SYN:  Synchronize sequence numbers
        FIN:  No more data from sender
     */
    uint16_t data_offset_and_reserved_and_control_bits_;

    /** The number of data octets beginning with the one indicated in the
    acknowledgment field which the sender of this segment is willing to
    accept. */
    uint16_t window_;

    /**
     * The checksum field is the 16 bit one's complement of the one's
     * complement sum of all 16 bit words in the segment, preceded by
     * the IPv4 pseudo-header. If a segment contains an odd number of
     * header and text octets to be checksummed, the last octet is
     * padded on the right with zeros to form a 16 bit word for checksum_
     * purposes. The pad is not transmitted as part of the segment.
     * While computing the checksum, the checksum field itself is
     * replaced with zeros.
     */
    uint16_t checksum_;

    /** This field communicates the current value of the urgent pointer as a
    positive offset from the sequence number in this segment.  The
    urgent pointer points to the sequence number of the octet following
    the urgent data.  This field is only be interpreted in segments with
    the URG control bit set. */
    uint16_t urgent_pointer_;

public:
    /*
     * Raw getters.
     *
     * These return the values exactly as they appear in the packet,
     * i.e. in network byte order for multi-byte fields.
     */

    uint16_t source_port() const noexcept {
        return source_port_;
    }

    uint16_t dest_port() const noexcept {
        return dest_port_;
    }

    uint32_t seq_num() const noexcept {
        return seq_num_;
    }

    uint32_t ack_num() const noexcept {
        return ack_num_;
    }

    uint16_t data_offset_and_reserved_and_control_bits() const noexcept {
        return data_offset_and_reserved_and_control_bits_;
    }

    uint16_t window() const noexcept {
        return window_;
    }

    uint16_t checksum() const noexcept {
        return checksum_;
    }

    uint16_t urgent_pointer() const noexcept {
        return urgent_pointer_;
    }

    /*
     * Interpreted / host-order getters.
     */

    /** Returns the source port in host byte order. */
    uint16_t source_port_ntoh() const noexcept {
        return ntohs(source_port_);
    }

    /** Returns the destination port in host byte order. */
    uint16_t dest_port_ntoh() const noexcept {
        return ntohs(dest_port_);
    }

    /** Returns the sequence number in host byte order. */
    uint32_t seq_num_ntoh() const noexcept {
        return ntohl(seq_num_);
    }

    /** Returns the acknowledgement number in host byte order. */
    uint32_t ack_num_ntoh() const noexcept {
        return ntohl(ack_num_);
    }

    /** Returns Data Offset + Reserved + Control Bits in host byte order. */
    uint16_t data_offset_and_reserved_and_control_bits_ntoh() const noexcept {
        return ntohs(data_offset_and_reserved_and_control_bits_);
    }

    /**
     * Returns the TCP Data Offset: the header length in
     * 32-bit words. The minimum value for a correct header is 5.
     */
    uint8_t data_offset() const noexcept {
        return static_cast<uint8_t>(
            ntohs(data_offset_and_reserved_and_control_bits_) >> 12
        );
    }

    /** Returns the TCP header length in bytes. */
    uint8_t header_length() const noexcept {
        return data_offset() * 4;
    }

    /** Returns the 6 control bits in host byte order. */
    uint16_t flags() const noexcept {
        return ntohs(data_offset_and_reserved_and_control_bits_) & 0x3F;
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
        return ntohs(window_);
    }

    /** Returns Checksum in host byte order. */
    uint16_t checksum_ntoh() const noexcept {
        return ntohs(checksum_);
    }

    /** Returns Urgent Pointer in host byte order. */
    uint16_t urgent_pointer_ntoh() const noexcept {
        return ntohs(urgent_pointer_);
    }

    /*
     * Setters.
     *
     * These take host byte order values and store them in network
     * byte order, ready to be written to the wire.
     */

    /** Sets the source port in host byte order. */
    void set_source_port(uint16_t port) noexcept {
        source_port_ = htons(port);
    }

    /** Sets the destination port in host byte order. */
    void set_dest_port(uint16_t port) noexcept {
        dest_port_ = htons(port);
    }

    /** Sets the sequence number in host byte order. */
    void set_seq_num(uint32_t seq) noexcept {
        seq_num_ = htonl(seq);
    }

    /** Sets the acknowledgement number in host byte order. */
    void set_ack_num(uint32_t ack) noexcept {
        ack_num_ = htonl(ack);
    }

    /** Sets the TCP Data Offset, in 32-bit words. */
    void set_data_offset(uint8_t words) noexcept {
        const uint16_t word = ntohs(data_offset_and_reserved_and_control_bits_);
        data_offset_and_reserved_and_control_bits_ =
            htons(static_cast<uint16_t>((word & 0x0FFF)
                                        | ((words & 0xF) << 12)));
    }

    /** Sets the header length in bytes; must be a multiple of 4. */
    void set_header_length(uint8_t bytes) noexcept {
        set_data_offset(bytes / 4);
    }

    /** Sets the 6 control bits at once, in host byte order. */
    void set_flags(uint16_t flags) noexcept {
        const uint16_t word = ntohs(data_offset_and_reserved_and_control_bits_);
        data_offset_and_reserved_and_control_bits_ =
            htons(static_cast<uint16_t>((word & 0xFFC0) | (flags & 0x3F)));
    }

private:
    /** Sets (or clears) a control bit by its position within the flags. */
    void set_flag(int bit, bool set) noexcept {
        const uint16_t word = ntohs(data_offset_and_reserved_and_control_bits_);
        const uint16_t mask = static_cast<uint16_t>(1u << bit);
        data_offset_and_reserved_and_control_bits_ = htons(set
            ? static_cast<uint16_t>(word | mask)
            : static_cast<uint16_t>(word & ~mask));
    }

public:
    void set_urg(bool set) noexcept { set_flag(5, set); }
    void set_ack(bool set) noexcept { set_flag(4, set); }
    void set_psh(bool set) noexcept { set_flag(3, set); }
    void set_rst(bool set) noexcept { set_flag(2, set); }
    void set_syn(bool set) noexcept { set_flag(1, set); }
    void set_fin(bool set) noexcept { set_flag(0, set); }

    /** Sets Window in host byte order. */
    void set_window(uint16_t window) noexcept {
        this->window_ = htons(window);
    }

    /** Sets the Checksum in host byte order. */
    void set_checksum(uint16_t checksum) noexcept {
        this->checksum_ = htons(checksum);
    }

    /** Sets Urgent Pointer in host byte order. */
    void set_urgent_pointer(uint16_t pointer) noexcept {
        urgent_pointer_ = htons(pointer);
    }

    /*
     * Wire serialization.
     */

    /**
     * Serializes the header into network-order wire bytes.
     *
     * Precondition: out.size() >= 20.
     */
    void serialize(std::span<uint8_t> out) const noexcept;

    /**
     * Computes the RFC 793 header checksum over this header.
     *
     * The checksum field is treated as zero for the computation.
     */
    uint16_t compute_checksum() const noexcept;

    /**
     * Computes the full RFC 793 TCP checksum over IPv4 pseudo-header,
     * TCP header, and payload.
     *
     * src_ip_net and dst_ip_net must be in network byte order.
     */
    uint16_t compute_checksum(uint32_t src_ip_net, uint32_t dst_ip_net,
                             std::span<const uint8_t> payload) const noexcept;

    friend std::ostream& operator<<(
        std::ostream& os,
        const TcpHeader& header
    );
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

static_assert(sizeof(TcpHeader) == 20);

/** The TCP segment structure */
struct TcpPacket {
    /** The segment's header */
    TcpHeader header;

    /** The byte payload. Starts from the end of the TCP header (Data
     * Offset). */
    std::span<const uint8_t> payload;

    inline size_t size() const noexcept {
        return sizeof(TcpHeader) + payload.size();
    }
};

/**
 * Serializes a full datagram (header + payload) into network-order
 * bytes, automatically computing the full RFC 793 TCP checksum.
 *
 * Returns the number of bytes written, or 0 if out is too small.
 */
[[nodiscard]] size_t WriteTcpPacket(std::span<uint8_t> out, uint32_t src_ip_net,
                                    uint32_t dst_ip_net, const TcpPacket& packet);

/**
 * Serializes a full datagram into network-order bytes using the existing header checksum.
 */
[[nodiscard]] size_t WriteTcpPacket(std::span<uint8_t> out, const TcpPacket& packet);

#endif //TCP_FROM_SCRATCH_TCPPACKET_H