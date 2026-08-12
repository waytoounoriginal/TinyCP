#ifndef TCP_FROM_SCRATCH_IPPACKET_H
#define TCP_FROM_SCRATCH_IPPACKET_H

#include <cstdint>
#include <optional>
#include <ostream>
#include <span>
#include <arpa/inet.h>

/** As per RFC 791 Documentation */
class __attribute__((packed)) IPv4Header {
private:
    /**
     * The first 4 bits are the Version field, which indicates the
     * format of the internet header.
     *
     * The last 4 bits are the Internet Header Length (IHL), which is
     * the length of the internet header in 32 bit words, and thus
     * points to the beginning of the data. Note that the minimum value
     * for a correct header is 5.
     */
    uint8_t version_ihl_;

    /**
     * The Type of Service provides an indication of the abstract
     * parameters of the quality of service desired. These parameters
     * are to be used to guide the selection of the actual service
     * parameters when transmitting a datagram through a particular
     * network.
     *
     * Bits 0-2: Precedence.
     * Bit 3:    0 = Normal Delay,      1 = Low Delay.
     * Bits 4:   0 = Normal Throughput, 1 = High Throughput.
     * Bits 5:   0 = Normal Reliability, 1 = High Reliability.
     * Bits 6-7: Reserved for Future Use.
     */
    uint8_t type_of_service_;

    /**
     * Total Length is the length of the datagram, measured in octets,
     * including internet header and data.
     */
    uint16_t total_length_;

    /**
     * An identifying value assigned by the sender to aid in assembling
     * the fragments of a datagram.
     */
    uint16_t identification_;

    /**
     * Various Control Flags and Fragment Offset.
     *
     * Bits 0-2: Flags.
     *
     * Bit 0: reserved, must be zero.
     * Bit 1: (DF) 0 = May Fragment, 1 = Don't Fragment.
     * Bit 2: (MF) 0 = Last Fragment, 1 = More Fragments.
     *
     * Bits 3-15: Fragment Offset.
     */
    uint16_t flags_fragment_offset_;

    /**
     * This field indicates the maximum time the datagram is allowed
     * to remain in the internet system.
     */
    uint8_t ttl_;

    /**
     * This field indicates the next level protocol used in the data
     * portion of the internet datagram.
     */
    uint8_t protocol_;

    /**
     * A checksum on the header only. Since some header fields change
     * (e.g., time to live), this is recomputed and verified at each
     * point that the internet header is processed.
     *
     * The checksum algorithm is:
     *
     * The checksum field is the 16 bit one's complement of the one's
     * complement sum of all 16 bit words in the header. For purposes of
     * computing the checksum, the value of the checksum field is zero.
     */
    uint16_t checksum_;

    /** The source address */
    uint32_t source_address_;

    /** The destination address */
    uint32_t destination_address_;

    /* Note for reader: option is omitted */

public:
    /*
     * Raw getters.
     *
     * These return the values exactly as they appear in the packet,
     * i.e. in network byte order for multi-byte fields.
     */

    uint8_t version_ihl() const noexcept {
        return version_ihl_;
    }

    uint8_t type_of_service() const noexcept {
        return type_of_service_;
    }

    uint16_t total_length() const noexcept {
        return total_length_;
    }

    uint16_t identification() const noexcept {
        return identification_;
    }

    uint16_t flags_fragment_offset() const noexcept {
        return flags_fragment_offset_;
    }

    uint8_t ttl() const noexcept {
        return ttl_;
    }

    uint8_t protocol() const noexcept {
        return protocol_;
    }

    uint16_t checksum() const noexcept {
        return checksum_;
    }

    uint32_t source_address() const noexcept {
        return source_address_;
    }

    uint32_t destination_address() const noexcept {
        return destination_address_;
    }

    /*
     * Interpreted / host-order getters.
     */

    /** Returns the IP version. */
    uint8_t version() const noexcept {
        return version_ihl_ >> 4;
    }

    /** Returns the Internet Header Length in 32-bit words. */
    uint8_t IHL() const noexcept {
        return version_ihl_ & 0x0F;
    }

    /** Returns the Internet Header Length in bytes. */
    uint8_t header_length() const noexcept {
        return IHL() * 4;
    }

    /** Returns Total Length in host byte order. */
    uint16_t total_length_ntoh() const noexcept {
        return ntohs(total_length_);
    }

    /** Returns Identification in host byte order. */
    uint16_t identification_ntoh() const noexcept {
        return ntohs(identification_);
    }

    /** Returns the 3-bit Flags field in host byte order. */
    uint8_t flags() const noexcept {
        return static_cast<uint8_t>(
            ntohs(flags_fragment_offset_) >> 13
        );
    }

    /** Returns Fragment Offset in host byte order. */
    uint16_t fragment_offset() const noexcept {
        return ntohs(flags_fragment_offset_) & 0x1FFF;
    }

    /** Returns Flags + Fragment Offset in host byte order. */
    uint16_t flags_fragment_offset_ntoh() const noexcept {
        return ntohs(flags_fragment_offset_);
    }

    /** Returns Checksum in host byte order. */
    uint16_t checksum_ntoh() const noexcept {
        return ntohs(checksum_);
    }

    /** Returns source address in host byte order. */
    uint32_t source_address_ntoh() const noexcept {
        return ntohl(source_address_);
    }

    /** Returns destination address in host byte order. */
    uint32_t destination_address_ntoh() const noexcept {
        return ntohl(destination_address_);
    }

    friend std::ostream& operator<<(
        std::ostream& os,
        const IPv4Header& header
    );
};

static_assert(sizeof(IPv4Header) == 20);


/** The IP packet structure */
struct IPv4Packet {
    /** The packet's header */
    IPv4Header header;

    /** The byte payload. Starts from the end of the header. */
    std::span<uint8_t> payload;
};

#endif // TCP_FROM_SCRATCH_IPPACKET_H