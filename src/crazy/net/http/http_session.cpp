#include "crazy/net/http/http_session.h"

#include <cerrno>
#include <utility>

#include "crazy/logger.h"
#include "crazy/net/http/http_router.h"
#include "crazy/utils.h"

namespace crazy {
	namespace {
		bool hasHeaderToken(const HttpHeader& headers, const std::string& key, const std::string& token) {
			const auto value = headers.get(key);
			if (!value) {
				return false;
			}
			const std::string lowerValue = StringUtil::ToLower(StringUtil::Trim(*value));
			const std::string lowerToken = StringUtil::ToLower(token);
			size_t offset = 0;
			while (offset <= lowerValue.size()) {
				const size_t comma = lowerValue.find(',', offset);
				const size_t end = comma == std::string::npos ? lowerValue.size() : comma;
				if (StringUtil::Trim(lowerValue.substr(offset, end - offset)) == lowerToken) {
					return true;
				}
				if (comma == std::string::npos) {
					break;
				}
				offset = comma + 1;
			}
			return false;
		}
	}  // namespace

	HttpSession::HttpSession(Socket::ptr socket, HttpRouter::ptr router)
		: socket_(std::move(socket)), router_(std::move(router)) {
		websocket_parser_init(&wsParser_);
		websocket_parser_settings_init(&wsSettings_);
		wsParser_.data = this;
		wsSettings_.on_frame_header = &HttpSession::onFrameHeader;
		wsSettings_.on_frame_body = &HttpSession::onFrameBody;
		wsSettings_.on_frame_end = &HttpSession::onFrameEnd;
	}
	HttpSession::~HttpSession() {
		closeSocket();
	}
	socket_t HttpSession::socket() const {
		return socket_ ? socket_->socket() : INVALID_SOCKET;
	}
	void HttpSession::onReadEvent() {
		if (closed_ || !socket_) {
			return;
		}
		char buffer[8192];
		const int32_t length = socket_->recv(buffer, sizeof(buffer));
		if (length < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return;
			}
			CRAZY_SYSTEM_ERROR() << "http recv fail, errno = " << errno;
			onClose();
			return;
		}
		if (length == 0) {
			// 对端正常关闭连接（FIN）
			onClose();
			return;
		}
		// 升级为 WebSocket 后，改用 websocket_parser 解析帧
		if (upgraded_) {
			websocket_parser_execute(&wsParser_, &wsSettings_, buffer, static_cast<size_t>(length));
			return;
		}
		recvBuffer_.append(buffer, static_cast<size_t>(length));
		processReadBuffer();
	}
	void HttpSession::onWriteEvent() {
		flushWrite();
		if (closed_) {
			return;
		}
		if (sendBuffer_.empty() && writeRegistered_) {
			writeRegistered_ = false;
			if (unregisterEvent_) {
				unregisterEvent_(socket(), SelectorEventType::write);
			}
		}
		if (sendBuffer_.empty()) {
			onResponseFlushed();
		}
	}
	void HttpSession::registerEventCallback(
		std::function<void(socket_t, SelectorEventType, std::function<void()>)> callback) {
		registerEvent_ = std::move(callback);
	}
	void HttpSession::registerUnregisterEventCallback(
		std::function<void(socket_t, SelectorEventType)> callback) {
		unregisterEvent_ = std::move(callback);
	}
	void HttpSession::registerPostCallback(std::function<void(std::function<void()>)> callback) {
		postCallback_ = std::move(callback);
	}
	void HttpSession::registerCloseCallback(std::function<void()> callback) {
		closeCallback_ = std::move(callback);
	}
	void HttpSession::closeSocket() {
		closed_ = true;
		if (socket_) {
			socket_->close();
		}
	}
	void HttpSession::processReadBuffer() {
		if (closed_ || upgraded_ || requestInFlight_) {
			return;
		}
		while (!closed_ && !upgraded_ && !requestInFlight_ && !recvBuffer_.empty()) {
			const size_t consumed = parser_.execute(recvBuffer_.data(), recvBuffer_.size());
			if (parser_.hasError()) {
				onClose();
				return;
			}
			if (consumed == 0) {
				return;
			}
			recvBuffer_.erase(0, consumed);
			if (!parser_.isFinished()) {
				return;
			}
			requestInFlight_ = true;
			handleRequest(parser_.getRequest());
			return;
		}
	}
	void HttpSession::onResponseFlushed() {
		if (closed_ || !sendBuffer_.empty()) {
			return;
		}
		if (closeAfterWrite_) {
			closeAfterWrite_ = false;
			onClose();
			return;
		}
		requestInFlight_ = false;
		if (upgraded_) {
			if (!recvBuffer_.empty()) {
				websocket_parser_execute(&wsParser_, &wsSettings_,
					recvBuffer_.data(), recvBuffer_.size());
				recvBuffer_.clear();
			}
			return;
		}
		processReadBuffer();
	}
	void HttpSession::handleRequest(HttpRequest::ptr request) {
		currentRequestKeepAlive_ = request->shouldKeepAlive();
		if (request->isWebSocketHandshake()) {
			handleWebSocketUpgrade(request);
			return;
		}
		auto response = std::make_shared<HttpResponse>();
		response->setVersion(request->version());
		if (request->shouldKeepAlive()) {
			response->headers().insert("Connection", "keep-alive");
		}
		else {
			response->headers().insert("Connection", "close");
		}
		const auto weakSession = weak_from_this();
		const std::weak_ptr<HttpResponse> weakResponse = response;
		response->setSendCallback([weakSession, weakResponse](HttpResponse&) {
			auto session = weakSession.lock();
			auto response = weakResponse.lock();
			if (!session || !response) {
				return;
			}
			auto send = [session, response]() {
				session->sendResponse(response);
			};
			if (session->postCallback_) {
				session->postCallback_(std::move(send));
			}
			else {
				send();
			}
		});

		bool routed = false;
		try {
			routed = router_ && router_->handle(*request, *response);
		}
		catch (const std::exception& e) {
			CRAZY_SYSTEM_ERROR() << "http handler exception: " << e.what();
			routed = true;
			response->setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
			response->setBody("Internal Server Error");
		}
		catch (...) {
			CRAZY_SYSTEM_ERROR() << "http handler unknown exception";
			routed = true;
			response->setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
			response->setBody("Internal Server Error");
		}

		if (!routed) {
			response->setStatus(HttpStatus::NOT_FOUND);
			response->setBody("Not Found");
		}
		if (!response->isDeferred() && !response->isSent()) {
			response->send();
		}
	}
	void HttpSession::sendResponse(HttpResponse::ptr response) {
		if (closed_ || !response) {
			return;
		}
		if (!response->headers().contains("Content-Type")) {
			response->headers().insert("Content-Type", "text/plain; charset=utf-8");
		}
		if (!response->headers().contains("Connection")) {
			response->headers().insert("Connection", "keep-alive");
		}
		const bool keepAlive = currentRequestKeepAlive_ &&
			!hasHeaderToken(response->headers(), "Connection", "close");
		sendBuffer_ += response->toString();
		closeAfterWrite_ = !keepAlive;
		flushWrite();
		if (closed_) {
			return;
		}
		if (!sendBuffer_.empty() && !writeRegistered_) {
			writeRegistered_ = true;
			if (registerEvent_) {
				registerEvent_(socket(), SelectorEventType::write, std::bind(&HttpSession::onWriteEvent, shared_from_this()));
			}
		}
		if (sendBuffer_.empty()) {
			onResponseFlushed();
		}
	}
	void HttpSession::flushWrite() {
		if (sendBuffer_.empty() || !socket_) {
			return;
		}
		const int32_t length = socket_->send(sendBuffer_.data(), sendBuffer_.size());
		if (length < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return;
			}
			CRAZY_SYSTEM_ERROR() << "http send fail, errno = " << errno;
			onClose();
			return;
		}
		sendBuffer_.erase(0, static_cast<size_t>(length));
	}
	void HttpSession::onClose() {
		if (closed_) {
			return;
		}
		closed_ = true;
		if (socket_) {
			socket_->close();
		}
		if (wsCloseCallback_) {
			wsCloseCallback_();
		}
		if (closeCallback_) {
			closeCallback_();
		}
	}
	void HttpSession::registerWebSocketMessageCallback(WebSocketMessageCallback callback) {
		wsMessageCallback_ = std::move(callback);
	}
	void HttpSession::registerWebSocketConnectCallback(WebSocketConnectCallback callback) {
		wsConnectCallback_ = std::move(callback);
	}
	void HttpSession::registerWebSocketCloseCallback(WebSocketCloseCallback callback) {
		wsCloseCallback_ = std::move(callback);
	}
	void HttpSession::handleWebSocketUpgrade(HttpRequest::ptr request) {
		const auto keyOpt = request->headers().get("Sec-WebSocket-Key");
		if (!keyOpt || keyOpt->empty()) {
			onClose();
			return;
		}

		auto response = std::make_shared<HttpResponse>();
		response->setStatus(HttpStatus::SWITCHING_PROTOCOLS);
		response->setVersion(request->version());
		response->headers().insert("Upgrade", "websocket");
		response->headers().insert("Connection", "Upgrade");
		response->headers().insert("Sec-WebSocket-Accept", HttpResponse::computeWebSocketAccept(*keyOpt));
		sendBuffer_ += response->toString();
		flushWrite();
		if (closed_) {
			return;
		}
		if (!sendBuffer_.empty() && !writeRegistered_) {
			writeRegistered_ = true;
			if (registerEvent_) {
				registerEvent_(socket(), SelectorEventType::write,
					std::bind(&HttpSession::onWriteEvent, shared_from_this()));
			}
		}
		upgraded_ = true;
		if (wsConnectCallback_) {
			wsConnectCallback_(shared_from_this());
		}
		if (sendBuffer_.empty()) {
			onResponseFlushed();
		}
	}
	void HttpSession::handleWebSocketMessage(const std::string& data, WebSocketOpCode opcode) {
		if (wsMessageCallback_) {
			wsMessageCallback_(shared_from_this(), data, opcode);
		}
	}
	void HttpSession::handleWebSocketControlFrame(WebSocketOpCode opcode, const std::string& data) {
		switch (opcode) {
		case WebSocketOpCode::ping:
			sendPong(data);
			break;
		case WebSocketOpCode::pong:
			break;
		case WebSocketOpCode::close:
			if (!closeSent_) {
				closeSent_ = true;
				sendFrame(WebSocketOpCode::close, data);
			}
			onClose();
			break;
		default:
			break;
		}
	}
	void HttpSession::sendFrame(WebSocketOpCode opcode, const std::string& data) {
		if (closed_ || !socket_) {
			return;
		}
		websocket_flags flags = static_cast<websocket_flags>(WS_FIN | static_cast<uint8_t>(opcode));
		const size_t frameSize = websocket_calc_frame_size(flags, data.size());
		std::string frame(frameSize, '\0');
		const size_t written = websocket_build_frame(frame.data(), flags, nullptr, data.data(), data.size());
		frame.resize(written);
		sendBuffer_ += frame;
		flushWrite();
		if (closed_) {
			return;
		}
		if (!sendBuffer_.empty() && !writeRegistered_) {
			writeRegistered_ = true;
			if (registerEvent_) {
				registerEvent_(socket(), SelectorEventType::write,
					std::bind(&HttpSession::onWriteEvent, shared_from_this()));
			}
		}
	}
	void HttpSession::sendText(const std::string& data) {
		sendFrame(WebSocketOpCode::text, data);
	}
	void HttpSession::sendBinary(const std::string& data) {
		sendFrame(WebSocketOpCode::binary, data);
	}
	void HttpSession::sendPing(const std::string& data) {
		sendFrame(WebSocketOpCode::ping, data);
	}
	void HttpSession::sendPong(const std::string& data) {
		sendFrame(WebSocketOpCode::pong, data);
	}
	void HttpSession::sendClose(uint16_t code) {
		if (closeSent_) {
			return;
		}
		closeSent_ = true;
		std::string payload(2, '\0');
		payload[0] = static_cast<char>((code >> 8) & 0xFF);
		payload[1] = static_cast<char>(code & 0xFF);
		sendFrame(WebSocketOpCode::close, payload);
		onClose();
	}
	int32_t HttpSession::onFrameHeader(websocket_parser* parser) {
		auto* self = static_cast<HttpSession*>(parser->data);
		const uint8_t opcode = static_cast<uint8_t>(websocket_parser_get_opcode(parser));
		const bool fin = websocket_parser_has_final(parser) != 0;
		if (!fin) {
			if (opcode != static_cast<uint8_t>(WebSocketOpCode::continuation)) {
				self->fragmentOpcode_ = static_cast<WebSocketOpCode>(opcode);
			}
		}
		return 0;
	}
	int32_t HttpSession::onFrameBody(websocket_parser* parser, const char* data, size_t length) {
		auto* self = static_cast<HttpSession*>(parser->data);
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
	int32_t HttpSession::onFrameEnd(websocket_parser* parser) {
		auto* self = static_cast<HttpSession*>(parser->data);
		const uint8_t opcode = static_cast<uint8_t>(websocket_parser_get_opcode(parser));
		const bool fin = websocket_parser_has_final(parser) != 0;

		if (fin) {
			WebSocketOpCode msgOpcode = (opcode == static_cast<uint8_t>(WebSocketOpCode::continuation))
				? self->fragmentOpcode_ : static_cast<WebSocketOpCode>(opcode);
			std::string data = std::move(self->fragmentBuffer_);
			self->fragmentBuffer_.clear();

			switch (msgOpcode) {
			case WebSocketOpCode::text:
			case WebSocketOpCode::binary:
				self->handleWebSocketMessage(data, msgOpcode);
				break;
			case WebSocketOpCode::close:
				self->handleWebSocketControlFrame(WebSocketOpCode::close, data);
				break;
			case WebSocketOpCode::ping:
				self->handleWebSocketControlFrame(WebSocketOpCode::ping, data);
				break;
			case WebSocketOpCode::pong:
				self->handleWebSocketControlFrame(WebSocketOpCode::pong, data);
				break;
			default:
				break;
			}
		}
		return 0;
	}
}  // namespace crazy
