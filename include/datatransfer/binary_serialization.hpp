#ifndef DATATRANSFER_BINARY_SERIALIZATION_HPP
#define DATATRANSFER_BINARY_SERIALIZATION_HPP

#include <Eigen/Core>

namespace datatransfer
{

struct binary_serialization
{
    template <typename output_stream>
    class write_policy_base
    {
    public:
        using stream_type = output_stream;
        using return_type = void;

    protected:
        write_policy_base(output_stream& os)
            : _os(os)
        {}

        template <typename T>
        return_type action(const T& t)
        {
            // Assume little endian encoding
            _os.write(reinterpret_cast<const typename output_stream::char_type*>(&t), sizeof(T));
        }

    private:
        output_stream& _os;
    };

    template <typename input_stream>
    class read_policy_base
    {
    public:
        using stream_type = input_stream;
        using return_type = bool;

    protected:
        read_policy_base(input_stream& is)
            : _is(is)
        {}

        template <typename T>
        return_type action(T& t)
        {
            // Assume little endian encoding
            const int n = sizeof(T);
            auto buf = reinterpret_cast<typename input_stream::char_type*>(&t);
            return _is.read(buf, n) == n;
        }

    private:
        input_stream& _is;
    };

    class checksum_policy_base
    {
    public:
        using data_type = uint8_t;
        using return_type = void;

    protected:
        checksum_policy_base(data_type& checksum)
            : _checksum(checksum)
        {
            _checksum = 0;
        }

        template <typename T>
        return_type action(T& t)
        {
            // Assume little endian encoding
            auto* buf = reinterpret_cast<const data_type*>(&t);
            for (int i = 0; i < sizeof(T); ++i)
                _checksum ^= buf[i];
        }

    public:
        data_type checksum() const { return _checksum; }

    private:
        data_type& _checksum;
    };

    class size_policy_base
    {
    public:
        using data_type = size_t;
        using return_type = void;

    protected:
        size_policy_base()
            : _size(0)
        {}

        template <typename T>
        return_type action(T& t) { _size += sizeof(T); }

    public:
        data_type size() const { return _size; }

    private:
        data_type _size;
    };

    template <typename policy, typename ...Args>
    class primitives : public policy
    {
    public:
        primitives(Args ...args)
            : policy(args...)
        {}

        template <typename T>
        primitives<policy, Args...>& operator% (T& x)
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

        template <typename Scalar, int N>
        void operate(Eigen::Matrix<Scalar, Eigen::Dynamic, N>& matrix) {}

        template <typename Scalar, int M>
        void operate(Eigen::Matrix<Scalar, M, Eigen::Dynamic>& matrix) {}

        template <typename Scalar>
        void operate(Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>& matrix) {}

        template <typename T>
        void operate(T& t)
        {
            t.template method(*this);
        }

        void operate(int& x)			{ policy::action(x); }
        void operate(float& x)			{ policy::action(x); }
        void operate(double& x)			{ policy::action(x); }
        void operate(char& x)			{ policy::action(x); }
        void operate(uint8_t& x)		{ policy::action(x); }
        void operate(bool& x)			{ policy::action(x); }
        void operate(uint64_t& x)		{ policy::action(x); }
    };

    template <typename output_stream>
    using write_policy = primitives<write_policy_base<output_stream>, output_stream&>;

    template <typename input_stream>
    using read_policy = primitives<read_policy_base<input_stream>, input_stream&>;

    using checksum_policy = primitives<checksum_policy_base, checksum_policy_base::data_type&>;
};

}

#endif // DATATRANSFER_BINARY_SERIALIZATION_HPP
