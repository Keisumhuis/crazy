#include "crazy/net/http/websocket_client.h"

#include <random>

#include "crazy/config.h"
#include "crazy/logger.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/tls.h"
#include "crazy/uri.h"

namespace crazy {
	WebSocketClient::WebSocketClient() {
		websocket_parser_init(&wsParser_);
		websocket_parser_settings_init(&wsSettings_);
		wsParser_.data = this;
		wsSettings_.on_frame_header = &WebSocketClient::onFrameHeader;
		wsSettings_.on_frame_body = &WebSocketClient::onFrameBody;
		wsSettings_.on_frame_end = &WebSocketClient::onFrameEnd;
	}
	WebSocketClient::~WebSocketClient() {
		close();
	}
	void WebSocketClient::registerConnectCallback(ConnectCallback callback) {
		connectCallback_ = std::move(callback);
	}
	void WebSocketClient::registerMessageCallback(MessageCallback callback) {
		messageCallback_ = std::move(callback);
	}
	void WebSocketClient::registerCloseCallback(CloseCallback callback) {
		closeCallback_ = std::move(callback);
	}
	void WebSocketClient::resetSession() {
		websocket_parser_init(&wsParser_);
		wsParser_.data = this;
		fragmentBuffer_.clear();
		fragmentOpcode_ = OpCode::text;
		handshakeKey_.clear();
		handshakeParser_.reset();
		sendBuffer_.clear();
		connected_ = false;
		handshakePending_ = false;
		closeSent_ = false;
		closeAfterFlush_ = false;
	}
	bool WebSocketClient::openTransport(const std::string& uri, std::string& host, uint16_t& port, std::string& error) {
		Uri u(uri);
		host = u.getHost();
		if (host.empty()) {
			host = "127.0.0.1";
		}
		// `wss://` 必须真的走 TLS：要么握手成功，要么显式失败，绝不静默明文
		const bool secure = (u.getScheme() == "wss");
		port = u.getPort();
		if (port == 0) {
			port = secure ? 443 : 80;
		}

		if (secure) {
			// 客户端 TLS 参数取 `[HTTP]`/`[http]` 节，与 HttpClient / HttpApplication 保持一致
			const std::string section = Config::HasSection("HTTP") ? "HTTP" : "http";
			TlsOptions options;
			options.caFile = Config::GetString(section, "tls_ca_file");
			options.minVersion = Config::GetString(section, "tls_min_version", "1.2");
			options.verifyPeer = Config::GetBoolean(section, "tls_verify_peer", true);
			TlsContext::ptr context = TlsContext::createClient(options, error);
			if (!context) {
				CRAZY_SYSTEM_ERROR() << "wss is not available, host = " << host << ":" << port
					<< ", section [" << section << "], reason = " << error;
				return false;
			}
			auto socket = std::make_shared<TlsSocket>(context, TlsSocket::Side::client);
			socket->setVerifyHost(host);
			if (!socket->connect(host, port)) {
				error = "connect " + host + ":" + std::to_string(port) + " failed";
				return false;
			}
			if (!socket->handshake(error)) {
				CRAZY_SYSTEM_ERROR() << "wss handshake failed, host = " << host << ":" << port
					<< ", reason = " << error;
				socket->abort();
				return false;
			}
			socket_ = std::move(socket);
		}
		else {
			auto socket = std::make_shared<Socket>();
			if (!socket->connect(host, port)) {
				error = "connect " + host + ":" + std::to_string(port) + " failed";
				return false;
			}
			socket_ = std::move(socket);
		}
		return true;
	}
	bool WebSocketClient::sendRaw(const std::string& data) {
		if (!socket_ || !socket_->active()) {
			return false;
		}
		size_t offset = 0;
		while (offset < data.size()) {
			const int32_t sent = socket_->send(data.data() + offset, data.size() - offset);
			if (sent > 0) {
				offset += static_cast<size_t>(sent);
				continue;
			}
			const int32_t error = WSAGetLastError();
			if (socketWouldBlock(error) || socket_->wantWrite()) {
				continue;
			}
			CRAZY_SYSTEM_ERROR() << "websocket send handshake fail, errno = " << error
				<< " errstr = " << socketErrorText(error);
			return false;
		}
		return true;
	}
	bool WebSocketClient::sendHandshakeRequest(const std::string& uri, const std::string& host) {
		Uri u(uri);
		handshakeKey_ = HttpRequest::generateWebSocketKey();
		auto req = HttpRequest::createWebSocketRequest(u, handshakeKey_);
		std::string target = u.getPath();
		if (target.empty()) {
			target = "/";
		}
		if (!u.getQuery().empty()) {
			target += "?" + u.getQuery();
		}
		req->setUri(target);
		req->headers().insert("Host", host);
		return sendRaw(req->toString());
	}
	bool WebSocketClient::connect(const std::string& uri) {
		async_ = false;
		resetSession();
		std::string host;
		uint16_t port = 0;
		std::string error;
		if (!openTransport(uri, host, port, error) || !sendHandshakeRequest(uri, host)) {
			close();
			return false;
		}
		handshakeParser_ = std::make_shared<HttpResponseParser>();
		handshakePending_ = true;

		char buf[8192];
		while (handshakePending_ && socket_) {
			const int32_t n = socket_->recv(buf, sizeof(buf));
			if (n <= 0) {
				break;
			}
			processIncoming(buf, static_cast<size_t>(n));
		}
		if (!connected_) {
			close();
			return false;
		}
		return true;
	}
	bool WebSocketClient::connectAsync(const std::string& uri, std::string& error) {
		resetSession();
		std::string host;
		uint16_t port = 0;
		if (!openTransport(uri, host, port, error)) {
			return false;
		}
		if (!sendHandshakeRequest(uri, host)) {
			error = "send handshake request failed";
			close();
			return false;
		}
		if (!socket_->setNonBlocking(true)) {
			error = "set non blocking failed";
			close();
			return false;
		}
		handshakeParser_ = std::make_shared<HttpResponseParser>();
		handshakePending_ = true;
		async_ = true;
		watchedFd_ = static_cast<int32_t>(socket_->socket());
		updateInterest();
		return true;
	}
	void WebSocketClient::sendText(const std::string& data) {
		sendFrame(OpCode::text, data);
	}
	void WebSocketClient::sendBinary(const std::string& data) {
		sendFrame(OpCode::binary, data);
	}
	void WebSocketClient::sendClose(uint16_t code) {
		if (!connected_ || closeSent_) {
			return;
		}
		closeSent_ = true;
		std::string payload(2, '\0');
		payload[0] = static_cast<char>((code >> 8) & 0xFF);
		payload[1] = static_cast<char>(code & 0xFF);
		if (async_) {
			// 事件驱动：等发送缓冲（含前面没发完的数据与这一帧）清空后再关闭
			closeAfterFlush_ = true;
			sendFrame(OpCode::close, payload);
			return;
		}
		sendFrame(OpCode::close, payload);
		close();
	}
	void WebSocketClient::run() {
		if (!connected_ || !socket_) {
			return;
		}
		char buf[8192];
		while (socket_) {
			const int32_t n = socket_->recv(buf, sizeof(buf));
			if (n <= 0) {
				break;
			}
			processIncoming(buf, static_cast<size_t>(n));
		}
		close();
	}
	bool WebSocketClient::onReadable() {
		if (!socket_) {
			return false;
		}
		char buf[8192];
		while (socket_) {
			const int32_t n = socket_->recv(buf, sizeof(buf));
			if (n > 0) {
				processIncoming(buf, static_cast<size_t>(n));
				if (!async_) {
					return socket_ != nullptr;
				}
				continue;
			}
			if (0 == n) {
				close();
				return false;
			}
			const int32_t error = WSAGetLastError();
			if (socket_ && (socketWouldBlock(error) || socket_->wantRead() || socket_->wantWrite())) {
				return true;
			}
			CRAZY_SYSTEM_ERROR() << "websocket recv fail, errno = " << error
				<< " errstr = " << socketErrorText(error);
			close();
			return false;
		}
		return false;
	}
	bool WebSocketClient::onWritable() {
		if (!flushSendBuffer()) {
			return true;
		}
		updateInterest();
		return false;
	}
	bool WebSocketClient::hasPendingWrite() const {
		return !sendBuffer_.empty();
	}
	socket_t WebSocketClient::socket() const {
		return socket_ ? socket_->socket() : INVALID_SOCKET;
	}
	bool WebSocketClient::isConnected() const {
		return connected_;
	}
	bool WebSocketClient::isAsync() const {
		return async_;
	}
	void WebSocketClient::registerSelectorCallbacks(RegisterEventCallback onRegister, UnregisterEventCallback onUnregister) {
		registerEvent_ = std::move(onRegister);
		unregisterEvent_ = std::move(onUnregister);
		if (socket_) {
			watchedFd_ = static_cast<int32_t>(socket_->socket());
			updateInterest();
		}
	}
	void WebSocketClient::detachSelector() {
		if (unregisterEvent_ && -1 != watchedFd_) {
			if (readRegistered_) {
				unregisterEvent_(watchedFd_, SelectorEventType::read);
			}
			if (writeRegistered_) {
				unregisterEvent_(watchedFd_, SelectorEventType::write);
			}
		}
		readRegistered_ = false;
		writeRegistered_ = false;
		watchedFd_ = -1;
	}
	void WebSocketClient::close() {
		detachSelector();
		connected_ = false;
		handshakePending_ = false;
		closeAfterFlush_ = false;
		sendBuffer_.clear();
		handshakeParser_.reset();
		if (socket_) {
			socket_->close();
			socket_.reset();
		}
		if (closeCallback_) {
			closeCallback_();
			closeCallback_ = nullptr;
		}
	}
	void WebSocketClient::updateInterest() {
		if (!registerEvent_ || !unregisterEvent_) {
			return;
		}
		if (!socket_) {
			detachSelector();
			return;
		}
		const int32_t fd = (-1 != watchedFd_) ? watchedFd_ : static_cast<int32_t>(socket_->socket());
		watchedFd_ = fd;
		const bool wantWrite = !sendBuffer_.empty();
		if (wantWrite != writeRegistered_) {
			if (wantWrite) {
				registerEvent_(fd, SelectorEventType::write, [this] { onWritable(); });
			}
			else {
				unregisterEvent_(fd, SelectorEventType::write);
			}
			writeRegistered_ = wantWrite;
		}
		if (!readRegistered_) {
			// 回调里可能注销本 fd 的事件，Selector 的分发已经先取出事件句柄再调用，允许这么做
			registerEvent_(fd, SelectorEventType::read, [this] {
				if (!onReadable()) {
					detachSelector();
				}
			});
			readRegistered_ = true;
		}
	}
	void WebSocketClient::processIncoming(const char* data, size_t length) {
		size_t offset = 0;
		if (handshakePending_ && handshakeParser_) {
			offset = handshakeParser_->execute(data, length);
			if (handshakeParser_->hasError()) {
				CRAZY_SYSTEM_ERROR() << "websocket handshake response is invalid";
				close();
				return;
			}
			if (!handshakeParser_->isFinished()) {
				return;
			}
			auto response = handshakeParser_->getResponse();
			if (!response || !response->isWebSocketHandshake() || !response->verifyWebSocketAccept(handshakeKey_)) {
				CRAZY_SYSTEM_ERROR() << "websocket handshake rejected by peer";
				close();
				return;
			}
			handshakePending_ = false;
			connected_ = true;
			handshakeParser_.reset();
			if (connectCallback_) {
				connectCallback_();
			}
			if (!socket_) {
				return;
			}
		}
		if (offset < length) {
			// 握手响应之后同一批数据里可能已经带了帧（101 与首帧同包），继续喂帧解析器
			websocket_parser_execute(&wsParser_, &wsSettings_, data + offset, length - offset);
		}
	}
	bool WebSocketClient::flushSendBuffer() {
		if (!socket_) {
			sendBuffer_.clear();
			return true;
		}
		while (!sendBuffer_.empty()) {
			const int32_t sent = socket_->send(sendBuffer_.data(), sendBuffer_.size());
			if (sent > 0) {
				sendBuffer_.erase(0, static_cast<size_t>(sent));
				continue;
			}
			const int32_t error = WSAGetLastError();
			if (socketWouldBlock(error) || socket_->wantWrite()) {
				return false;
			}
			CRAZY_SYSTEM_ERROR() << "websocket send fail, errno = " << error
				<< " errstr = " << socketErrorText(error);
			close();
			return true;
		}
		if (closeAfterFlush_) {
			closeAfterFlush_ = false;
			close();
		}
		return true;
	}
	void WebSocketClient::sendFrame(OpCode opcode, const std::string& data) {
		if (!connected_ || !socket_) {
			return;
		}
		char mask[4];
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dist(0, 255);
		for (char& byte : mask) {
			byte = static_cast<char>(dist(gen));
		}

		const websocket_flags flags = static_cast<websocket_flags>(
			WS_FIN | WS_HAS_MASK | static_cast<uint8_t>(opcode));
		const size_t frameSize = websocket_calc_frame_size(flags, data.size());
		std::string frame(frameSize, '\0');
		const size_t written = websocket_build_frame(
			frame.data(), flags, mask, data.data(), data.size());
		frame.resize(written);

		sendBuffer_ += frame;
		flushSendBuffer();
		updateInterest();
	}
	int32_t WebSocketClient::onFrameHeader(websocket_parser* parser) {
		auto* self = static_cast<WebSocketClient*>(parser->data);
		const uint8_t opcode = static_cast<uint8_t>(websocket_parser_get_opcode(parser));
		const bool fin = websocket_parser_has_final(parser) != 0;
		if (!fin) {
			if (opcode != static_cast<uint8_t>(OpCode::continuation)) {
				self->fragmentOpcode_ = static_cast<OpCode>(opcode);
			}
		}
		return 0;
	}
	int32_t WebSocketClient::onFrameBody(websocket_parser* parser, const char* data, size_t length) {
		auto* self = static_cast<WebSocketClient*>(parser->data);
		std::string decoded(length, '\0');
		if (websocket_parser_has_mask(parser)) {
			websocket_parser_decode(decoded.data(), data, length, parser);
		}
		else {
			decoded.assign(data, length);
		}
		self->fragmentBuffer_.append(decoded);
		return 0;
	}
	int32_t WebSocketClient::onFrameEnd(websocket_parser* parser) {
		auto* self = static_cast<WebSocketClient*>(parser->data);
		const uint8_t opcode = static_cast<uint8_t>(websocket_parser_get_opcode(parser));
		const bool fin = websocket_parser_has_final(parser) != 0;

		if (fin) {
			const OpCode msgOpcode = (opcode == static_cast<uint8_t>(OpCode::continuation))
				? self->fragmentOpcode_ : static_cast<OpCode>(opcode);
			std::string data = std::move(self->fragmentBuffer_);
			self->fragmentBuffer_.clear();

			switch (msgOpcode) {
			case OpCode::text:
			case OpCode::binary:
				if (self->messageCallback_) {
					self->messageCallback_(data, msgOpcode);
				}
				break;
			case OpCode::ping:
				self->sendFrame(OpCode::pong, data);
				break;
			case OpCode::close:
				if (!self->closeSent_) {
					self->closeSent_ = true;
					self->sendFrame(OpCode::close, data);
				}
				self->close();
				break;
			default:
				break;
			}
		}
		return 0;
	}
}  // namespace crazy
