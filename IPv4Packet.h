#ifndef TCP_FROM_SCRATCH_IPPACKET_H
#define TCP_FROM_SCRATCH_IPPACKET_H

#include <cstdint>
#include <optional>
#include <ostream>
#include <span>
#include "utils/Platform.h"
#include "utils/Checksum.h"

/** As per RFC 791 Documentation */
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
class
#ifndef _MSC_VER
__attribute__((packed))
#endif
IPv4Header {
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

    /*
     * Setters.
     *
     * These take host byte order values and store them in network
     * byte order, ready to be written to the wire.
     */

    /** Sets the IP version; preserves the header length. */
    void set_version(uint8_t version) noexcept {
        version_ihl_ = static_cast<uint8_t>(
            (version_ihl_ & 0x0F) | ((version & 0x0F) << 4)
        );
    }

    /** Sets the Internet Header Length, in 32-bit words. */
    void set_ihl(uint8_t ihl) noexcept {
        version_ihl_ = static_cast<uint8_t>(
            (version_ihl_ & 0xF0) | (ihl & 0x0F)
        );
    }

    /** Sets the header length in bytes; must be a multiple of 4. */
    void set_header_length(uint8_t bytes) noexcept {
        set_ihl(bytes / 4);
    }

    void set_type_of_service(uint8_t type_of_service) noexcept {
        type_of_service_ = type_of_service;
    }

    /** Sets Total Length in host byte order. */
    void set_total_length(uint16_t total_length) noexcept {
        total_length_ = htons(total_length);
    }

    /** Sets Identification in host byte order. */
    void set_identification(uint16_t identification) noexcept {
        identification_ = htons(identification);
    }

    /** Sets the 3-bit Flags field; preserves the fragment offset. */
    void set_flags(uint8_t flags) noexcept {
        const uint16_t word = ntohs(flags_fragment_offset_);
        flags_fragment_offset_ =
            htons(static_cast<uint16_t>((word & 0x1FFF)
                                        | ((flags & 0x7) << 13)));
    }

    /** Sets Fragment Offset in host byte order; preserves the flags. */
    void set_fragment_offset(uint16_t offset) noexcept {
        const uint16_t word = ntohs(flags_fragment_offset_);
        flags_fragment_offset_ =
            htons(static_cast<uint16_t>((word & 0xE000)
                                        | (offset & 0x1FFF)));
    }

    void set_ttl(uint8_t ttl) noexcept {
        ttl_ = ttl;
    }

    void set_protocol(uint8_t protocol) noexcept {
        protocol_ = protocol;
    }

    /** Sets the Checksum in host byte order. */
    void set_checksum(uint16_t checksum) noexcept {
        checksum_ = htons(checksum);
    }

    /** Sets the source address in host byte order. */
    void set_source_address(uint32_t address) noexcept {
        source_address_ = htonl(address);
    }

    /** Sets the source address from a dotted-quad string. */
    void set_source_address(const char* address) noexcept {
        inet_pton(AF_INET, address, &source_address_);
    }

    /** Sets the destination address in host byte order. */
    void set_destination_address(uint32_t address) noexcept {
        destination_address_ = htonl(address);
    }

    /** Sets the destination address from a dotted-quad string. */
    void set_destination_address(const char* address) noexcept {
        inet_pton(AF_INET, address, &destination_address_);
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
     * Computes the RFC 791 header checksum over this header.
     *
     * The checksum field is treated as zero for the computation, as
     * per the spec. Call set_checksum() with the result before
     * sending.
     */
    uint16_t compute_checksum() const noexcept;

    friend std::ostream& operator<<(
        std::ostream& os,
        const IPv4Header& header
    );
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

static_assert(sizeof(IPv4Header) == 20);


/** The IP packet structure */
struct IPv4Packet {
    /** The packet's header */
    IPv4Header header;

    /** The byte payload. Starts from the end of the IHL of the header. */
    std::span<const uint8_t> payload;

    inline size_t size() const noexcept {
        return sizeof(IPv4Header) + payload.size();
    }

    /**
     * Serializes this full datagram (header + payload) into network-order bytes.
     * Automatically updates total_length and compute_checksum before serializing.
     * Returns the number of bytes written, or 0 if out is too small.
     */
    [[nodiscard]] size_t write(std::span<uint8_t> out) const;
};

inline size_t WriteIPv4Packet(std::span<uint8_t> out, const IPv4Packet& packet) {
    return packet.write(out);
}

#endif // TCP_FROM_SCRATCH_IPPACKET_H