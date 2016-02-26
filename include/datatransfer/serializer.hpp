#ifndef DATATRANSFER_SERIALIZER_HPP
#define DATATRANSFER_SERIALIZER_HPP

#include "serialization_write_policy.hpp"

namespace datatransfer {

template <typename output_stream>
struct serializer
{
	output_stream& _os;
	serialization_write_policy<output_stream> _write_policy;

	serializer(output_stream& os)
		: _os(os)
		, _write_policy(_os)
    {}

    template <typename T>
	void operator() (T &t)
    {
		_write_policy.operate(t);
    }
};

}

#endif // DATATRANSFER_SERIALIZER_HPP
