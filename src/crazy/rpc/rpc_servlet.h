/**
 * @file rpc_servlet.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_RPC_RPC_SERVLRT_H____
#define ____CRAZY_RPC_RPC_SERVLRT_H____

#include <functional>
#include <map>
#include <memory>

#include "crazy/byte_array.h"

namespace crazy::rpc {

    class RpcServlet {
    public:
        using Ptr = std::shared_ptr<RpcServlet>;
    };
    
    class FunctionRpcServlet : public RpcServlet {
    public:
        using Ptr = std::shared_ptr<FunctionRpcServlet>;
        template <typename Func>
        void Bind(Func&& function);
        template <typename Func, typename Self>
        void Bind(Func&& function, Self* selfptr);
        void Handle(byte_array::Serialize& serialize, byte_array::Deserialize& deserialize);
    private:
        template <typename Func>
        void CallProxy(Func function, byte_array::Serialize& serialize, byte_array::Deserialize& deserialize);
        template <typename Func, typename Self>
        void CallProxy(Func function, Self* selfptr, byte_array::Serialize& serialize, byte_array::Deserialize& deserialize);
        template<typename Return, typename... Params>
	    void Callproxy_(Return(*function)(Params...), byte_array::Serialize& serialize, byte_array::Deserialize& deserialize);
        template<typename Return, typename Class, typename S, typename... Params>
	    void Callproxy_(Return(Class::* function)(Params...), S* s, byte_array::Serialize& serialize, byte_array::Deserialize& deserialize);
        template<typename Return, typename... Params>
	    void Callproxy_(std::function<Return(Params... ps)> func, byte_array::Serialize& serialize, byte_array::Deserialize& deserialize);
    private:
        std::function<void (const char&, size_t)> m_func;
    };

    class RpcServletDispatch : public RpcServlet {
    public:
        using Ptr = std::shared_ptr<RpcServletDispatch>;
    private:
        std::map<std::string, FunctionRpcServlet::Ptr> m_servlets;
    };

    template <typename Func>
    void FunctionRpcServlet::Bind(Func&& function) {
        m_func = std::bind(&FunctionRpcServlet::CallProxy, this, function, std::placeholders::_1, std::placeholders::_2);
    }
    template <typename Func, typename Self>
    void FunctionRpcServlet::Bind(Func&& function, Self* selfptr) {
        m_func = std::bind(&FunctionRpcServlet::CallProxy, this, function, selfptr, std::placeholders::_1, std::placeholders::_2);
    }
    template <typename Func>
    void FunctionRpcServlet::CallProxy(Func function, byte_array::Serialize& serialize, byte_array::Deserialize& deserialize) {
        Callproxy_(function, serialize, deserialize);
    }
    template <typename Func, typename Self>
    void FunctionRpcServlet::CallProxy(Func function, Self* selfptr, byte_array::Serialize& serialize, byte_array::Deserialize& deserialize) {
        Callproxy_(function, selfptr, serialize, deserialize);
    }
    template<typename Return, typename... Params>
	void FunctionRpcServlet::Callproxy_(Return(*function)(Params...), byte_array::Serialize& serialize, byte_array::Deserialize& deserialize) {
        Callproxy_(std::function<Return(Params...)>(function), serialize, deserialize);
    }
    template<typename Return, typename Class, typename S, typename... Params>
	void FunctionRpcServlet::Callproxy_(Return(Class::* function)(Params...), S* s, byte_array::Serialize& serialize, byte_array::Deserialize& deserialize) {
        using args_type = std::tuple<typename std::decay<Params>::type...>;
        constexpr auto N = std::tuple_size<typename std::decay<args_type>::type>::value;
        args_type args = deserialize.get_tuple < args_type >(std::make_index_sequence<N>{});
    }
    template<typename Return, typename... Params>
	void FunctionRpcServlet::Callproxy_(std::function<Return(Params... ps)> func, byte_array::Serialize& serialize, byte_array::Deserialize& deserialize) {
        using args_type = std::tuple<typename std::decay<Params>::type...>;
        constexpr auto N = std::tuple_size<typename std::decay<args_type>::type>::value;
        args_type args = deserialize.get_tuple < args_type >(std::make_index_sequence<N>{});
    }
    
}

#endif // ! ____CRAZY_RPC_RPC_SERVLRT_H____