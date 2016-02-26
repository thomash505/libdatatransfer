#ifndef DATATRANSFER_SERIALIZATIONWRITEPOLICY_HPP
#define DATATRANSFER_SERIALIZATIONWRITEPOLICY_HPP

#include <Eigen/Core>
#include <boost/detail/endian.hpp>

namespace datatransfer
{

template <typename output_stream>
class serialization_write_policy
{
public:
	using stream_type = output_stream;
	output_stream& os;

	serialization_write_policy(output_stream& os) : os(os) {}

    template <typename T>
	serialization_write_policy<output_stream>& operator% (T& x)
    {
		operate(x);

        return *this;
    }

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

    template <typename T, int M=0, int N=0>
	void operate(T& t)
    {
		t.template method<serialization_write_policy<output_stream> >(os);
    }

protected:
	void operate(int& x)			{ write(x); }
	void operate(float& x)			{ write(x); }
	void operate(double& x)			{ write(x); }
	void operate(char& x)			{ write(x); }
	void operate(uint8_t& x)		{ write(x); }
	void operate(bool& x)			{ write(x); }
	void operate(uint64_t& x)		{ write(x); }

private:
    template <typename T>
    void write(const T& t)
    {
        const char* buf = reinterpret_cast<const char*>(&t);
#ifdef BOOST_LITTLE_ENDIAN
        os.write(buf, sizeof(T));
#elif BOOST_BIG_ENDIAN
#error <Serialization> big endian is not currently supported...
#endif
    }
};

}

#endif // DATATRANSFER_SERIALIZATIONWRITEPOLICY_HPP
