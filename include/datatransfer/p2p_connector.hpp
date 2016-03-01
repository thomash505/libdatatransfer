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
		 typename serialization_traits,
		 unsigned int MAX_MESSAGE_SIZE=1024>
class p2p_connector
		: wait_policy
{
	using MessageHandlerPtr = std::shared_ptr<message_handler_base>;
	using MessageHandlerArray = MessageHandlerPtr[serialization_traits::NUMBER_OF_MESSAGES];
	using write_policy = typename serialization_traits::template serialization<input_output_stream>::write_policy;
	using read_policy = typename serialization_traits::template serialization<input_output_stream>::read_policy;

	template<int N,
			 int Count>
	struct DeserializeHelper
	{
		void static deserializeType(int data_type, deserializer<read_policy>& deserializer, char* read_buffer)
		{
			if (N == data_type )
			{
				deserializer(reinterpret_cast<typename serialization_traits::template data<N>::type&>(*read_buffer));
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
		void static deserializeType(uint8_t, deserializer<read_policy>&, char*) { }
	};

	enum parse_state
	{
		WAIT_FOR_SYNC_1,
		WAIT_FOR_SYNC_2,
		WAIT_FOR_ID,
		WAIT_FOR_SIZE,
		WAIT_FOR_DATA,
		WAIT_FOR_CRC
	};

protected:
	mutex _send_mutex;
	bool _stop_thread;
	unsigned int _wait_ms;
	input_output_stream& _iostream;
	MessageHandlerArray _message_handlers;
	thread _read_thread;
	packet<char[MAX_MESSAGE_SIZE]> _rx_packet;
	char _read_buffer[MAX_MESSAGE_SIZE];
	parse_state _parse_state;

public:
	p2p_connector(input_output_stream& stream,
				  unsigned int waitMs=5)
		: _stop_thread(false)
		, _wait_ms(waitMs)
		, _iostream(stream)
		, _read_thread(std::bind(&p2p_connector::run_read, this))
		, _rx_packet(_read_buffer)
		, _parse_state(WAIT_FOR_SYNC_1)
	{}

	~p2p_connector()
	{
		_stop_thread = true;
		_read_thread.join();
	}

	template<int T>
	void registerMessageHandler(std::shared_ptr<message_handler_base> handler)
	{
		static_assert((T > 0) && (T <= serialization_traits::NUMBER_OF_MESSAGES), "T is not a valid message type");

		_message_handlers[T-1] = handler;
	}

	template<int T>
	void send(typename serialization_traits::template data<T>::type& data)
	{
		static_assert((T > 0) && (T <= serialization_traits::NUMBER_OF_MESSAGES), "T is not a valid message type");
		using data_type = typename serialization_traits::template data<T>::type;

		MutexLocker<mutex> locker(_send_mutex);

		if (_iostream.good())
		{
			packet<data_type> p(data, T);
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
							if (!serialization_traits::valid(c))
							{
								// Silently fail on error
								_parse_state = WAIT_FOR_SYNC_1;
							}

							_rx_packet.header.id = c;
							_parse_state = WAIT_FOR_SIZE;
						break;
						case WAIT_FOR_SIZE:
							_rx_packet.header.deserialized_size = c;
							if (_rx_packet.header.deserialized_size > 0)
								_parse_state = WAIT_FOR_DATA;
							else
								_parse_state = WAIT_FOR_CRC;
						break;
						case WAIT_FOR_DATA:
						{
							_iostream.ungetc();
							deserializer<read_policy> d(_iostream);

							DeserializeHelper<1, serialization_traits::NUMBER_OF_MESSAGES> helper;
							helper.deserializeType(_rx_packet.header.id, d, _read_buffer);

							_parse_state = WAIT_FOR_CRC;
						}
						break;
						case WAIT_FOR_CRC:
							if (c == _rx_packet.calculate_crc())
							{
								auto id = _rx_packet.header.id-1;
								if (_message_handlers[id])
								{
									_message_handlers[id]->signal(_read_buffer);
								}
							}
							_parse_state = WAIT_FOR_SYNC_1;
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
