#include "crazy/net/http/http_server.h"

#include "crazy/logger.h"

namespace crazy {
	HttpServer::HttpServer()
		: router_(std::make_shared<HttpRouter>()),
		acceptor_(std::make_shared<Socket>()) {
	}
	HttpServer::~HttpServer() {
		if (acceptor_) {
			acceptor_->close();
		}
	}
	bool HttpServer::listen(uint16_t port, const std::string& address) {
		return listen(port, address, nullptr);
	}
	bool HttpServer::listen(uint16_t port, const std::string& address, const TlsContext::ptr& tlsContext) {
		tlsContext_ = tlsContext;
		if (tlsContext_) {
			acceptor_ = std::make_shared<TlsServerSocket>(tlsContext_);
		}
		else if (!acceptor_) {
			acceptor_ = std::make_shared<Socket>();
		}
		return acceptor_->listen(port, address);
	}
	const TlsContext::ptr& HttpServer::tlsContext() const {
		return tlsContext_;
	}
	const Socket::ptr& HttpServer::listener() const {
		return acceptor_;
	}
	HttpSession::ptr HttpServer::acceptSession() {
		auto clientSocket = acceptor_->accept();
		if (!clientSocket) {
			return nullptr;
		}
		// TLS 握手失败（畸形字节、协议版本不匹配等）：直接关闭连接，
		// HTTP 层一个字节都不会收到，也绝不退化成明文
		auto tlsSocket = std::dynamic_pointer_cast<TlsSocket>(clientSocket);
		if (tlsSocket) {
			std::string error;
			if (!tlsSocket->handshake(error)) {
				CRAZY_SYSTEM_ERROR() << "tls handshake failed, remote = " << clientSocket->remoteAddress()
					<< ", reason = " << error;
				tlsSocket->close();
				return nullptr;
			}
		}
		return std::make_shared<HttpSession>(clientSocket, router_);
	}
	const HttpRouter::ptr& HttpServer::router() const {
		return router_;
	}
}
