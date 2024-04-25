/**
 * @file http_server.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_HTTP_HTTP_SERVER_H____
#define ____CRAZY_HTTP_HTTP_SERVER_H____

#include <memory>

#include "crazy/http/http_session.h"
#include "crazy/http/servlet.h"
#include "crazy/logger.h"
#include "crazy/tcp_server.h"

namespace crazy::http {

    class HttpServer : public TcpServer {
    public:
	    using Ptr = std::shared_ptr<HttpServer>;
	    HttpServer();
	    virtual void HandleClient(Socket::Ptr sock) override;
	    ServletDispatch::Ptr GetServletDispatch() const;
	    void SetServletDispatch(ServletDispatch::Ptr dispatch);
	    virtual void SetName(const std::string& name);
	    void SetKeepAlive(bool keepalive);
	    bool GetKeepAlive() const;
    private:
	    bool m_isKeepAlive = true;
	    ServletDispatch::Ptr m_dispatch;
    };

}

#endif // ! ____CRAZY_HTTP_HTTP_SERVER_H____
