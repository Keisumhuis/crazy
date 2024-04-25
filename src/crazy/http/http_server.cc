#include "http_server.h"

namespace crazy::http {
HttpServer::HttpServer() 
	: m_isKeepAlive (true) {
	m_dispatch.reset(new ServletDispatch);
}
void HttpServer::HandleClient(Socket::Ptr sock) {
	HttpSession::Ptr session(new HttpSession(sock));
	do {
		auto req = session->RecvRequest();
		if (!req) {
			// CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "recv http request fail, errno = "
			//  	<< errno << " errstr = " << strerror(errno) << " client addr = "
			//  	<< sock->GetRemoteAddress()->ToString() << " socket " << sock->GetSocket();
			break;
		}
		auto res = req->GetResponse();	
		m_dispatch->Handle(req, res);
		res->SetKeepALive(res->IsKeepALive() && m_isKeepAlive);
		auto rt = session->SendResponse(res);
		if (rt <= 0 || !m_isKeepAlive || !req->IsKeepALive()) {
			break;
		}
	} while (true);
}
ServletDispatch::Ptr HttpServer::GetServletDispatch() const {
	return m_dispatch;
}
void HttpServer::SetServletDispatch(ServletDispatch::Ptr dispatch) {
	m_dispatch = dispatch;
}
void HttpServer::SetName(const std::string& name) {
	TcpServer::SetName(name);
}
void HttpServer::SetKeepAlive(bool keepalive) {
	m_isKeepAlive = keepalive;
}
bool HttpServer::GetKeepAlive() const {
	return m_isKeepAlive;
}
}
