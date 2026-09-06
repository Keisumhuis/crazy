/**
 * @file test_websocket.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief WebSocket 帧库与握手逻辑测试.
 * @version 0.1
 * @date 2026-09-06
 */

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/websocket_parser.h"

namespace {
	int failures = 0;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	/**
	 * @brief 帧解析测试上下文，记录回调触发情况与解码后的数据.
	 */
	struct FrameTestContext {
		bool headerCalled = false;
		bool endCalled = false;
		std::string body;
		websocket_flags flags = static_cast<websocket_flags>(0);
	};

	int onFrameHeader(websocket_parser* parser) {
		auto* ctx = static_cast<FrameTestContext*>(parser->data);
		ctx->headerCalled = true;
		ctx->flags = parser->flags;
		return 0;
	}

	int onFrameBody(websocket_parser* parser, const char* data, size_t length) {
		auto* ctx = static_cast<FrameTestContext*>(parser->data);
		// 客户端帧带 mask，需要解码；服务端帧则原样保留.
		std::string decoded(length, '\0');
		if (websocket_parser_has_mask(parser)) {
			websocket_parser_decode(decoded.data(), data, length, parser);
		}
		else {
			decoded.assign(data, length);
		}
		ctx->body.append(decoded);
		return 0;
	}

	int onFrameEnd(websocket_parser* parser) {
		auto* ctx = static_cast<FrameTestContext*>(parser->data);
		ctx->endCalled = true;
		return 0;
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
	// ---- websocket_calc_frame_size：帧大小计算 ----
	// 小帧：2 字节头 + 数据长度.
	CHECK(websocket_calc_frame_size(static_cast<websocket_flags>(WS_FIN | WS_OP_TEXT), 0) == 2);
	CHECK(websocket_calc_frame_size(static_cast<websocket_flags>(WS_FIN | WS_OP_TEXT), 125) == 127);
	// 126~65535：额外 2 字节扩展长度.
	CHECK(websocket_calc_frame_size(static_cast<websocket_flags>(WS_FIN | WS_OP_TEXT), 126) == 130);
	CHECK(websocket_calc_frame_size(static_cast<websocket_flags>(WS_FIN | WS_OP_TEXT), 65535) == 65539);
	// 65536+：额外 8 字节扩展长度.
	CHECK(websocket_calc_frame_size(static_cast<websocket_flags>(WS_FIN | WS_OP_TEXT), 65536) == 65546);
	// 带 mask 额外 4 字节.
	CHECK(websocket_calc_frame_size(static_cast<websocket_flags>(WS_FIN | WS_HAS_MASK | WS_OP_TEXT), 5) == 11);

	// ---- websocket_build_frame：服务端帧（无 mask）----
	{
		const std::string payload = "hello";
		const websocket_flags flags = static_cast<websocket_flags>(WS_FIN | WS_OP_TEXT);
		std::string frame(websocket_calc_frame_size(flags, payload.size()), '\0');
		const size_t written = websocket_build_frame(
			frame.data(), flags, nullptr, payload.data(), payload.size());
		CHECK(written == 7);
		CHECK(static_cast<unsigned char>(frame[0]) == 0x81);  // FIN + TEXT
		CHECK(static_cast<unsigned char>(frame[1]) == 0x05);  // 长度 5，无 mask
		CHECK(frame.substr(2) == payload);
	}

	// ---- websocket_build_frame：客户端帧（带 mask）----
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
		CHECK(static_cast<unsigned char>(frame[2]) == 0x01);  // mask[0]
		CHECK(static_cast<unsigned char>(frame[3]) == 0x02);
		CHECK(static_cast<unsigned char>(frame[4]) == 0x03);
		CHECK(static_cast<unsigned char>(frame[5]) == 0x04);
		// 数据被 XOR 编码
		CHECK(static_cast<unsigned char>(frame[6]) == static_cast<unsigned char>('h' ^ 0x01));
		CHECK(static_cast<unsigned char>(frame[7]) == static_cast<unsigned char>('e' ^ 0x02));
	}

	// ---- websocket_decode / websocket_encode：mask 编解码对称性 ----
	{
		const char mask[4] = { 0x11, 0x22, 0x33, 0x44 };
		const std::string original = "hello world";
		std::string encoded(original.size(), '\0');
		std::string decoded(original.size(), '\0');
		websocket_encode(encoded.data(), original.data(), original.size(), mask, 0);
		websocket_decode(decoded.data(), encoded.data(), encoded.size(), mask, 0);
		CHECK(decoded == original);
	}

	// ---- websocket_parser_execute：完整解析客户端帧 ----
	{
		FrameTestContext ctx;
		websocket_parser parser;
		websocket_parser_settings settings;
		websocket_parser_init(&parser);
		websocket_parser_settings_init(&settings);
		parser.data = &ctx;
		settings.on_frame_header = onFrameHeader;
		settings.on_frame_body = onFrameBody;
		settings.on_frame_end = onFrameEnd;

		const char mask[4] = { 0x01, 0x02, 0x03, 0x04 };
		const std::string payload = "hello";
		const websocket_flags flags = static_cast<websocket_flags>(WS_FIN | WS_HAS_MASK | WS_OP_TEXT);
		std::string frame(websocket_calc_frame_size(flags, payload.size()), '\0');
		const size_t written = websocket_build_frame(
			frame.data(), flags, mask, payload.data(), payload.size());
		frame.resize(written);

		const size_t consumed = websocket_parser_execute(&parser, &settings, frame.data(), frame.size());
		CHECK(consumed == frame.size());
		CHECK(ctx.headerCalled);
		CHECK(ctx.endCalled);
		CHECK(ctx.body == payload);  // 解码后应还原原始数据
		CHECK((ctx.flags & WS_OP_MASK) == WS_OP_TEXT);
		CHECK((ctx.flags & WS_FIN) != 0);
		CHECK((ctx.flags & WS_HAS_MASK) != 0);
	}

	// ---- 握手校验负向测试 ----
	{
		const std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
		crazy::HttpResponse response;
		response.setStatus(crazy::HttpStatus::SWITCHING_PROTOCOLS);
		response.headers().insert("Upgrade", "websocket");
		response.headers().insert("Connection", "Upgrade");
		response.headers().insert("Sec-WebSocket-Accept", "invalid-accept-value");
		CHECK(!response.verifyWebSocketAccept(key));  // 错误的 accept 应校验失败
	}

	if (failures == 0) {
		std::cout << "websocket tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}

#undef CHECK
