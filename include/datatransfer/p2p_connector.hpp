#ifndef DATATRANSFER_P2P_CONNECTOR_HPP
#define DATATRANSFER_P2P_CONNECTOR_HPP

#include <functional>

#include <boost/ptr_container/ptr_map.hpp>
#include <boost/signals2.hpp>
#include <boost/signals2/signal_type.hpp>

#include "serializer.hpp"
#include "deserializer.hpp"

namespace datatransfer {

template <typename mutex>
struct MutexLocker
{
private:
	mutex& _mtx;

public:
	inline explicit MutexLocker(mutex& mtx) : _mtx(mtx) { _mtx.lock(); }
	inline ~MutexLocker() { _mtx.unlock(); }
};

struct MessageHandlerBase
{
    virtual void signal(char*) { }
};

typedef std::shared_ptr<MessageHandlerBase> MessageHandlerPtr;

template<typename DataType>
struct MessageHandler : MessageHandlerBase
{
    boost::signals2::signal<void(DataType&)> message_signal;

    virtual void signal(char* data)
    {
        message_signal(reinterpret_cast<DataType&>(*data));
    }
};

typedef boost::ptr_map<uint8_t,MessageHandlerPtr> MessageHandlerMap;


template<typename input_stream,
		 typename serialization_traits,
		 int N,
		 int Count>
struct DeserializeHelper
{
	void static deserializeType(int data_type, deserializer<input_stream>& deserializer, char* read_buffer)
	{
		if (N == data_type )
		{
			deserializer(reinterpret_cast<typename serialization_traits::template data<N>::type&>(*read_buffer));
		}
		else
		{
			DeserializeHelper<input_stream,serialization_traits,N+1,Count-1>::deserializeType(data_type, deserializer,read_buffer);
		}
	}
};

template<typename input_stream,
		 typename serialization_traits,
		 int N>
struct DeserializeHelper<input_stream, serialization_traits, N, 0>
{
	void static deserializeType(uint8_t, deserializer<input_stream>&, char*) { }
};

template<typename mutex,
		 typename input_stream,
		 typename output_stream,
		 typename wait_policy,
		 typename thread,
		 typename serialization_traits,
		 uint MAX_MESSAGE_SIZE=1024>
class p2p_connector
		: wait_policy
{
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
	mutex send_mutex;
    bool stop_thread;
    uint wait_ms;
	input_stream& _istream;
	output_stream& _ostream;
    MessageHandlerMap message_handlers;
	thread read_thread;
	packet _rx_packet;
    char read_buffer[MAX_MESSAGE_SIZE];
	parse_state _parse_state;

public:
	p2p_connector(input_stream& istream,
				 output_stream& ostream,
				 uint waitMs=5)
		: stop_thread(false)
        , wait_ms(waitMs)
		, _istream(istream)
		, _ostream(ostream)
		, read_thread(std::bind(&p2p_connector::run_read, this))
		, _rx_packet(read_buffer)
		, _parse_state(WAIT_FOR_SYNC_1)
    {
    }

	~p2p_connector()
    {
        stop_thread = true;
        read_thread.join();
    }

	template<int T>
	void registerMessageHandler(std::function<void(typename serialization_traits::template data<T>::type&)> handler)
	{
		static_assert(T != 0, "T Requires MessageType");

		if(message_handlers.count(T) == 0)
		{
			message_handlers[T].reset(new MessageHandler<typename serialization_traits::template data<T>::type>());
		}
		auto pHandle = std::static_pointer_cast<MessageHandler<typename serialization_traits::template data<T>::type>>(message_handlers[T]);
		pHandle->message_signal.connect(handler);
	}

    template<int T>
	void send(typename serialization_traits::template data<T>::type& data)
    {
		MutexLocker<mutex> locker(send_mutex);
    	static_assert(T != 0, "T Requires MessageType");

		if (_ostream.good())
		{
			try
			{
				packet_header header(T, sizeof(T));
				packet_footer footer;

				serializer<output_stream> s(_ostream);
				s(header);
				s(data);
				s(footer);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}
    }

protected:
    virtual void run_read()
    {
        while(!stop_thread)
        {
			if (_istream.good())
            {
                try
                {
					int c;
					if ((c = _istream.get()) >= 0)
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
									throw std::runtime_error("Data type not recognised for deserialisation.");

								_rx_packet.header.id = c;
								_parse_state = WAIT_FOR_SIZE;
							break;
							case WAIT_FOR_SIZE:
								_rx_packet.header.size = c;
								if (_rx_packet.header.size > 0)
									_parse_state = WAIT_FOR_DATA;
								else
									_parse_state = WAIT_FOR_CRC;
							break;
							case WAIT_FOR_DATA:
							{
								_istream.putback(c);
								deserializer<input_stream> d(_istream);

								DeserializeHelper<input_stream, serialization_traits, 1, serialization_traits::NUMBER_OF_MESSAGES> helper;
								helper.deserializeType(_rx_packet.header.id, d, read_buffer);

								_parse_state = WAIT_FOR_CRC;
							}
							break;
							case WAIT_FOR_CRC:
								if (message_handlers.count(_rx_packet.header.id) != 0)
								{
									message_handlers[_rx_packet.header.id]->signal(read_buffer);
								}
						}
					}
                }
                catch(std::exception& ex)
                {
                    std::cerr << ex.what() << std::endl;
                }
            }

			wait_policy::wait(wait_ms);
        }
    }
};
}
#endif // DATATRANSFER_P2P_CONNECTOR_HPP
