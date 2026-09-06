/**
 * @file test_websocket_client.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief WebSocket 客户端纯逻辑测试（握手与客户端帧构造）.
 * @version 0.1
 * @date 2026-09-06
 */

#include <cstdlib>
#include <iostream>
#include <string>

#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/websocket_parser.h"
#include "crazy/uri.h"

namespace {
	int failures = 0;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
	// ---- 客户端握手请求构造 ----
	const std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
	crazy::HttpRequest::ptr request =
		crazy::HttpRequest::createWebSocketRequest(crazy::Uri("ws://example.com/chat"), key);
	CHECK(request->method() == crazy::HttpMethod::GET);
	CHECK(request->isWebSocket());
	CHECK(request->isWebSocketHandshake());
	const auto keyHeader = request->headers().get("Sec-WebSocket-Key");
	CHECK(keyHeader && *keyHeader == key);

	// ---- 握手 accept 校验 ----
	CHECK(crazy::HttpResponse::computeWebSocketAccept(key) == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

	// ---- 客户端带 mask 帧构造 ----
	{
		const std::string payload = "hello";
		const char mask[4] = { 0x01, 0x02, 0x03, 0x04 };
		const websocket_flags flags = static_cast<websocket_flags>(WS_FIN | WS_HAS_MASK | WS_OP_TEXT);
		std::string frame(websocket_calc_frame_size(flags, payload.size()), '\0');
		const size_t written = websocket_build_frame(
			frame.data(), flags, mask, payload.data(), payload.size());
		CHECK(written == 11);  // 2 头 + 4 mask + 5 数据
		CHECK(static_cast<unsigned char>(frame[0]) == 0x81);
		CHECK(static_cast<unsigned char>(frame[1]) == 0x85);  // 0x80(mask) | 5
		CHECK(static_cast<unsigned char>(frame[2]) == 0x01);
		CHECK(static_cast<unsigned char>(frame[3]) == 0x02);
		CHECK(static_cast<unsigned char>(frame[4]) == 0x03);
		CHECK(static_cast<unsigned char>(frame[5]) == 0x04);
		// 数据被 XOR 编码
		CHECK(static_cast<unsigned char>(frame[6]) == static_cast<unsigned char>('h' ^ 0x01));
		CHECK(static_cast<unsigned char>(frame[7]) == static_cast<unsigned char>('e' ^ 0x02));
	}

	if (failures == 0) {
		std::cout << "websocket client tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}

#undef CHECK
