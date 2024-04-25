#include "crazy.h"

void on_connect(crazy::http::Request::Ptr req, crazy::http::WSSession::Ptr session) {
    CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "on connect";
}

void on_close(crazy::http::Request::Ptr req, crazy::http::WSSession::Ptr session) {
    CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "on close";
}

void on_message(crazy::http::Request::Ptr req,crazy::http::WSFrameMessage::Ptr msg, crazy::http::WSSession::Ptr session) {
    CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "on message " << msg->GetData();
    session->SendMessage(msg->GetData());
}

int main () {
    crazy::IPv4Address::Ptr addr1(new crazy::IPv4Address("0.0.0.0", 8877));
    crazy::http::WSServer::Ptr wsserver(new crazy::http::WSServer());
    wsserver->Bind(addr1);
    wsserver->GetServletDispatch()->AddServlet("/ws", on_connect, on_close, on_message);
    wsserver->Start();
    while (1) {
        sleep(1);
    }
    
}