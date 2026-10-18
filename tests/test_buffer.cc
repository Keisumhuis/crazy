/**
 * @file test_buffer.cc
 * @brief Buffer 边界用例（空 / 分包 / 粘包 / 超长 / 非法长度）
 */
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "crazy/buffer.h"

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

	// (a) 空输入
	void testEmptyInput() {
		crazy::Buffer buffer;
		CHECK(buffer.readableCount() == 0);

		// 零长度读取是合法空操作
		buffer.readed(0);
		CHECK(buffer.readableCount() == 0);
		buffer.written(0);
		CHECK(buffer.readableCount() == 0);

		// reset 之后仍可继续写入并读出
		buffer.reset();
		CHECK(buffer.readableCount() == 0);
		buffer.append('a');
		CHECK(buffer.readableCount() == 1);
		CHECK(buffer.readBegin()[0] == 'a');
		buffer.readed(1);
		CHECK(buffer.readableCount() == 0);
	}

	// (b) 分包：逐字节追加
	void testSplitAppend() {
		crazy::Buffer buffer;
		buffer.append('a');
		CHECK(buffer.readableCount() == 1);
		buffer.append('b');
		CHECK(buffer.readableCount() == 2);
		buffer.append('c');
		CHECK(buffer.readableCount() == 3);

		CHECK(buffer.readBegin()[0] == 'a');
		CHECK(buffer.readBegin()[1] == 'b');
		CHECK(buffer.readBegin()[2] == 'c');
		CHECK(std::string(buffer.readBegin(), buffer.readableCount()) == "abc");
	}

	// (c) 粘包：多种 append 后一次 readed 读走全部
	void testStickyAppend() {
		crazy::Buffer buffer;
		const std::vector<char> first{'1', '2'};
		buffer.append(first);
		CHECK(buffer.readableCount() == 2);

		const char second[] = "345";
		buffer.append(second, 3);
		CHECK(buffer.readableCount() == 5);

		buffer.append('6');
		CHECK(buffer.readableCount() == 6);
		CHECK(std::string(buffer.readBegin(), buffer.readableCount()) == "123456");

		buffer.readed(buffer.readableCount());
		CHECK(buffer.readableCount() == 0);
		CHECK(buffer.writableCount() > 0);
	}

	// (d) 超长：扩容后已有数据不变
	void testOversizeEnsure() {
		crazy::Buffer buffer(crazy::Buffer::kInitializeSize);
		const std::string prefix = "0123456789";
		buffer.append(prefix.data(), static_cast<uint32_t>(prefix.size()));
		CHECK(buffer.readableCount() == prefix.size());

		const uint32_t need = 2 * crazy::Buffer::kInitializeSize;
		buffer.ensureWritableCount(static_cast<int32_t>(need));
		CHECK(buffer.writableCount() >= need);
		CHECK(buffer.readableCount() == prefix.size());
		CHECK(std::string(buffer.readBegin(), buffer.readableCount()) == prefix);
	}

	// (e) 非法长度
	void testIllegalLength() {
		crazy::Buffer buffer;
		buffer.append('x');

		bool readThrew = false;
		try {
			buffer.readed(buffer.readableCount() + 1);
		}
		catch (const std::out_of_range&) {
			readThrew = true;
		}
		CHECK(readThrew);

		bool writeThrew = false;
		try {
			buffer.written(buffer.writableCount() + 1);
		}
		catch (const std::out_of_range&) {
			writeThrew = true;
		}
		CHECK(writeThrew);

		bool nullThrew = false;
		try {
			crazy::Buffer nullBuffer(nullptr, 4);
			(void)nullBuffer;
		}
		catch (const std::invalid_argument&) {
			nullThrew = true;
		}
		CHECK(nullThrew);
	}

}  // namespace

int main() {
	testEmptyInput();
	testSplitAppend();
	testStickyAppend();
	testOversizeEnsure();
	testIllegalLength();

	if (failures == 0) {
		std::cout << "buffer tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}

#undef CHECK
