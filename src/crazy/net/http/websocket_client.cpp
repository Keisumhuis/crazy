#include "crazy/net/http/websocket_client.h"

#include <random>

#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/uri.h"

namespace crazy {
	WebSocketClient::WebSocketClient()
		: socket_(std::make_shared<Socket>()) {
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
	bool WebSocketClient::connect(const std::string& uri) {
		Uri u(uri);
		std::string host = u.getHost();
		if (host.empty()) {
			host = "127.0.0.1";
		}
		uint16_t port = u.getPort();
		if (port == 0) {
			port = (u.getScheme() == "wss") ? 443 : 80;
		}

		if (!socket_->connect(host, port)) {
			return false;
		}

		const std::string key = HttpRequest::generateWebSocketKey();
		auto req = HttpRequest::createWebSocketRequest(u, key);
		std::string target = u.getPath();
		if (target.empty()) {
			target = "/";
		}
		if (!u.getQuery().empty()) {
			target += "?" + u.getQuery();
		}
		req->setUri(target);
		req->headers().insert("Host", host);

		const std::string raw = req->toString();
		if (socket_->send(raw.data(), raw.size()) < 0) {
			socket_->close();
			return false;
		}

		HttpResponseParser parser;
		char buf[8192];
		while (!parser.isFinished() && !parser.hasError()) {
			const int32_t n = socket_->recv(buf, sizeof(buf));
			if (n <= 0) {
				break;
			}
			parser.execute(buf, static_cast<size_t>(n));
		}

		auto resp = parser.getResponse();
		if (!resp || !resp->isWebSocketHandshake() || !resp->verifyWebSocketAccept(key)) {
			socket_->close();
			return false;
		}

		connected_ = true;
		if (connectCallback_) {
			connectCallback_();
		}
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
		sendFrame(OpCode::close, payload);
		close();
	}
	void WebSocketClient::run() {
		char buf[8192];
		while (connected_) {
			const int32_t n = socket_->recv(buf, sizeof(buf));
			if (n <= 0) {
				break;
			}
			websocket_parser_execute(&wsParser_, &wsSettings_, buf, static_cast<size_t>(n));
		}
		if (connected_) {
			close();
		}
	}
	void WebSocketClient::close() {
		if (!connected_ && !socket_) {
			return;
		}
		connected_ = false;
		if (socket_) {
			socket_->close();
		}
		if (closeCallback_) {
			closeCallback_();
			closeCallback_ = nullptr;
		}
	}
	bool WebSocketClient::isConnected() const {
		return connected_;
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
		socket_->send(frame.data(), frame.size());
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
