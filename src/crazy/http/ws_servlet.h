/**
 * @file ws_servlet.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_HTTP_WS_SERVLET_H____
#define ____CRAZY_HTTP_WS_SERVLET_H____

#include <functional>
#include <memory>

#include "request.h"
#include "servlet.h"
#include "ws_session.h"

namespace crazy::http {

    class WSServlet : public Servlet {
    public:
        using Ptr = std::shared_ptr<WSServlet>;
        WSServlet(const std::string& name)
            :Servlet(name) {
        }
        ~WSServlet() {}
        virtual void Handle(Request::Ptr req, Response::Ptr res) override {}
        virtual void OnConnect(Request::Ptr, WSSession::Ptr) = 0;
        virtual void OnClose(Request::Ptr, WSSession::Ptr) = 0;
        virtual void OnMessage(Request::Ptr, WSFrameMessage::Ptr, WSSession::Ptr) = 0;
    };

    class FunctionWSServlet : public WSServlet {
    public:
        using Ptr = std::shared_ptr<FunctionWSServlet>;
        using on_connect_cb = std::function<void(Request::Ptr, WSSession::Ptr)>;
        using on_close_cb = std::function<void(Request::Ptr, WSSession::Ptr)>;
        using on_message_cb = std::function<void(Request::Ptr, WSFrameMessage::Ptr, WSSession::Ptr)>;
        
        FunctionWSServlet(on_connect_cb connect_cb, on_close_cb close_cb, on_message_cb message_cb);
        virtual void OnConnect(Request::Ptr req, WSSession::Ptr session) override;
        virtual void OnClose(Request::Ptr req, WSSession::Ptr session) override;
        virtual void OnMessage(Request::Ptr req, WSFrameMessage::Ptr msg, WSSession::Ptr session) override;
    protected:
        on_connect_cb m_connect;
        on_close_cb m_close;
        on_message_cb m_message;
    };

    class WSServletDispatch : public ServletDispatch {
    public:
        using Ptr = std::shared_ptr<WSServletDispatch>;
        WSServletDispatch() {}
        void AddServlet(const std::string& uri
                    ,FunctionWSServlet::on_connect_cb connect_cb
                    ,FunctionWSServlet::on_close_cb close_cb
                    ,FunctionWSServlet::on_message_cb message_cb);
        void AddGlobServlet(const std::string& uri
                    ,FunctionWSServlet::on_connect_cb connect_cb
                    ,FunctionWSServlet::on_close_cb close_cb
                    ,FunctionWSServlet::on_message_cb message_cb);
        WSServlet::Ptr GetWSServlet(const std::string& uri);
    };

}

#endif // ! ____CRAZY_HTTP_WS_SERVLET_H____