/**
 * @file test_websocket_client_e2e.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief WebSocket 客户端端到端测试（真实握手与消息收发）.
 * @version 0.1
 * @date 2026-09-06
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

#include "crazy/net/http/http_response.h"
#include "crazy/net/http/websocket_client.h"
#include "crazy/net/http/websocket_parser.h"
#include "crazy/net/socket.h"

namespace {
	int failures = 0;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	// 帧收集上下文（解析客户端发来的带 mask 帧）
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

	// 构建服务端帧（无 mask）
	std::string buildServerFrame(websocket_flags flags, const std::string& data) {
		std::string frame(websocket_calc_frame_size(flags, data.size()), '\0');
		const size_t written = websocket_build_frame(
			frame.data(), flags, nullptr, data.data(), data.size());
		frame.resize(written);
		return frame;
	}

	// 最小 WebSocket 服务端：握手 → 发欢迎消息 → 收客户端帧 → 发 close 帧
	void runWsServer(uint16_t port, std::atomic<bool>& ready) {
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

		// 读取握手请求
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

		// 提取 Sec-WebSocket-Key
		const std::string keyHeader = "Sec-WebSocket-Key:";
		const size_t keyPos = request.find(keyHeader);
		if (keyPos == std::string::npos) {
			client->close();
			return;
		}
		size_t valueStart = keyPos + keyHeader.size();
		while (valueStart < request.size() &&
			(request[valueStart] == ' ' || request[valueStart] == '\t')) {
			++valueStart;
		}
		const size_t valueEnd = request.find("\r\n", valueStart);
		const std::string key = request.substr(valueStart, valueEnd - valueStart);

		// 发送握手响应
		const std::string accept = crazy::HttpResponse::computeWebSocketAccept(key);
		const std::string response =
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: " + accept + "\r\n"
			"\r\n";
		client->send(response.data(), response.size());

		// 发送欢迎消息（服务端帧，无 mask）
		const std::string welcomeFrame = buildServerFrame(
			static_cast<websocket_flags>(WS_FIN | WS_OP_TEXT), "welcome from server");
		client->send(welcomeFrame.data(), welcomeFrame.size());

		// 接收客户端帧（带 mask）
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

		// 发送关闭帧（状态码 1000）
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
#ifdef _WIN32
	// Windows 下 socket 使用前必须先初始化 Winsock 库
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed" << std::endl;
		return 1;
	}
#endif
	const uint16_t port = 19091;
	std::atomic<bool> ready{ false };

	std::thread serverThread(runWsServer, port, std::ref(ready));
	while (!ready.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	crazy::WebSocketClient ws;
	std::string received;
	ws.registerMessageCallback([&](const std::string& data, crazy::WebSocketClient::OpCode opcode) {
		if (opcode == crazy::WebSocketClient::OpCode::text) {
			received = data;
		}
		});

	CHECK(ws.connect("ws://127.0.0.1:" + std::to_string(port) + "/chat"));
	CHECK(ws.isConnected());

	ws.sendText("ping from client");
	ws.run();  // 阻塞，直到收到服务端 close 帧后返回

	CHECK(received == "welcome from server");
	CHECK(!ws.isConnected());

	serverThread.join();

	if (failures == 0) {
		std::cout << "websocket client e2e tests passed" << std::endl;
	}
#ifdef _WIN32
	WSACleanup();
#endif
	return failures == 0 ? 0 : 1;
}

#undef CHECK
