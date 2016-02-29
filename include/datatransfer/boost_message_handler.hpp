#ifndef DATATRANSFER_BOOST_MESSAGE_HANDLER_HPP
#define DATATRANSFER_BOOST_MESSAGE_HANDLER_HPP

#include <boost/signals2/signal.hpp>
#include "message_handler_base.hpp"

namespace datatransfer {

template<typename DataType>
struct boost_message_handler : message_handler_base
{
    boost::signals2::signal<void(DataType&)> message_signal;

    virtual void signal(char* data)
    {
        message_signal(reinterpret_cast<DataType&>(*data));
    }
};

}

#endif // DATATRANSFER_BOOST_MESSAGE_HANDLER_HPP
