#include "ws_servlet.h"

namespace crazy::http {

FunctionWSServlet::FunctionWSServlet(on_connect_cb connect_cb, on_close_cb close_cb, on_message_cb message_cb) 
    : m_connect (connect_cb) 
    , m_close (close_cb)
    , m_message (message_cb) 
    , WSServlet ("FunctionWSServlet") {
}
void FunctionWSServlet::OnConnect(Request::Ptr req, WSSession::Ptr session) {
    if (m_connect) {
        return m_connect(req, session);
    }
}
void FunctionWSServlet::OnClose(Request::Ptr req, WSSession::Ptr session) {
    if (m_close) {
        return m_close(req, session);
    }
}
void FunctionWSServlet::OnMessage(Request::Ptr req, WSFrameMessage::Ptr msg, WSSession::Ptr session) {
    if (m_message) {
        return m_message(req, msg, session);
    }
}

void WSServletDispatch::AddServlet(const std::string& uri
                    ,FunctionWSServlet::on_connect_cb connect_cb
                    ,FunctionWSServlet::on_close_cb close_cb
                    ,FunctionWSServlet::on_message_cb message_cb) {
    ServletDispatch::AddServlet(uri, std::make_shared<FunctionWSServlet>(connect_cb, close_cb, message_cb));
}
void WSServletDispatch::AddGlobServlet(const std::string& uri
                    ,FunctionWSServlet::on_connect_cb connect_cb
                    ,FunctionWSServlet::on_close_cb close_cb
                    ,FunctionWSServlet::on_message_cb message_cb) {
    ServletDispatch::AddGlobServlet(uri, std::make_shared<FunctionWSServlet>(connect_cb, close_cb, message_cb));
}
WSServlet::Ptr WSServletDispatch::GetWSServlet(const std::string& uri) {
    auto slt = GetMatchedServlet(uri);
    return std::dynamic_pointer_cast<WSServlet>(slt);
}

};