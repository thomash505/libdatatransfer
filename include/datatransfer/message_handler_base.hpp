#ifndef DATATRANSFER_MESSAGE_HANDLER_BASE_HPP
#define DATATRANSFER_MESSAGE_HANDLER_BASE_HPP

namespace datatransfer {

struct message_handler_base
{
	virtual void signal(const void*) = 0;
};

}

#endif // DATATRANSFER_MESSAGE_HANDLER_BASE_HPP
