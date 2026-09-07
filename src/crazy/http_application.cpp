#include "crazy/http_application.h"

#include "crazy/config.h"
#include "crazy/logger.h"

namespace crazy {
	void HttpApplication::listen(uint16_t port, const std::string& address) {
		port_ = port;
		address_ = address;
		listenSet_ = true;
	}
	void HttpApplication::listen(const std::string& address, uint16_t port) {
		listen(port, address);
	}
	void HttpApplication::startServer() {
		const bool hasHttpConfig = Config::HasSection("HTTP") || Config::HasSection("http");
		if (httpServer_ == nullptr && !listenSet_ && !hasHttpConfig) {
			return;
		}

		if (!listenSet_) {
			const std::string section = Config::HasSection("HTTP") ? "HTTP" : "http";
			address_ = Config::GetString(section, "address", "0.0.0.0");
			port_ = static_cast<uint16_t>(Config::GetInteger(section, "port", 8080));
		}

		auto server = ensureServer();
		if (!server->listen(port_, address_)) {
			CRAZY_SYSTEM_ERROR() << "http application listen failed";
			return;
		}
		registerEvent(server->listener()->socket(), SelectorEventType::read, std::bind(&HttpApplication::onHttpAccept, this));
		CRAZY_SYSTEM_INFO() << "http application listening " << address_ << ":" << port_;
	}
	HttpServer::ptr HttpApplication::ensureServer() {
		if (!httpServer_) {
			httpServer_ = std::make_shared<HttpServer>();
		}
		return httpServer_;
	}
	void HttpApplication::registerWebSocketMessageCallback(HttpSession::WebSocketMessageCallback callback) {
		wsMessageCallback_ = std::move(callback);
	}
	void HttpApplication::registerWebSocketConnectCallback(HttpSession::WebSocketConnectCallback callback) {
		wsConnectCallback_ = std::move(callback);
	}
	void HttpApplication::registerWebSocketCloseCallback(HttpSession::WebSocketCloseCallback callback) {
		wsCloseCallback_ = std::move(callback);
	}
	void HttpApplication::onHttpAccept() {
		auto session = httpServer_->acceptSession();
		if (session) {
			assignHttpSession(session);
		}
	}
	void HttpApplication::assignHttpSession(HttpSession::ptr session) {
		auto actor = getActorImplement();
		session->registerEventCallback([actor](socket_t fd, SelectorEventType type, std::function<void()> callback) {
			actor->registerEventOnThread(fd, type, std::move(callback));
			});
		session->registerUnregisterEventCallback([actor](socket_t fd, SelectorEventType type) {
			actor->unregisterEventOnThread(fd, type);
			});
		auto weakSession = std::weak_ptr<HttpSession>(session);
		session->registerCloseCallback([actor, weakSession]() {
			if (auto closeSession = weakSession.lock()) {
				actor->cancelEventOnThread(closeSession->socket(), [closeSession]() {
					closeSession->closeSocket(); });
			}
			});
		if (wsMessageCallback_) {
			session->registerWebSocketMessageCallback(wsMessageCallback_);
		}
		if (wsConnectCallback_) {
			session->registerWebSocketConnectCallback(wsConnectCallback_);
			if (wsCloseCallback_) {
				session->registerWebSocketCloseCallback(wsCloseCallback_);
			}
			actor->registerEventOnThread(session->socket(), SelectorEventType::read, [session]() {
				session->onReadEvent();
				});
		}
	}
}  // namespace crazy
