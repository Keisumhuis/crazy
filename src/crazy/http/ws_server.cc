#include "ws_server.h"

namespace crazy::http {

WSServer::WSServer() {
    m_dispatch.reset(new WSServletDispatch);
}
void WSServer::HandleClient(Socket::Ptr sock) {
    CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "handle ws clien";
    WSSession::Ptr session(new WSSession(sock));
    do {
        auto header = session->HandleShake();
        if (!header) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "handle shake fail";
            break;
        }
        auto servlet = m_dispatch->GetWSServlet(header->GetPath());
        if (!servlet) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "no match wsservlet";
            break;
        }
        servlet->OnConnect(header, session);
        while (true) {
            auto msg = session->RecvMessage();
            if(!msg) {
                break;
            }
            servlet->OnMessage(header, msg, session);
        }
        servlet->OnClose(header, session);
    } while (false);
    session->Close();
}
void WSServer::SetServletDispatch(WSServletDispatch::Ptr dispatch) {
    m_dispatch = dispatch;
}
WSServletDispatch::Ptr WSServer::GetServletDispatch() const {
    return m_dispatch;
}
}