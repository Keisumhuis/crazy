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

		// 节名选择与 TLS 配置读取都必须独立于 listenSet_：
		// 调用方用 app.listen(port, address) 时也不能跳过 TLS 配置（否则监听会静默保持明文）
		const std::string section = Config::HasSection("HTTP") ? "HTTP" : "http";
		if (!listenSet_) {
			address_ = Config::GetString(section, "address", "0.0.0.0");
			port_ = static_cast<uint16_t>(Config::GetInteger(section, "port", 8080));
		}

		TlsContext::ptr tlsContext = nullptr;
		tlsInitError_.clear();
		if (Config::GetBoolean(section, "tls_enable", false)) {
			std::string error;
			tlsContext = createTlsContext(section, error);
			if (!tlsContext) {
				// 可用性优先（Lead 决策 2026-10-10「没有证书的时候也要正常运行」）：
				// 证书/私钥缺失或不可读、tls_min_version 非法、TLS 未编译等任何 TLS 初始化失败
				// 都不再拒绝启动、不再跳过监听，而是降级为明文继续监听该端口；
				// 但必须以醒目告警（含具体文件路径与底层错误）+ 可查询的 tlsInitError() 记录，
				// 使这次降级绝不是静默行为。
				tlsInitError_ = error;
				CRAZY_SYSTEM_ERROR()
					<< "[TLS ALERT] tls_enable=true but TLS initialization failed, "
					<< "serving PLAINTEXT on " << address_ << ":" << port_
					<< " (TLS 已启用但加载失败，现以明文提供服务). reason = " << error
					<< ". Deployments that rely on HTTPS must monitor this warning: "
					<< "the framework does not guarantee that \"TLS configured\" means \"running TLS\".";
			}
		}

		auto server = ensureServer();
		if (!server->listen(port_, address_, tlsContext)) {
			CRAZY_SYSTEM_ERROR() << "http application listen failed";
			return;
		}
		registerEvent(server->listener()->socket(), SelectorEventType::read, std::bind(&HttpApplication::onHttpAccept, this));
		if (server->tlsContext()) {
			CRAZY_SYSTEM_INFO() << "https application listening " << address_ << ":" << port_;
		}
		else if (!tlsInitError_.empty()) {
			CRAZY_SYSTEM_INFO() << "http application listening " << address_ << ":" << port_
				<< " (PLAINTEXT fallback: tls initialization failed, see the [TLS ALERT] line above)";
		}
		else {
			CRAZY_SYSTEM_INFO() << "http application listening " << address_ << ":" << port_;
		}
	}
	const std::string& HttpApplication::tlsInitError() const {
		return tlsInitError_;
	}
	TlsContext::ptr HttpApplication::createTlsContext(const std::string& section, std::string& error) {
		TlsOptions options;
		options.certFile = Config::GetString(section, "tls_cert_file");
		options.keyFile = Config::GetString(section, "tls_key_file");
		options.minVersion = Config::GetString(section, "tls_min_version", "1.2");
		options.verifyPeer = false;
		TlsContext::ptr context = TlsContext::createServer(options, error);
		if (!context) {
			error = "section [" + section + "] tls_cert_file = \"" + options.certFile
				+ "\", tls_key_file = \"" + options.keyFile + "\": " + error;
		}
		return context;
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
		session->registerPostCallback([actor](std::function<void()> callback) {
			actor->enqueueFunction(std::move(callback));
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
		}
		if (wsCloseCallback_) {
			session->registerWebSocketCloseCallback(wsCloseCallback_);
		}
		actor->registerEventOnThread(session->socket(), SelectorEventType::read, [session]() {
			session->onReadEvent();
			});
	}
}  // namespace crazy
