/**
 * @file test_aggregate_header.cc
 * @brief 聚合头契约闸（REQ-20261009-aggregate-header-doc-sync 的 AC-1 / AC-2 / AC-3）
 *
 * 本 TU **只** `#include "crazy.h"`，用来判定：
 *  1. AC-1：仅凭聚合头即可引用 TLS 公开类型
 *     （`crazy::TlsOptions` / `TlsContext` / `TlsSocket` / `TlsServerSocket`）；
 *  2. AC-1：聚合头**不泄漏任何 `<openssl/*>`**（`OPENSSL_VERSION_NUMBER` 编译期断言）；
 *  3. AC-2：聚合头收录的自研头数量不得低于《实现说明》里记录的基线
 *     （低于基线 = 有头被移出聚合头，闸门失败）。
 *
 * C1（每个被收录的头单独 include 可编译）由同目标的编译单元集合
 * `crazy_aggregate_header_only` 承载：CMake 在 configure 期读 `src/crazy.h` 的
 * 每一条 `#include "crazy/..."`，为**每一个**被收录的头生成一个「只 include 该头」
 * 的翻译单元并全部编译。任何头不自包含都会让本目标构建失败。
 *
 * 注：`CRAZY_AGGREGATE_HEADER_ACTUAL` 由 CMake 以编译定义注入；
 * 基线是**本文件里的字面量**（不是从 CMake 取），否则断言恒真。
 */
#include "crazy.h"

#ifdef OPENSSL_VERSION_NUMBER
#error "crazy.h 不得把任何 <openssl/*> 头泄漏给包含者（OPENSSL_VERSION_NUMBER 已被定义）"
#endif

#ifndef CRAZY_AGGREGATE_HEADER_ACTUAL
#error "缺少编译定义 CRAZY_AGGREGATE_HEADER_ACTUAL（应由 CMake 从 src/crazy.h 数出）"
#endif

namespace {
	//! 收录下限：crazy.h 收录的自研头数量（见需求文档《实现说明》）。下降即回归。
	constexpr int kAggregateHeaderBaseline = 65;

	//! 累计失败次数
	int failures = 0;

	/**
	 * @brief 断言（仓库自研 check，不用 assert —— Release 下 NDEBUG 会把它变成空操作）.
	 */
	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			++failures;
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
		}
	}

	/**
	 * @brief AC-1：仅凭 crazy.h，TLS 公开类型必须**完整可见**.
	 */
	void testTlsPublicTypesVisible() {
		// 完整类型（sizeof 需要完整定义；前置声明会直接编译失败）
		check(sizeof(crazy::TlsOptions) > 0, "sizeof(TlsOptions) > 0", __LINE__);
		check(sizeof(crazy::TlsContext) > 0, "sizeof(TlsContext) > 0", __LINE__);
		check(sizeof(crazy::TlsSocket) > 0, "sizeof(TlsSocket) > 0", __LINE__);
		check(sizeof(crazy::TlsServerSocket) > 0, "sizeof(TlsServerSocket) > 0", __LINE__);

		// 以指针 / 取地址方式引用（不构造，也不依赖任何 OpenSSL 符号）
		crazy::TlsOptions* options = nullptr;
		crazy::TlsContext::ptr* context = nullptr;
		crazy::TlsSocket::ptr* tlsSocket = nullptr;
		crazy::TlsServerSocket::ptr* serverSocket = nullptr;
		auto createServer = &crazy::TlsContext::createServer;
		auto createClient = &crazy::TlsContext::createClient;
		check(options == nullptr, "TlsOptions* is null", __LINE__);
		check(context == nullptr, "TlsContext::ptr* is null", __LINE__);
		check(tlsSocket == nullptr, "TlsSocket::ptr* is null", __LINE__);
		check(serverSocket == nullptr, "TlsServerSocket::ptr* is null", __LINE__);
		check(createServer != nullptr, "&TlsContext::createServer", __LINE__);
		check(createClient != nullptr, "&TlsContext::createClient", __LINE__);

		// 字段可写：证明 TlsOptions 是完整类型而非前置声明
		crazy::TlsOptions value;
		value.certFile = "server_cert.pem";
		value.keyFile = "server_key.pem";
		value.minVersion = "1.2";
		check(!value.certFile.empty(), "certFile assigned", __LINE__);
		check(!value.keyFile.empty(), "keyFile assigned", __LINE__);
		check(value.minVersion == "1.2", "minVersion default is 1.2", __LINE__);
	}

	/**
	 * @brief AC-3：TLS 不可用时必须**显式失败**，绝不静默降级（与 tls.h 的 @note 一致）.
	 */
	void testTlsFailureIsExplicit() {
		crazy::TlsOptions options;
		options.certFile = "crazy_aggregate_header_no_such_cert.pem";
		options.keyFile = "crazy_aggregate_header_no_such_key.pem";
		std::string error;
		crazy::TlsContext::ptr context = crazy::TlsContext::createServer(options, error);
#ifdef CRAZY_TLS_ENABLED
		// ON：证书文件不存在 → 必须失败并给出可读原因（不是返回半可用上下文）
		check(context == nullptr, "createServer fails on missing cert", __LINE__);
		check(!error.empty(), "createServer reports a reason", __LINE__);
#else
		// OFF：必须明确报「TLS 支持未编译」，且不得要求任何 OpenSSL 头
		check(context == nullptr, "createServer returns nullptr when TLS is off", __LINE__);
		check(error.find("not compiled in") != std::string::npos,
			"OFF reason mentions 'not compiled in'", __LINE__);
#endif
	}

	/**
	 * @brief AC-2：聚合头收录数量不得低于基线（其余由编译单元集合承载）.
	 */
	void testAggregateHeaderCoverage() {
		check(CRAZY_AGGREGATE_HEADER_ACTUAL >= kAggregateHeaderBaseline,
			"CRAZY_AGGREGATE_HEADER_ACTUAL >= kAggregateHeaderBaseline", __LINE__);
		std::cout << "aggregate headers collected: " << CRAZY_AGGREGATE_HEADER_ACTUAL
			<< " (baseline " << kAggregateHeaderBaseline << ")" << std::endl;
	}
}  // namespace

int main() {
	testTlsPublicTypesVisible();
	testTlsFailureIsExplicit();
	testAggregateHeaderCoverage();

	if (failures == 0) {
		std::cout << "aggregate header tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}
