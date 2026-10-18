/**
 * @file test_websocket_client_async.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief WebSocket 客户端事件驱动用法测试：connectAsync + Selector 驱动 onReadable/onWritable.
 * @version 0.1
 * @date 2026-10-12
 */

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "crazy/net/http/http_response.h"
#include "crazy/net/http/websocket_client.h"
#include "crazy/net/http/websocket_parser.h"
#include "crazy/net/selector.h"
#include "crazy/net/socket.h"

namespace {
	int failures = 0;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			++failures;
		}
	}

	struct FrameCollector {
		std::string payload;
		bool ended = false;
	};

	int onFrameBody(websocket_parser* parser, const char* data, size_t length) {
		auto* ctx = static_cast<FrameCollector*>(parser->data);
		if (websocket_parser_has_mask(parser)) {
			std::string decoded(length, '\0');
			websocket_parser_decode(decoded.data(), data, length, parser);
			ctx->payload.append(decoded);
		}
		else {
			ctx->payload.append(data, length);
		}
		return 0;
	}

	int onFrameEnd(websocket_parser* parser) {
		auto* ctx = static_cast<FrameCollector*>(parser->data);
		ctx->ended = true;
		return 0;
	}

	std::string buildServerFrame(websocket_flags flags, const std::string& data) {
		std::string frame(websocket_calc_frame_size(flags, data.size()), '\0');
		const size_t written = websocket_build_frame(
			frame.data(), flags, nullptr, data.data(), data.size());
		frame.resize(written);
		return frame;
	}

	// 头字段名大小写不敏感（HttpHeader 序列化时统一小写），这里按小写查找
	bool findHeaderValue(const std::string& request, const std::string& name, std::string& value) {
		std::string lowered;
		lowered.reserve(request.size());
		for (const char ch : request) {
			lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		}
		std::string field = name;
		for (char& ch : field) {
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		}
		field += ':';
		const size_t fieldPos = lowered.find(field);
		if (std::string::npos == fieldPos) {
			return false;
		}
		size_t valueStart = fieldPos + field.size();
		while (valueStart < request.size() && (' ' == request[valueStart] || '\t' == request[valueStart])) {
			++valueStart;
		}
		const size_t valueEnd = lowered.find("\r\n", valueStart);
		if (std::string::npos == valueEnd) {
			return false;
		}
		value = request.substr(valueStart, valueEnd - valueStart);
		return true;
	}

	// 最小 WebSocket 服务端：101 + 首帧同一个包发出 → 收一帧 → 回显 → close 帧
	void runWsServer(uint16_t port, std::atomic<bool>& ready, std::atomic<bool>& echoed) {
		crazy::Socket server;
		if (!server.listen(port, "127.0.0.1")) {
			ready.store(true);
			return;
		}
		ready.store(true);

		auto client = server.accept();
		if (!client) {
			return;
		}

		std::string request;
		char buf[8192];
		while (request.find("\r\n\r\n") == std::string::npos) {
			const int n = client->recv(buf, sizeof(buf));
			if (n <= 0) {
				client->close();
				return;
			}
			request.append(buf, n);
		}

		std::string key;
		if (!findHeaderValue(request, "Sec-WebSocket-Key", key)) {
			client->close();
			return;
		}
		const std::string accept = crazy::HttpResponse::computeWebSocketAccept(key);
		std::string handshake =
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: " + accept + "\r\n"
			"\r\n";
		handshake += buildServerFrame(static_cast<websocket_flags>(WS_FIN | WS_OP_TEXT), "welcome from server");
		client->send(handshake.data(), handshake.size());

		websocket_parser parser;
		websocket_parser_settings settings;
		websocket_parser_init(&parser);
		websocket_parser_settings_init(&settings);
		FrameCollector collector;
		parser.data = &collector;
		settings.on_frame_body = onFrameBody;
		settings.on_frame_end = onFrameEnd;

		while (!collector.ended) {
			const int n = client->recv(buf, sizeof(buf));
			if (n <= 0) {
				break;
			}
			websocket_parser_execute(&parser, &settings, buf, static_cast<size_t>(n));
		}
		if (collector.ended) {
			const std::string echo = buildServerFrame(
				static_cast<websocket_flags>(WS_FIN | WS_OP_TEXT), collector.payload);
			client->send(echo.data(), echo.size());
			echoed.store(true);
		}

		std::string closePayload(2, '\0');
		closePayload[0] = static_cast<char>(0x03);
		closePayload[1] = static_cast<char>(0xE8);
		const std::string closeFrame = buildServerFrame(
			static_cast<websocket_flags>(WS_FIN | WS_OP_CLOSE), closePayload);
		client->send(closeFrame.data(), closeFrame.size());
		client->close();
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
	const uint16_t port = 19181;
	std::atomic<bool> ready{ false };
	std::atomic<bool> echoed{ false };
	std::thread serverThread(runWsServer, port, std::ref(ready), std::ref(echoed));
	while (!ready.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	crazy::Selector selector;
	crazy::WebSocketClient ws;
	std::vector<std::string> messages;
	bool connected = false;
	bool closed = false;
	bool timedOut = false;
	std::atomic<bool> done{ false };

	ws.registerConnectCallback([&] {
		connected = true;
		// 握手完成后在回调里发（与 selector 同线程）
		ws.sendText("hello from async client");
	});
	ws.registerMessageCallback([&](const std::string& data, crazy::WebSocketClient::OpCode opcode) {
		if (opcode == crazy::WebSocketClient::OpCode::text) {
			messages.push_back(data);
		}
	});
	ws.registerCloseCallback([&] {
		closed = true;
		done.store(true);
		selector.wakeup();
	});

	std::string error;
	CHECK(ws.connectAsync("ws://127.0.0.1:" + std::to_string(port) + "/chat", error));
	CHECK(ws.isAsync());
	CHECK(ws.socket() != INVALID_SOCKET);

	ws.registerSelectorCallbacks(
		[&selector](int32_t fd, crazy::SelectorEventType type, std::function<void()> callback) {
			selector.registerEvent(fd, type, std::move(callback));
		},
		[&selector](int32_t fd, crazy::SelectorEventType type) {
			selector.unregisterEvent(fd, type);
		});
	selector.registerTimer("watchdog", 5000, [&] {
		timedOut = true;
		done.store(true);
		selector.wakeup();
	});

	while (!done.load()) {
		selector.select();
	}

	CHECK(!timedOut);
	CHECK(connected);
	CHECK(closed);
	CHECK(!ws.isConnected());
	CHECK(messages.size() == 2);
	CHECK(!messages.empty() && messages[0] == "welcome from server");
	CHECK(messages.size() > 1 && messages[1] == "hello from async client");
	CHECK(echoed.load());

	ws.close();
	selector.unregisterTimer("watchdog");
	serverThread.join();

	if (failures == 0) {
		std::cout << "websocket client async tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}

#undef CHECK
