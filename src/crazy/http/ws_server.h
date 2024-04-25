/**
 * @file ws_server.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_HTTP_WS_SERVER_H____
#define ____CRAZY_HTTP_WS_SERVER_H____

#include <memory>

#include "crazy/endian.h"
#include "crazy/logger.h"
#include "crazy/tcp_server.h"
#include "ws_servlet.h"
#include "ws_session.h"

namespace crazy::http {

    class WSServer : public TcpServer {
    public:
        using Ptr = std::shared_ptr<WSServer>;
        WSServer();
        virtual void HandleClient(Socket::Ptr sock) override;
        void SetServletDispatch(WSServletDispatch::Ptr dispatch);
        WSServletDispatch::Ptr GetServletDispatch() const;
    private:
        WSServletDispatch::Ptr m_dispatch;
    };

}

#endif // ! ____CRAZY_HTTP_WS_SERVER_H____