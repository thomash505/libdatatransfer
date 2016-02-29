#ifndef DATATRANSFER_MESSAGE_HANDLER_BASE_HPP
#define DATATRANSFER_MESSAGE_HANDLER_BASE_HPP

namespace datatransfer {

struct message_handler_base
{
    virtual void signal(char*) {}
};

}

#endif // DATATRANSFER_MESSAGE_HANDLER_BASE_HPP
