#ifndef DATATRANSFER_P2P_CONNECTOR_HPP
#define DATATRANSFER_P2P_CONNECTOR_HPP

#include <cstdio>
#include "serializer.hpp"
#include "deserializer.hpp"

namespace datatransfer {

template <typename mutex>
struct MutexLocker
{
private:
    mutex& _mtx;

public:
    explicit MutexLocker(mutex& mtx) : _mtx(mtx) { _mtx.lock(); }
    ~MutexLocker() { _mtx.unlock(); }
};

template<typename mutex,
         typename input_output_stream,
         typename serialization_policy,
         typename callback_handler_type>
class p2p_connector
{
    using write_policy = typename serialization_policy::template serialization<input_output_stream>::write_policy;
    using read_policy = typename serialization_policy::template serialization<input_output_stream>::read_policy;
    using checksum_policy = typename serialization_policy::template serialization<input_output_stream>::checksum_policy;
    using size_policy = typename serialization_policy::template serialization<input_output_stream>::size_policy;

    template<int N,
             int Count>
    struct SizeHelper
    {
        static size_t size(int id, uint8_t data[serialization_policy::MAX_MESSAGE_SIZE])
        {
            if (N == id)
            {
                using type = typename serialization_policy::template data<N>::type;

                type* t = reinterpret_cast<type*>(data);
                size_policy s;
                s.operate(*t);
                return s.size();
            }
            else
            {
                return SizeHelper<N+1, Count-1>::size(id, data);
            }
        }
    };

    template <int N>
    struct SizeHelper<N,0>
    {
        static size_t size(int id, uint8_t data[serialization_policy::MAX_MESSAGE_SIZE]) { return 0; }
    };

    template<int N,
             int Count>
    struct CallbackHelper
    {
        static void callback(const typename checksum_policy::data_type& checksum,
                             packet<uint8_t[serialization_policy::MAX_MESSAGE_SIZE], checksum_policy>& rx_packet,
                             callback_handler_type& message_handlers)
        {
            if (N == rx_packet.header.id)
            {
                using type = typename serialization_policy::template data<N>::type;

                packet<type, checksum_policy> decoded_packet(reinterpret_cast<type&>(rx_packet.data), rx_packet.header.id);

                if (checksum == decoded_packet.calculate_crc())
                {
                    message_handlers.template signal<N>(reinterpret_cast<const type&>(rx_packet.data));
                }
            }
            else
            {
                CallbackHelper<N+1,Count-1>::callback(checksum, rx_packet, message_handlers);
            }
        }
    };

    template<int N>
    struct CallbackHelper<N,0>
    {
        static void callback(const typename checksum_policy::data_type& checksum,
                             packet<uint8_t[serialization_policy::MAX_MESSAGE_SIZE], checksum_policy>& rx_packet,
                             callback_handler_type& message_handlers)
        {}
    };

    template<int N,
             int Count>
    struct DeserializeHelper
    {
        bool static deserializeType(int data_type, deserializer<read_policy>& deserializer, uint8_t* read_buffer, uint8_t c)
        {
            if (N == data_type )
            {
                if (serialization_policy::template data<N>::length > 0)
                {
                    return deserializer(reinterpret_cast<typename serialization_policy::template data<N>::type&>(*read_buffer));
                }
            }
            else
            {
                return DeserializeHelper<N+1,Count-1>::deserializeType(data_type, deserializer, read_buffer, c);
            }
        }
    };

    template<int N>
    struct DeserializeHelper<N, 0>
    {
        bool static deserializeType(uint8_t, deserializer<read_policy>&, uint8_t*, uint8_t)
        {
            // Invalid data type
            return false;
        }
    };

    enum parse_state
    {
        WAIT_FOR_SYNC_1,
        WAIT_FOR_SYNC_2,
        WAIT_FOR_ID,
        WAIT_FOR_DATA,
        WAIT_FOR_CRC
    };

protected:
    mutex _send_mutex;
    input_output_stream& _iostream;
    callback_handler_type _message_handlers;
    packet<uint8_t[serialization_policy::MAX_MESSAGE_SIZE], checksum_policy> _rx_packet;
    uint8_t _parse_buffer[serialization_policy::MAX_MESSAGE_SIZE];
    typename deserializer<read_policy>::input_stream _input_stream;
    uint8_t _payload_size;
    parse_state _parse_state;
    deserializer<read_policy> _deserializer;

public:
    p2p_connector(input_output_stream& stream)
        : _iostream(stream)
        , _rx_packet(_parse_buffer)
        , _parse_state(WAIT_FOR_SYNC_1)
        , _deserializer(_input_stream)
    {}

    ~p2p_connector() {}

    template<int T>
    void registerMessageHandler(typename callback_handler_type::template function_type<T> handler)
    {
        static_assert(serialization_policy::valid(T), "T is not a valid message type");

        _message_handlers.template registerHandler<T>(handler);
    }

    template<int T>
    void deregisterMessageHandler()
    {
        static_assert(serialization_policy::valid(T), "T is not a valid message type");

        //_message_handlers[T-1] = nullptr;
    }

    template<int T>
    void send(typename serialization_policy::template data<T>::type& data)
    {
        static_assert(serialization_policy::valid(T), "T is not a valid message type");
        using data_type = typename serialization_policy::template data<T>::type;

        MutexLocker<mutex> locker(_send_mutex);

        if (_iostream.good())
        {
            packet<data_type, checksum_policy> p(data, T);
            p.footer.checksum = p.calculate_crc();

            serializer<write_policy> s(_iostream);
            s(p);

            _iostream.flush();
        }
    }

    void readOnce()
    {
        if (_iostream.good())
        {
            int c;
            if ((c = _iostream.get()) >= 0)
            {
                processChar(c);
            }
        }
    }

    void read()
    {
        if (_iostream.good())
        {
            int c;
            while ((c = _iostream.get()) >= 0)
            {
                processChar(c);
            }
        }
    }

private:

    void processChar(int c)
    {
        switch (_parse_state)
        {
            case WAIT_FOR_SYNC_1:
                if (c == _rx_packet.header.SYNC_1)
                    _parse_state = WAIT_FOR_SYNC_2;
            break;
            case WAIT_FOR_SYNC_2:
                if (c == _rx_packet.header.SYNC_2)
                    _parse_state = WAIT_FOR_ID;
                else
                    _parse_state = WAIT_FOR_SYNC_1;
            break;
            case WAIT_FOR_ID:
            {
                if (!serialization_policy::valid(c))
                {
                    // Silently fail on error
                    _parse_state = WAIT_FOR_SYNC_1;
                }
                else
                {
                    _rx_packet.header.id = c;
                    _deserializer.reset();
                    SizeHelper<1, serialization_policy::NUMBER_OF_MESSAGES> helper;
                    _payload_size = helper.size(_rx_packet.header.id, _rx_packet.data);
                    _input_stream.clear();
                    if (_payload_size > serialization_policy::MAX_MESSAGE_SIZE)
                    {
                        _parse_state = WAIT_FOR_SYNC_1;
                    }
                    else
                    {
                        _parse_state = WAIT_FOR_DATA;
                    }
                }
            }
            break;
            case WAIT_FOR_DATA:
            {
                _input_stream.receive(c);
                if (_input_stream.size() == _payload_size)
                {
                    DeserializeHelper<1, serialization_policy::NUMBER_OF_MESSAGES> helper;
                    if (helper.deserializeType(_rx_packet.header.id, _deserializer, _parse_buffer, c))
                    {
                        _parse_state = WAIT_FOR_CRC;
                    }
                    else
                    {
                        _parse_state = WAIT_FOR_SYNC_1;
                    }
                }
            }
            break;
            case WAIT_FOR_CRC:
            {
                CallbackHelper<1, serialization_policy::NUMBER_OF_MESSAGES> helper;
                helper.callback(c, _rx_packet, _message_handlers);
                _parse_state = WAIT_FOR_SYNC_1;
            }
            break;
        }
    }
};
}
#endif // DATATRANSFER_P2P_CONNECTOR_HPP
