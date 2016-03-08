#ifndef DATATRANSFER_PACKET_TYPES_H
#define DATATRANSFER_PACKET_TYPES_H

#include <stdint.h>

namespace datatransfer {

struct packet_header
{
    uint8_t SYNC_1;
    uint8_t SYNC_2;
    uint8_t id;
    uint8_t deserialized_size;

    packet_header(const uint8_t size, const uint8_t id)
        : SYNC_1(0x55)
        , SYNC_2(0xAA)
        , id(id)
        , deserialized_size(size)
    {}

    template <typename policy>
    void method(typename policy::stream_type& stream)
    {
        policy p(stream);
        p % SYNC_1;
        p % SYNC_2;
        p % id;
        p % deserialized_size;
    }
};

template <typename checksum_type>
struct packet_footer
{
    checksum_type checksum;

    template <typename policy>
    void method(typename policy::stream_type& stream)
    {
        policy p(stream);
        p % checksum;
    }
};

template<typename T, typename checksum_policy>
struct packet
{
    packet_header header;
    T& data;
    packet_footer<typename checksum_policy::data_type> footer;

    packet(T& t, const uint8_t id = 0)
        : header(sizeof(T), id)
        , data(t)
    {
    }

    template <typename policy>
    void method(typename policy::stream_type& stream)
    {
        policy p(stream);
        p % header;
        p % data;
        p % footer;
    }

    typename checksum_policy::data_type calculate_crc()
    {
        checksum_policy p;

        p % header.id;
        p % header.deserialized_size;
        p % data;

        return p.checksum();
    }
};

}

#endif // DATATRANSFER_PACKET_TYPES_H

