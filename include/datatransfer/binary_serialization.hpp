#ifndef DATATRANSFER_BINARY_SERIALIZATION_HPP
#define DATATRANSFER_BINARY_SERIALIZATION_HPP

#include <Eigen/Core>

namespace datatransfer
{

struct binary_serialization
{
    template <typename output_stream>
    class write_policy
    {
    public:
        using stream_type = output_stream;
        output_stream& os;

        write_policy(output_stream& os) : os(os) {}

        template <typename T>
        write_policy<output_stream>& operator% (T& x)
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
            t.template method<write_policy<output_stream> >(os);
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
            // Assume little endian encoding
            os.write(reinterpret_cast<const typename output_stream::char_type*>(&t), sizeof(T));
        }
    };

    template <typename input_stream>
    struct read_policy
    {
        using stream_type = input_stream;
        input_stream& is;

        read_policy(input_stream& is) : is(is) {}

        template <typename T>
        read_policy& operator% (T& x)
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
            t.template method<read_policy<input_stream> >(is);
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
            // Assume little endian encoding
			const auto n = sizeof(T);
			return is.read(reinterpret_cast<typename input_stream::char_type*>(&t), n) == n;
        }
    };

    class checksum_policy
    {
    private:
        uint8_t& _checksum;

    public:
        using data_type = uint8_t;
        using stream_type = data_type;

        checksum_policy(data_type& checksum)
            : _checksum(checksum)
        {
            _checksum = 0;
        }

        uint8_t checksum() const { return _checksum; }

        template <typename T>
        void operator% (T& x)
        {
            operate(x);
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

        template <typename T, int M=0, int N=0>
        void operate(T& t)
        {
            t.template method<checksum_policy>(_checksum);
        }

        void operate(float& x) 		{ read(x); }
        void operate(double& x) 	{ read(x); }
        void operate(char& x)   	{ read(x); }
        void operate(bool& x)   	{ read(x); }
        void operate(uint8_t& x)    { read(x); }
        void operate(uint64_t& x)	{ read(x); }

        template <typename T>
        void read(T& t)
        {
            // Assume little endian encoding
            auto* buf = reinterpret_cast<const data_type*>(&t);
            for (int i = 0; i < sizeof(T); ++i)
                _checksum ^= buf[i];
        }
    };
};

}

#endif // DATATRANSFER_BINARY_SERIALIZATION_HPP
