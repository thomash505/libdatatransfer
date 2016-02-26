#ifndef DATATRANSFER_DESERIALIZER_HPP
#define DATATRANSFER_DESERIALIZER_HPP

#include "packet_types.h"
#include "serialization_read_policy.hpp"

namespace datatransfer {

template <typename input_stream>
class deserializer
{
protected:
	input_stream& _is;
	serialization_read_policy<input_stream> _read_policy;

public:
	deserializer(input_stream& is)
		: _is(is)
		, _read_policy(is)
	{}

	template <typename T>
	void operator() (T &t)
	{
		_read_policy.operate(t);
	}
};

}

#endif // DATATRANSFER_DESERIALIZER_HPP

