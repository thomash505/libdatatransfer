#ifndef DATATRANSFER_SERIALIZER_HPP
#define DATATRANSFER_SERIALIZER_HPP

namespace datatransfer {

template <typename write_policy>
struct serializer
{
	using output_stream = typename write_policy::stream_type;
	output_stream& _os;
	write_policy _write_policy;

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
