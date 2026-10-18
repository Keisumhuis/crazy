/**
 * @file test_decoder.cc
 * @brief Decoder 边界用例（空 / 分包 / 粘包 / 超长 / 非法长度）
 *
 * 帧格式（见 src/crazy/net/encoder.cpp / decoder.cpp）：
 *   [uint64_t size][uint64_t cmd][size - sizeof(cmd) 字节 body]
 * size 与 cmd 均为 memcpy 原始字节，故本文件一律用 memcpy 按本机字节序构造与解析。
 */
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "crazy/buffer.h"
#include "crazy/message_base.h"
#include "crazy/net/decoder.h"

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

namespace {

	// 只写长度字段（8 字节），用于构造非法长度 / 超长长度输入
	std::string encodeLength(uint64_t length) {
		std::string data(sizeof(length), '\0');
		std::memcpy(&data[0], &length, sizeof(length));
		return data;
	}

	// 完整帧：[size][cmd][body]
	std::string encodeFrame(uint64_t cmd, const std::string& body) {
		std::string data = encodeLength(static_cast<uint64_t>(sizeof(cmd)) + body.size());
		data.append(reinterpret_cast<const char*>(&cmd), sizeof(cmd));
		data.append(body);
		return data;
	}

	struct DecoderFixture {
		crazy::Buffer buffer;
		crazy::Decoder decoder{buffer};
		int finished = 0;
		int exceptions = 0;
		std::vector<uint64_t> cmds;
		std::vector<std::string> bodies;

		DecoderFixture() {
			decoder.registerParseFinishCallback([this](crazy::MessageBase::ptr message) {
				++finished;
				cmds.push_back(message->getCmd());
				bodies.push_back(message->getData());
			});
			decoder.registerParseExceptionCallback([this]() {
				++exceptions;
			});
		}

		void feed(const std::string& data) {
			buffer.append(data.data(), static_cast<uint32_t>(data.size()));
		}
	};

	// (1) 空输入
	void testEmptyInput() {
		DecoderFixture fixture;
		CHECK(fixture.decoder.getMaxMessageLength() == 10485760);

		bool threw = false;
		try {
			fixture.decoder.parse();
		}
		catch (...) {
			threw = true;
		}
		CHECK(!threw);
		CHECK(fixture.finished == 0);
		CHECK(fixture.exceptions == 0);
		CHECK(fixture.buffer.readableCount() == 0);
	}

	// (2) 分包：一条帧按 1 字节步长投喂，每次投喂后都 parse
	void testFragmentedFrame() {
		DecoderFixture fixture;
		const uint64_t cmd = 0x1122334455667788ULL;
		const std::string body = "fragmented-body";
		const std::string frame = encodeFrame(cmd, body);

		for (std::size_t index = 0; index < frame.size(); ++index) {
			fixture.feed(frame.substr(index, 1));
			fixture.decoder.parse();
		}

		CHECK(fixture.finished == 1);
		CHECK(fixture.exceptions == 0);
		CHECK(fixture.cmds[0] == cmd);
		CHECK(fixture.bodies[0] == body);
		CHECK(fixture.buffer.readableCount() == 0);
	}

	// (3) 粘包：一次投喂 3 条完整帧
	void testStickyFrames() {
		DecoderFixture fixture;
		const std::string frame1 = encodeFrame(101, "first");
		const std::string frame2 = encodeFrame(202, "second-frame");
		const std::string frame3 = encodeFrame(303, "");

		fixture.feed(frame1 + frame2 + frame3);
		fixture.decoder.parse();

		CHECK(fixture.finished == 3);
		CHECK(fixture.exceptions == 0);
		CHECK(fixture.cmds.size() == 3);
		CHECK(fixture.cmds[0] == 101);
		CHECK(fixture.cmds[1] == 202);
		CHECK(fixture.cmds[2] == 303);
		CHECK(fixture.bodies[0] == "first");
		CHECK(fixture.bodies[1] == "second-frame");
		CHECK(fixture.bodies[2].empty());
		CHECK(fixture.buffer.readableCount() == 0);
	}

	// (3') 粘包边界：先只投喂 8 字节长度字段，再投喂剩余字节
	void testSplitAtLengthField() {
		DecoderFixture fixture;
		const uint64_t cmd = 42;
		const std::string body = "split-length";
		const std::string frame = encodeFrame(cmd, body);

		fixture.feed(frame.substr(0, sizeof(uint64_t)));
		fixture.decoder.parse();
		CHECK(fixture.finished == 0);
		CHECK(fixture.exceptions == 0);

		fixture.feed(frame.substr(sizeof(uint64_t)));
		fixture.decoder.parse();
		CHECK(fixture.finished == 1);
		CHECK(fixture.exceptions == 0);
		CHECK(fixture.cmds[0] == cmd);
		CHECK(fixture.bodies[0] == body);
		CHECK(fixture.buffer.readableCount() == 0);
	}

	// (4) 超长报文
	void testOversizeFrame() {
		// f1: 长度 = getMaxMessageLength() + 1 = 10485761
		{
			DecoderFixture fixture;
			const uint64_t length = fixture.decoder.getMaxMessageLength() + 1;
			CHECK(length == 10485761ULL);

			fixture.feed(encodeLength(length));
			fixture.decoder.parse();
			CHECK(fixture.exceptions == 1);
			CHECK(fixture.finished == 0);
			CHECK(fixture.buffer.readableCount() == 0);
		}

		// f2: 长度 = UINT64_MAX
		{
			DecoderFixture fixture;
			fixture.feed(encodeLength(UINT64_MAX));
			fixture.decoder.parse();
			CHECK(fixture.exceptions == 1);
			CHECK(fixture.finished == 0);
			CHECK(fixture.buffer.readableCount() == 0);
		}

		// f3: setMaxMessageLength(1024) 后 1025 触发异常，1024 的合法帧正常完成
		{
			DecoderFixture fixture;
			fixture.decoder.setMaxMessageLength(1024);
			CHECK(fixture.decoder.getMaxMessageLength() == 1024);

			fixture.feed(encodeLength(1025));
			fixture.decoder.parse();
			CHECK(fixture.exceptions == 1);
			CHECK(fixture.finished == 0);
			CHECK(fixture.buffer.readableCount() == 0);

			const uint64_t cmd = 555;
			const std::string body(1024 - sizeof(cmd), 'x');
			fixture.feed(encodeFrame(cmd, body));
			fixture.decoder.parse();
			CHECK(fixture.finished == 1);
			CHECK(fixture.exceptions == 1);
			CHECK(fixture.cmds[0] == cmd);
			CHECK(fixture.bodies[0] == body);
			CHECK(fixture.buffer.readableCount() == 0);
		}
	}

	// (5) 非法长度
	void testIllegalLengthField() {
		// 长度字段 = 0
		{
			DecoderFixture fixture;
			fixture.feed(encodeLength(0));
			fixture.decoder.parse();
			CHECK(fixture.exceptions == 1);
			CHECK(fixture.finished == 0);
			CHECK(fixture.buffer.readableCount() == 0);
		}

		// 长度字段 = 7（< sizeof(uint64_t)），异常后可恢复
		{
			DecoderFixture fixture;
			fixture.feed(encodeLength(7));
			fixture.decoder.parse();
			CHECK(fixture.exceptions == 1);
			CHECK(fixture.finished == 0);
			CHECK(fixture.buffer.readableCount() == 0);

			fixture.decoder.reset();
			fixture.buffer.reset();

			const uint64_t cmd = 909;
			const std::string body = "recovered";
			fixture.feed(encodeFrame(cmd, body));
			fixture.decoder.parse();
			CHECK(fixture.finished == 1);
			CHECK(fixture.cmds[0] == cmd);
			CHECK(fixture.bodies[0] == body);
		}
	}

}  // namespace

int main() {
	testEmptyInput();
	testFragmentedFrame();
	testStickyFrames();
	testSplitAtLengthField();
	testOversizeFrame();
	testIllegalLengthField();

	if (failures == 0) {
		std::cout << "decoder tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}

#undef CHECK
