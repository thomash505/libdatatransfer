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

struct packet_footer
{
	using checksum_type = uint8_t;
	checksum_type checksum;

	template <typename policy>
	void method(typename policy::stream_type& stream)
	{
		policy p(stream);
		p % checksum;
	}
};

template<typename T>
struct packet
{
	packet_header header;
	T& data;
	packet_footer footer;

	packet(T& t, const uint8_t id = 0)
		: header(sizeof(T), id)
		, data(t)
	{}

	template <typename policy>
	void method(typename policy::stream_type& stream)
	{
		policy p(stream);
		p % header;
		p % data;
		p % footer;
	}

	packet_footer::checksum_type calculate_crc() const
	{
		packet_footer::checksum_type ret = 0;

		ret ^= header.id;
		ret ^= header.deserialized_size;
		for (int i = 0; i < header.deserialized_size; ++i)
		{
			ret ^= reinterpret_cast<char*>(&data)[i];
		}

		return ret;
	}
};

}

#endif // DATATRANSFER_PACKET_TYPES_H

