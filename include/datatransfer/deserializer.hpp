#ifndef DATATRANSFER_DESERIALIZER_HPP
#define DATATRANSFER_DESERIALIZER_HPP

#include "packet_types.h"

namespace datatransfer {

template <typename read_policy>
class deserializer
{
protected:
	using input_stream = typename read_policy::stream_type;
	input_stream& _is;
	read_policy _read_policy;

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

