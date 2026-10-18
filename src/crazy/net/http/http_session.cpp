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
		// 按 RFC 7230 4.1 写出终止块；有 trailer 时写在终止块之后、结束空行之前
		void appendChunkTerminator(std::string& out, const HttpHeader& trailers) {
			out += "0\r\n";
			for (const auto& trailer : trailers) {
				out += trailer.first;
				out += ": ";
				out += trailer.second;
				out += "\r\n";
			}
			out += "\r\n";
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
		for (;;) {
			const int32_t length = socket_->recv(buffer, sizeof(buffer));
			if (length < 0) {
				// TLS 的 WANT_READ/WANT_WRITE 与明文 socket 的 EAGAIN/EWOULDBLOCK 一样，
				// 都是「稍后再来」，不是错误
				if (errno == EAGAIN || errno == EWOULDBLOCK || socket_->wantRead() || socket_->wantWrite()) {
					return;
				}
				CRAZY_SYSTEM_ERROR() << "http recv fail, errno = " << errno;
				onClose();
				return;
			}
			if (length == 0) {
				// 对端正常关闭连接（明文 FIN / TLS close_notify）
				onClose();
				return;
			}
			// 升级为 WebSocket 后，改用 websocket_parser 解析帧
			if (upgraded_) {
				websocket_parser_execute(&wsParser_, &wsSettings_, buffer, static_cast<size_t>(length));
			}
			else {
				recvBuffer_.append(buffer, static_cast<size_t>(length));
				processReadBuffer();
			}
			if (closed_) {
				return;
			}
			// TLS 层可能已经把下一条记录解密到自己的缓冲区里，而 socket 上不再有新数据；
			// 此时 level-triggered 的 epoll 不会再通知，必须在这里排空（明文 socket 恒为 0，
			// 因此行为与既有实现一致，不会二次阻塞读）
			if (socket_->pending() <= 0) {
				return;
			}
		}
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
			// 客户端在等 100 Continue 时必须先回临时响应，否则请求体永远发不出来
			if (!continueSent_ && parser_.expectContinue() && !parser_.isFinished()) {
				continueSent_ = true;
				sendContinue();
				if (closed_) {
					return;
				}
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
		if (chunkedStreaming_) {
			if (streamingResponse_) {
				// 数据源驱动的分块响应：可写时继续拉取下一段
				pumpStreamingChunks();
			}
			// 分块响应尚未写出终止块，连接保持占用
			return;
		}
		if (!requestInFlight_) {
			// 没有在途响应（例如 100 Continue 刚写完）：不改动请求相关状态
			return;
		}
		// 当前响应（含分块响应）已完整写出，复位分块写出状态
		chunkedHeaderSent_ = false;
		continueSent_ = false;
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
		requestVersion_ = request->version();
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
		response->setChunkCallback([weakSession, weakResponse](
			HttpResponse&, const std::string& data, bool finished) {
			auto session = weakSession.lock();
			auto response = weakResponse.lock();
			if (!session || !response) {
				return;
			}
			auto send = [session, response, data, finished]() {
				session->sendChunk(response, data, finished);
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
		if (response->isChunked()) {
			if (chunkedHeaderSent_) {
				// 分块响应头已写出，后续数据段由 sendChunk 写出
				return;
			}
			// HTTP/1.0 客户端不认识分块编码：降级为「不带长度、由关闭连接界定响应体」
			const bool chunkedFraming = requestVersion_.getMajor() > 1 ||
				(requestVersion_.getMajor() == 1 && requestVersion_.getMinor() >= 1);
			if (!chunkedFraming) {
				response->setChunkedFraming(false);
				response->headers().erase("Connection");
				response->headers().insert("Connection", "close");
			}
			if (response->isStreaming()) {
				streamingResponse_ = response;
			}
			chunkedHeaderSent_ = true;
			chunkedStreaming_ = true;
			chunkedKeepAlive_ = chunkedFraming && currentRequestKeepAlive_ &&
				!hasHeaderToken(response->headers(), "Connection", "close");
			// 分块响应头不携带 Content-Length，响应体逐段写出
			sendBuffer_ += response->toString();
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
			return;
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
	void HttpSession::sendChunk(HttpResponse::ptr response, const std::string& data, bool finished) {
		if (closed_ || !response || !response->isChunked()) {
			return;
		}
		const bool chunkedFraming = response->isChunkedFraming();
		if (finished) {
			if (chunkedFraming) {
				appendChunkTerminator(sendBuffer_, response->trailers());
			}
			// 正常收到终止块后才允许复用连接；异常中断由 onClose 强制关闭
			chunkedStreaming_ = false;
			streamingResponse_.reset();
			closeAfterWrite_ = chunkedFraming ? !chunkedKeepAlive_ : true;
		}
		else if (!data.empty()) {
			sendBuffer_ += chunkedFraming ? httpEncodeChunk(data) : data;
		}
		else {
			return;
		}
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
	void HttpSession::sendContinue() {
		if (closed_) {
			return;
		}
		sendBuffer_ += "HTTP/1.1 100 Continue\r\n\r\n";
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
	}
	void HttpSession::pumpStreamingChunks() {
		if (closed_ || !socket_ || !streamingResponse_) {
			return;
		}
		auto response = streamingResponse_;
		const bool chunkedFraming = response->isChunkedFraming();
		const HttpResponse::ChunkSource source = response->chunkSource();
		bool finished = false;
		while (sendBuffer_.size() < kChunkHighWaterMark) {
			std::string data;
			size_t produced = 0;
			try {
				if (source) {
					produced = source(data);
				}
			}
			catch (const std::exception& e) {
				CRAZY_SYSTEM_ERROR() << "http chunk source fail: " << e.what();
				onClose();
				return;
			}
			catch (...) {
				CRAZY_SYSTEM_ERROR() << "http chunk source fail: unknown exception";
				onClose();
				return;
			}
			if (produced == 0 || data.empty()) {
				// 数据源结束：写终止块（HTTP/1.0 降级时不写，改由关闭连接界定响应体）
				if (chunkedFraming) {
					appendChunkTerminator(sendBuffer_, response->trailers());
				}
				streamingResponse_.reset();
				chunkedStreaming_ = false;
				closeAfterWrite_ = chunkedFraming ? !chunkedKeepAlive_ : true;
				finished = true;
				break;
			}
			sendBuffer_ += chunkedFraming ? httpEncodeChunk(data) : data;
			flushWrite();
			if (closed_) {
				return;
			}
			if (!sendBuffer_.empty()) {
				// 内核写不动了：等可写事件再拉下一段，避免把整份文件堆进内存
				break;
			}
		}
		flushWrite();
		if (closed_) {
			return;
		}
		if (!sendBuffer_.empty()) {
			if (!writeRegistered_) {
				writeRegistered_ = true;
				if (registerEvent_) {
					registerEvent_(socket(), SelectorEventType::write, std::bind(&HttpSession::onWriteEvent, shared_from_this()));
				}
			}
			return;
		}
		if (finished) {
			onResponseFlushed();
		}
	}
	void HttpSession::flushWrite() {
		if (sendBuffer_.empty() || !socket_) {
			return;
		}
		const int32_t length = socket_->send(sendBuffer_.data(), sendBuffer_.size());
		if (length < 0) {
			// TLS 的 WANT_READ/WANT_WRITE：保留整段明文并等待可写，不能按字节偏移截断
			if (errno == EAGAIN || errno == EWOULDBLOCK || socket_->wantRead() || socket_->wantWrite()) {
				return;
			}
			CRAZY_SYSTEM_ERROR() << "http send fail, errno = " << errno;
			onClose();
			return;
		}
		if (static_cast<size_t>(length) >= sendBuffer_.size()) {
			sendBuffer_.clear();
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
