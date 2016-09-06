#ifndef DATATRANSFER_STD_FUNCTION_CALLBACK_HANDLER_HPP
#define DATATRANSFER_STD_FUNCTION_CALLBACK_HANDLER_HPP

#include <functional>

namespace datatransfer {

template <typename serialization_policy>
class std_function_callback_handler
{
public:
    template <int N>
    using function_type = std::function<void(const typename serialization_policy::template data<N>::type&)>;

    template <int N>
    void signal(const typename serialization_policy::template data<N>::type& t)
    {
        auto func = RetrieveHelper<N,1,serialization_policy::NUMBER_OF_MESSAGES>::retrieve(_callbacks);

        if (func != nullptr) func(t);
    }

    template <int N>
    void registerHandler(function_type<N> func)
    {
        RetrieveHelper<N,1,serialization_policy::NUMBER_OF_MESSAGES>::retrieve(_callbacks) = func;
    }

protected:
    template <int N, int count>
    struct FunctionHelper
    {
        FunctionHelper()
            : first(nullptr)
        {}

        function_type<N> first;
        FunctionHelper<N+1, count-1> second;
    };

    template <int N>
    struct FunctionHelper<N, 0> { };

    template <int id, int N, int count>
    struct RetrieveHelper
    {
        static constexpr function_type<id>& retrieve(FunctionHelper<N, count>& helper)
        {
            return RetrieveHelper<id,N+1,count-1>::retrieve(helper.second);
        }
    };

    template <int id, int count>
    struct RetrieveHelper<id, id, count>
    {
        static constexpr function_type<id>& retrieve(FunctionHelper<id, count>& helper)
        {
            return helper.first;
        }
    };

private:
    FunctionHelper<1, serialization_policy::NUMBER_OF_MESSAGES> _callbacks;
};

}

#endif // DATATRANSFER_STD_FUNCTION_CALLBACK_HANDLER_HPP
