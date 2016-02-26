#ifndef DATATRANSFER_SERIALIZATIONREADPOLICY_HPP
#define DATATRANSFER_SERIALIZATIONREADPOLICY_HPP

#include <Eigen/Core>
#include <boost/detail/endian.hpp>

namespace datatransfer
{

template <typename input_stream>
struct serialization_read_policy
{
	using stream_type = input_stream;
	input_stream& is;

	serialization_read_policy(input_stream& is) : is(is) {}

    template <typename T>
	serialization_read_policy& operator% (T& x)
    {
        operate(x);

        return *this;
    }

protected:
    template <typename Scalar, int M, int N>
    void operate(Eigen::Matrix<Scalar, M, N>& matrix)
    {
        for (int j = 0; j < N; j++)
        {
            for (int i = 0; i < M; i++)
            {
                operate(matrix(i,j));
            }
        }
    }

public:
    template <typename T, int M=0, int N=0>
    void operate(T& t)
    {
		t.template method<serialization_read_policy<input_stream> >(is);
    }

	bool operate(int& x) 		{ return read(x); }
	bool operate(float& x) 		{ return read(x); }
	bool operate(double& x) 	{ return read(x); }
	bool operate(char& x)   	{ return read(x); }
	bool operate(bool& x)   	{ return read(x); }
	bool operate(uint64_t& x)	{ return read(x); }

    template <typename T>
	bool read(T& t)
    {
        char* buf = reinterpret_cast<char*>(&t);
#ifdef BOOST_LITTLE_ENDIAN
        is.read(buf, sizeof(T));
		return true;
#elif BOOST_BIG_ENDIAN
#error <Serialization> big endian is not currently supported...
#endif
    }
};

}

#endif // DATATRANSFER_SERIALIZATIONREADPOLICY_HPP
