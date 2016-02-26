#ifndef DATATRANSFER_PACKET_TYPES_H
#define DATATRANSFER_PACKET_TYPES_H

#include <stdint.h>

namespace datatransfer {

struct packet_header
{
	uint8_t SYNC_1;
	uint8_t SYNC_2;
	uint8_t id;
	uint8_t size;

	packet_header(const uint8_t id, const uint8_t size)
		: SYNC_1(0x55)
		, SYNC_2(0xAA)
		, id(id)
		, size(size)
	{}

	template <typename policy>
	void method(typename policy::stream_type& stream)
	{
		policy p(stream);
		p % SYNC_1;
		p % SYNC_2;
		p % id;
		p % size;
	}
};

struct packet_footer
{
	uint8_t checksum;

	template <typename policy>
	void method(typename policy::stream_type& stream)
	{
		policy p(stream);
		p % checksum;
	}
};

struct packet
{
	packet_header header;
	void* data;
	packet_footer footer;

	packet(void* data)
		: header(0, 0)
		, data(data)
	{}
};

}

#endif // DATATRANSFER_PACKET_TYPES_H

