/**
 * @file test_encoder.cc
 * @brief Encoder 往返用例（长度字段 / 空 body / 同 Buffer 连续编码）
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
#include "crazy/net/encoder.h"

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

	crazy::MessageBase::ptr makeMessage(uint64_t cmd, const std::string& body) {
		auto message = std::make_shared<crazy::MessageBase>();
		message->setCmd(cmd);
		message->setData(body);
		return message;
	}

	uint64_t decodeLength(crazy::Buffer& buffer) {
		uint64_t length = 0;
		std::memcpy(&length, buffer.readBegin(), sizeof(length));
		return length;
	}

	struct Receiver {
		int finished = 0;
		int exceptions = 0;
		std::vector<uint64_t> cmds;
		std::vector<std::string> bodies;

		void attach(crazy::Decoder& decoder) {
			decoder.registerParseFinishCallback([this](crazy::MessageBase::ptr message) {
				++finished;
				cmds.push_back(message->getCmd());
				bodies.push_back(message->getData());
			});
			decoder.registerParseExceptionCallback([this]() {
				++exceptions;
			});
		}
	};

	// (a) body 非空：编码后的可读长度 = 长度字段(8) + cmd(8) + body
	void testStringifySize() {
		crazy::Buffer buffer;
		crazy::Encoder encoder(buffer);
		const std::string body = "payload";
		auto message = makeMessage(1001, body);

		encoder.stringify(message);
		CHECK(buffer.readableCount() == 8 + 8 + body.size());
		CHECK(decodeLength(buffer) == 8 + body.size());
	}

	// (b) 空 body：长度字段解出的值为 8，解码端恰好产出 1 条空 body 消息
	void testEmptyBody() {
		crazy::Buffer buffer;
		crazy::Encoder encoder(buffer);
		crazy::Decoder decoder(buffer);
		Receiver receiver;
		receiver.attach(decoder);

		auto message = makeMessage(7, "");
		encoder.stringify(message);
		CHECK(buffer.readableCount() == 16);
		CHECK(decodeLength(buffer) == 8);

		bool threw = false;
		try {
			decoder.parse();
		}
		catch (...) {
			threw = true;
		}
		CHECK(!threw);
		CHECK(receiver.finished == 1);
		CHECK(receiver.exceptions == 0);
		CHECK(receiver.cmds[0] == 7);
		CHECK(receiver.bodies[0].empty());
		CHECK(buffer.readableCount() == 0);
	}

	// (c) 同一 Buffer 连续编码两条消息，Decoder 依次解析出两条
	void testTwoMessagesInOneBuffer() {
		crazy::Buffer buffer;
		crazy::Encoder encoder(buffer);
		crazy::Decoder decoder(buffer);
		Receiver receiver;
		receiver.attach(decoder);

		encoder.stringify(makeMessage(11, "first"));
		encoder.stringify(makeMessage(22, "second"));
		CHECK(buffer.readableCount() == (8 + 8 + 5) + (8 + 8 + 6));

		decoder.parse();
		CHECK(receiver.finished == 2);
		CHECK(receiver.exceptions == 0);
		CHECK(receiver.cmds.size() == 2);
		CHECK(receiver.cmds[0] == 11);
		CHECK(receiver.cmds[1] == 22);
		CHECK(receiver.bodies[0] == "first");
		CHECK(receiver.bodies[1] == "second");
		CHECK(buffer.readableCount() == 0);
	}

}  // namespace

int main() {
	testStringifySize();
	testEmptyBody();
	testTwoMessagesInOneBuffer();

	if (failures == 0) {
		std::cout << "encoder tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}

#undef CHECK
