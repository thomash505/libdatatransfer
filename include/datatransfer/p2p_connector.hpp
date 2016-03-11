#ifndef DATATRANSFER_P2P_CONNECTOR_HPP
#define DATATRANSFER_P2P_CONNECTOR_HPP

#include <functional>
#include <memory>

#include "message_handler_base.hpp"
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
		 typename wait_policy,
		 typename thread,
                 typename serialization_policy,
		 unsigned int MAX_MESSAGE_SIZE=1024>
class p2p_connector
		: wait_policy
{
	using message_handler_callback = std::function<void(const void*)>;
        using message_handler_array = message_handler_callback[serialization_policy::NUMBER_OF_MESSAGES];
        using write_policy = typename serialization_policy::template serialization<input_output_stream>::write_policy;
        using read_policy = typename serialization_policy::template serialization<input_output_stream>::read_policy;
        using checksum_policy = typename serialization_policy::template serialization<input_output_stream>::checksum_policy;

    template<int N,
             int Count>
    struct CallbackHelper
    {
        static void callback(const typename checksum_policy::data_type& checksum,
                             packet<uint8_t[MAX_MESSAGE_SIZE], checksum_policy>& rx_packet,
                             const message_handler_array& message_handlers)
        {
            if (N == rx_packet.header.id)
            {
                using type = typename serialization_policy::template data<N>::type;

                packet<type, checksum_policy> decoded_packet(reinterpret_cast<type&>(rx_packet.data), rx_packet.header.id);

                if (checksum == decoded_packet.calculate_crc())
                {
                    auto id = rx_packet.header.id - 1;
                    if (message_handlers[id])
                    {
                        message_handlers[id](rx_packet.data);
                    }
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
                             packet<uint8_t[MAX_MESSAGE_SIZE], checksum_policy>& rx_packet,
                             const message_handler_array& message_handlers)
        {}
    };

	template<int N,
			 int Count>
	struct DeserializeHelper
	{
        void static deserializeType(int data_type, deserializer<read_policy>& deserializer, uint8_t* read_buffer)
		{
			if (N == data_type )
			{
                if (serialization_policy::template data<N>::length > 0)
                                deserializer(reinterpret_cast<typename serialization_policy::template data<N>::type&>(*read_buffer));
			}
			else
			{
				DeserializeHelper<N+1,Count-1>::deserializeType(data_type, deserializer,read_buffer);
			}
		}
	};

	template<int N>
	struct DeserializeHelper<N, 0>
	{
        void static deserializeType(uint8_t, deserializer<read_policy>&, uint8_t*) { }
	};

	enum parse_state
	{
		WAIT_FOR_SYNC_1,
		WAIT_FOR_SYNC_2,
		WAIT_FOR_ID,
		WAIT_FOR_CRC
	};

protected:
	mutex _send_mutex;
	bool _stop_thread;
	unsigned int _wait_ms;
	input_output_stream& _iostream;
	message_handler_array _message_handlers;
	thread _read_thread;
    packet<uint8_t[MAX_MESSAGE_SIZE], checksum_policy> _rx_packet;
    uint8_t _read_buffer[MAX_MESSAGE_SIZE];
	parse_state _parse_state;

public:
	p2p_connector(input_output_stream& stream,
				  unsigned int waitMs=5)
		: _stop_thread(false)
		, _wait_ms(waitMs)
		, _iostream(stream)
        , _read_thread([this] () { run_read(); })
        , _rx_packet(_read_buffer)
		, _parse_state(WAIT_FOR_SYNC_1)
	{}

	~p2p_connector()
	{
		_stop_thread = true;
		_read_thread.join();
	}

	template<int T>
	void registerMessageHandler(message_handler_callback handler)
	{
                static_assert(serialization_policy::valid(T), "T is not a valid message type");

		_message_handlers[T-1] = handler;
	}

    template<int T>
    void deregisterMessageHandler()
    {
        static_assert(serialization_policy::valid(T), "T is not a valid message type");

        _message_handlers[T-1] = nullptr;
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

protected:
	void run_read()
	{
		while(!_stop_thread)
		{
			if (_iostream.good())
			{
				int c;
				while (((c = _iostream.get()) >= 0) && !_stop_thread)
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
                                deserializer<read_policy> d(_iostream);

                                DeserializeHelper<1, serialization_policy::NUMBER_OF_MESSAGES> helper;
                                helper.deserializeType(_rx_packet.header.id, d, _read_buffer);

                                _parse_state = WAIT_FOR_CRC;
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
			}

			wait_policy::wait(_wait_ms);
		}
	}
};
}
#endif // DATATRANSFER_P2P_CONNECTOR_HPP
