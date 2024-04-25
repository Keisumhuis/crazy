/**
 * @file rpc_server.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_RPC_RPC_SERVER_H____
#define ____CRAZY_RPC_RPC_SERVER_H____

#include <memory>
#include "crazy/tcp_server.h"

namespace crazy::rpc {

    class RpcServer : public TcpServer {
    public:
	    using Ptr = std::shared_ptr<RpcServer>;
	    virtual void HandleClient(Socket::Ptr sock) override;
    private:
    };
}

#endif // ! ____CRAZY_RPC_RPC_SERVER_H____