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
		return acceptor_->listen(port, address);
	}
	const Socket::ptr& HttpServer::listener() const {
		return acceptor_;
	}
	HttpSession::ptr HttpServer::acceptSession() {
		auto clientSocket = acceptor_->accept();
		if (!clientSocket) {
			return nullptr;
		}
		return std::make_shared<HttpSession>(clientSocket, router_);
	}
	const HttpRouter::ptr& HttpServer::router() const {
		return router_;
	}
}
