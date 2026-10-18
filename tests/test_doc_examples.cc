/**
 * @file test_doc_examples.cc
 * @brief 文档示例编译闸（REQ-20261009-aggregate-header-doc-sync 的 AC-4 / AC-5 / AC-6 / AC-7）
 *
 * 本测试**真的编译** `doc/zh/*.md` 与 `doc/en/*.md` 里每一个「带 `#include` 的 C++
 * 代码块」：CMake 在 configure 期调用 `tests/gen_doc_examples.cmake`，为每个这样的块
 * 生成等价 TU（块内 `#include` 全部提到顶部；语句序列式块包一层函数体），再全部编进
 * OBJECT 目标 `crazy_doc_examples`。**任何一个示例编译不过，本目标构建就失败**。
 *
 * 本文件自身的运行期断言负责编译期断言做不到的两件事：
 *  1. 防「静默抽样」：被覆盖的块数不得低于《实现说明》里记录的**字面**基线
 *     （基线写在本文件里，不由 CMake 提供 —— 否则断言恒真）；
 *  2. 防「页面漏采」：REQ §1.3 A/B/C/D 类点名的页面必须真的出现在被覆盖页面集合里
 *     （CMake 对每个页面注入 `CRAZY_DOC_PAGE_<page>_ZH/_EN` 为 1 或 0）。
 */
#include <cstdlib>
#include <iostream>

// 实际覆盖数量由 CMake 注入
#ifndef CRAZY_DOC_EXAMPLE_ACTUAL_ZH
#error "缺少编译定义 CRAZY_DOC_EXAMPLE_ACTUAL_ZH"
#endif
#ifndef CRAZY_DOC_EXAMPLE_ACTUAL_EN
#error "缺少编译定义 CRAZY_DOC_EXAMPLE_ACTUAL_EN"
#endif

namespace {
	//! 覆盖下限：doc/zh 中「带 #include 的 C++ 代码块」数量（快照基线，见《实现说明》）。
	constexpr int kDocExampleBaselineZh = 48;
	//! 覆盖下限：doc/en 同上。
	constexpr int kDocExampleBaselineEn = 49;

	//! 累计失败次数
	int failures = 0;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			++failures;
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
		}
	}

	//! 页面是否在 zh 与 en 两侧都进了编译闸.
	bool covered(int zh, int en) {
		return zh != 0 && en != 0;
	}
}  // namespace

//! 逐页覆盖断言：REQ §1.3 A/B/C/D 类点名的页面必须在闸内（zh 与 en 都要）.
#define REQUIRE_PAGE(page_id)                                                        \
	do {                                                                             \
		check(covered(CRAZY_DOC_PAGE_##page_id##_ZH, CRAZY_DOC_PAGE_##page_id##_EN),  \
			"doc page covered (zh+en): " #page_id, __LINE__);                         \
	} while (false)

int main() {
	check(CRAZY_DOC_EXAMPLE_ACTUAL_ZH >= kDocExampleBaselineZh,
		"CRAZY_DOC_EXAMPLE_ACTUAL_ZH >= kDocExampleBaselineZh", __LINE__);
	check(CRAZY_DOC_EXAMPLE_ACTUAL_EN >= kDocExampleBaselineEn,
		"CRAZY_DOC_EXAMPLE_ACTUAL_EN >= kDocExampleBaselineEn", __LINE__);

	// A 类（聚合头扩头后本应自然可编译）
	REQUIRE_PAGE(daemon);

	// B 类（用了日志宏却缺 crazy/logger.h）
	REQUIRE_PAGE(base64);
	REQUIRE_PAGE(config);
	REQUIRE_PAGE(date_time);
	REQUIRE_PAGE(decoder);
	REQUIRE_PAGE(file_lock);
	REQUIRE_PAGE(http_client);
	REQUIRE_PAGE(key_value_pair);
	REQUIRE_PAGE(local_socket);
	REQUIRE_PAGE(md5);
	REQUIRE_PAGE(mmap_interface);
	REQUIRE_PAGE(mmap_vector);
	REQUIRE_PAGE(mvcc);
	REQUIRE_PAGE(mysql);
	REQUIRE_PAGE(selector);
	REQUIRE_PAGE(smtp);
	REQUIRE_PAGE(socket);
	REQUIRE_PAGE(thread_pool);
	REQUIRE_PAGE(time_zone);
	REQUIRE_PAGE(uri);
	REQUIRE_PAGE(utilities);

	// C 类（符号缺头）
	REQUIRE_PAGE(buffer);
	REQUIRE_PAGE(protocol);
	REQUIRE_PAGE(reflection);
	REQUIRE_PAGE(clickhouse);

	// D 类（整块无 include / 引用页内未定义符号）
	REQUIRE_PAGE(service_actor);
	REQUIRE_PAGE(telnet_service);
	REQUIRE_PAGE(overview);
	REQUIRE_PAGE(integration);

	std::cout << "doc examples compiled: zh=" << CRAZY_DOC_EXAMPLE_ACTUAL_ZH
		<< " (baseline " << kDocExampleBaselineZh << "), en=" << CRAZY_DOC_EXAMPLE_ACTUAL_EN
		<< " (baseline " << kDocExampleBaselineEn << ")" << std::endl;

	if (failures == 0) {
		std::cout << "doc example tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}

#undef REQUIRE_PAGE
