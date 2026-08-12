//
// Created by waytoounoriginal on 8/11/2026.
//

#ifndef TCP_FROM_SCRATCH_TCPPACKET_H
#define TCP_FROM_SCRATCH_TCPPACKET_H

#include <cstdint>
#include <ostream>
#include <span>
#include <arpa/inet.h>

/** As per RFC 793 Documentation */
class __attribute__((packed)) TcpHeader {
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

    friend std::ostream& operator<<(
        std::ostream& os,
        const TcpHeader& header
    );
};

static_assert(sizeof(TcpHeader) == 20);

/** The TCP segment structure */
struct TcpPacket {
    /** The segment's header */
    TcpHeader header;

    /** The byte payload. Starts from the end of the TCP header (Data
     * Offset). */
    std::span<uint8_t> payload;
};

#endif //TCP_FROM_SCRATCH_TCPPACKET_H