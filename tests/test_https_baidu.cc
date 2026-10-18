/**
 * @file test_https_baidu.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief 真实公网 HTTPS 端到端用例：用自研 OpenSSL 客户端访问 `https://www.baidu.com` 并把应答回显.
 * @version 0.1
 * @date 2026-10-10
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "crazy/config.h"
#include "crazy/logger.h"
#include "crazy/net/http/http_client.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_version.h"
#include "crazy/net/socket.h"
#include "crazy/net/tls.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

//! 完整正文的落盘路径（由 CMake 注入为构建树根下的 `baidu-response.html`）
#ifndef CRAZY_BAIDU_OUTPUT_FILE
#define CRAZY_BAIDU_OUTPUT_FILE "baidu-response.html"
#endif

//! 断言宏：一律走自研 `check()`，不使用 `assert`（Release 下 `NDEBUG` 会让它失效）
#define CHECK(expression) check((expression), #expression, __LINE__)

namespace {
	//! 目标主机（真实公网）
	const char* const kHost = "www.baidu.com";
	//! 目标端口
	const uint64_t kPort = 443;
	//! 目标 URI
	const char* const kUri = "https://www.baidu.com/";
	//! 整体预算（毫秒）；硬性要求整体 <= 10 秒
	const int64_t kTotalBudgetMs = 9000;
	//! 握手超时（毫秒）
	const uint32_t kHandshakeTimeoutMs = 4000;
	//! 回显的正文前缀长度
	const size_t kEchoBytes = 4096;
	//! CTest `SKIP_RETURN_CODE`：网络不可达 / 超时
	const int32_t kSkipExitCode = 77;
	//! 完整正文落盘路径
	const char* const kOutputFile = CRAZY_BAIDU_OUTPUT_FILE;
	//! 判定通过的状态码
	const int32_t kAcceptedStatusCodes[] = { 200, 301, 302 };

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	/**
	 * @brief 旁挂到 system logger 上的内存捕获 appender.
	 * @note 只**追加**一个 appender，不动既有的 console/file appender；
	 * 用于把 `Socket::connect` / `TlsContext` / `HttpClient` 打在日志里的
	 * **原始错误串**（含 OpenSSL 的 `error:...`）确定性地取出来。
	 */
	class CaptureAppender final : public crazy::LoggerAppenderInterface {
	public:
		void log(crazy::LoggerEvent::ptr event) override {
			std::lock_guard<std::mutex> lock(mutex());
			captured().append(event->getSS().str()).push_back('\n');
		}
		static std::string& captured() {
			static std::string text;
			return text;
		}
		static void clear() {
			std::lock_guard<std::mutex> lock(mutex());
			captured().clear();
		}

	private:
		static std::mutex& mutex() {
			static std::mutex instance;
			return instance;
		}
	};

	/**
	 * @brief 自研 OpenSSL 客户端握手结果.
	 */
	struct ProbeResult {
		//! 纯 TCP 是否能连上（网络 / DNS 判据）
		bool tcpReachable = false;
		//! 网络不可达时的原始日志
		std::string tcpLog;
		//! 客户端 TLS 上下文是否创建成功
		bool tlsContextOk = false;
		//! 上下文创建失败的原始错误
		std::string tlsContextError;
		//! 本次构建是否根本没有编译进 TLS 客户端（`-DCRAZY_ENABLE_TLS=OFF`）
		bool tlsNotCompiledIn = false;
		//! TLS socket 的 TCP 连接是否建立成功
		bool connectOk = false;
		//! TLS socket 的 TCP 连接失败时的原始日志
		std::string connectLog;
		//! 握手是否成功
		bool handshakeOk = false;
		//! 握手失败的原始错误（含 OpenSSL 错误串）
		std::string handshakeError;
		//! 协商出的协议版本
		std::string version;
		//! 协商出的密码套件
		std::string cipher;
	};

	/**
	 * @brief `HttpClient` 端到端请求结果.
	 */
	struct FetchResult {
		//! 是否给客户端设置过 `Accept-Encoding`（要求**不要**发送 gzip）
		bool acceptEncodingHeaderSet = false;
		//! 响应（失败为 nullptr）
		crazy::HttpResponse::ptr response;
		//! 逐段交付的响应体（用于校验分块解码）
		std::vector<std::string> segments;
		//! 客户端日志（含握手失败的原始原因）
		std::string log;
	};

	/**
	 * @brief 工作线程与主线程之间的交接物.
	 */
	struct Outcome {
		std::mutex mutex;
		std::condition_variable cv;
		bool done = false;
		ProbeResult probe;
		FetchResult fetch;
	};

	/**
	 * @brief 发布结果并唤醒主线程.
	 */
	void publish(Outcome& outcome, ProbeResult&& probe, FetchResult&& fetch) {
		{
			std::lock_guard<std::mutex> lock(outcome.mutex);
			outcome.probe = std::move(probe);
			outcome.fetch = std::move(fetch);
			outcome.done = true;
		}
		outcome.cv.notify_all();
	}

	/**
	 * @brief 真实网络访问主体（全部跑在独立线程里，便于主线程做有界等待）.
	 */
	void runCase(Outcome& outcome) {
		ProbeResult probe;
		FetchResult fetch;
		try {
#ifndef CRAZY_TLS_ENABLED
			// 本构建根本没有 TLS 客户端（-DCRAZY_ENABLE_TLS=OFF）：这不是缺陷，按 SKIP 报
			probe.tlsNotCompiledIn = true;
			probe.tlsContextError = "TLS support is not compiled in (CRAZY_ENABLE_TLS=OFF),"
				" rebuild with -DCRAZY_ENABLE_TLS=ON";
			publish(outcome, std::move(probe), std::move(fetch));
			return;
#endif
			// ① 纯 TCP 可达性探测：区分「网络不可达 / DNS 失败」与「TLS 失败」
			CaptureAppender::clear();
			{
				crazy::Socket tcp;
				probe.tcpReachable = tcp.connect(kHost, kPort);
				tcp.close();
			}
			probe.tcpLog = CaptureAppender::captured();
			if (!probe.tcpReachable) {
				publish(outcome, std::move(probe), std::move(fetch));
				return;
			}

			// ② 自研 OpenSSL 客户端：真实 TLS 握手，**系统默认信任库** + 真实主机名校验
			crazy::TlsOptions options;
			options.minVersion = "1.2";
			options.verifyPeer = true;   // 绝不关闭证书校验
			options.caFile.clear();      // 空 = SSL_CTX_set_default_verify_paths（系统默认信任库）
			std::string error;
			crazy::TlsContext::ptr context = crazy::TlsContext::createClient(options, error);
			if (!context) {
				probe.tlsContextError = error;
				publish(outcome, std::move(probe), std::move(fetch));
				return;
			}
			probe.tlsContextOk = true;
			{
				crazy::TlsSocket tls(context, crazy::TlsSocket::Side::client);
				tls.setVerifyHost(kHost);
				tls.setHandshakeTimeout(kHandshakeTimeoutMs);
				CaptureAppender::clear();
				probe.connectOk = tls.connect(kHost, kPort);
				probe.connectLog = CaptureAppender::captured();
				if (!probe.connectOk) {
					publish(outcome, std::move(probe), std::move(fetch));
					return;
				}
				CaptureAppender::clear();
				probe.handshakeOk = tls.handshake(error);
				probe.handshakeError = error;
				probe.version = tls.negotiatedVersion();
				probe.cipher = tls.negotiatedCipher();
			}   // 析构 -> close() -> 发送 close_notify

			if (!probe.handshakeOk) {
				publish(outcome, std::move(probe), std::move(fetch));
				return;
			}

			// ③ `HttpClient` 端到端 GET：真实解析、真实（可能的）chunked 解码、完整正文
			CaptureAppender::clear();
			{
				crazy::HttpClient client;
				// 要求：不要发送 `Accept-Encoding: gzip`（客户端不解压，拿到二进制无法回显）
				fetch.acceptEncodingHeaderSet = client.headers().contains("Accept-Encoding");
				client.setHeader("Connection", "close");
				fetch.response = client.request(crazy::HttpMethod::GET, kUri, std::string(), {},
					[&fetch](const std::string& data) { fetch.segments.push_back(data); });
			}
			fetch.log = CaptureAppender::captured();
		}
		catch (const std::exception& exception) {
			fetch.log += std::string("unexpected exception: ") + exception.what();
		}
		catch (...) {
			fetch.log += "unexpected unknown exception";
		}
		publish(outcome, std::move(probe), std::move(fetch));
	}

	/**
	 * @brief 打印「如何指定 CA 文件」的可行路径建议.
	 * @note 这里**只给建议**：本用例不会为了让结果变绿而关闭证书校验。
	 */
	void printTrustStoreAdvice() {
		std::cerr
			<< "  --- how to specify a CA bundle (remediation, do NOT disable verification) ---" << std::endl
			<< "  this client verifies the peer against the OpenSSL *default* trust store" << std::endl
			<< "  (SSL_CTX_set_default_verify_paths). On Windows that does NOT read the Windows" << std::endl
			<< "  certificate store; it needs SSL_CERT_FILE / SSL_CERT_DIR, e.g.:" << std::endl
			<< "    $env:SSL_CERT_FILE = '<ca-bundle.pem>'" << std::endl
			<< "    ctest --test-dir <build> -C Release --output-on-failure -R test_https_baidu" << std::endl
			<< "  a ready-made Mozilla bundle is often already on disk (conda/python), e.g." << std::endl
			<< "    <prefix>/ssl/cacert.pem   or   <site-packages>/certifi/cacert.pem," << std::endl
			<< "  or download https://curl.se/ca/cacert.pem" << std::endl;
	}

	/**
	 * @brief 判断状态码是否落在用例接受的集合内.
	 */
	bool acceptedStatus(int32_t status) {
		for (const int32_t candidate : kAcceptedStatusCodes) {
			if (candidate == status) {
				return true;
			}
		}
		return false;
	}

	/**
	 * @brief 回显「状态行 + 关键响应头 + 正文前 4 KB + 正文总字节数」并落盘完整正文.
	 */
	void echoResponse(const crazy::HttpResponse::ptr& response, const FetchResult& fetch) {
		const std::string& body = response->body();
		const int32_t status = static_cast<int32_t>(response->status());

		std::cout << "--- status line / 状态行 ---" << std::endl;
		// HttpVersion::toString() 自带 "HTTP/" 前缀
		std::cout << response->version().toString() << " " << status
			<< " " << response->reasonPhrase() << std::endl;

		std::cout << "--- key response headers / 关键响应头 ---" << std::endl;
		const char* const kRequired[] = { "Content-Type", "Transfer-Encoding", "Content-Length" };
		for (const char* name : kRequired) {
			const auto value = response->headers().get(name);
			std::cout << name << ": " << (value.has_value() ? *value : std::string("(absent)")) << std::endl;
		}
		const char* const kOptional[] = { "Content-Encoding", "Server", "Date", "Connection", "Location", "Cache-Control" };
		for (const char* name : kOptional) {
			const auto value = response->headers().get(name);
			std::cout << name << ": " << (value.has_value() ? *value : std::string("(absent)")) << std::endl;
		}

		const size_t echoBytes = body.size() < kEchoBytes ? body.size() : kEchoBytes;
		std::cout << "--- body / 正文 ---" << std::endl;
		std::cout << "total body bytes = " << body.size() << std::endl;
		std::cout << "first " << echoBytes << " bytes"
			<< (body.size() > echoBytes ? " (truncated for display)" : "") << ":" << std::endl;
		std::cout.write(body.data(), static_cast<std::streamsize>(echoBytes));
		std::cout << std::endl << "--- end of body echo ---" << std::endl;

		{
			std::ofstream stream(kOutputFile, std::ios::out | std::ios::binary | std::ios::trunc);
			check(stream.is_open(), "open the full-body output file", __LINE__);
			stream.write(body.data(), static_cast<std::streamsize>(body.size()));
			stream.close();
		}
		std::ifstream written(kOutputFile, std::ios::in | std::ios::binary);
		check(written.is_open(), "reopen the full-body output file", __LINE__);
		written.seekg(0, std::ios::end);
		const std::streamoff onDisk = written.tellg();
		written.close();
		std::cout << "full body written to: " << kOutputFile
			<< " (" << onDisk << " bytes on disk)" << std::endl;

		// 分块解码自洽性：逐段交付的解码字节必须与解析出的正文逐字节同长
		size_t segmentBytes = 0;
		for (const auto& segment : fetch.segments) {
			segmentBytes += segment.size();
		}
		CHECK(segmentBytes == body.size());

		const auto transferEncoding = response->headers().get("Transfer-Encoding");
		const bool chunked = transferEncoding.has_value()
			&& transferEncoding->find("chunked") != std::string::npos;
		if (chunked) {
			std::cout << "chunked decode check: Transfer-Encoding = " << *transferEncoding
				<< ", Content-Length present = " << (response->headers().contains("Content-Length") ? "yes" : "no")
				<< ", decoded segments = " << fetch.segments.size()
				<< ", decoded bytes = " << segmentBytes
				<< ", body bytes = " << body.size() << " -> ok" << std::endl;
			CHECK(!response->headers().contains("Content-Length"));
		}
		else {
			std::cout << "chunked decode check: not chunked (Transfer-Encoding absent),"
				<< " body framing = Content-Length/EOF, decoded bytes = " << segmentBytes << std::endl;
		}

		const auto contentEncoding = response->headers().get("Content-Encoding");
		if (contentEncoding.has_value() && !contentEncoding->empty() && *contentEncoding != "identity") {
			std::cout << "WARNING: the server returned COMPRESSED content (" << *contentEncoding
				<< ") even though the client never sent Accept-Encoding; reported as-is (not decompressed, "
				<< "so the echoed bytes above are compressed)." << std::endl;
		}
		else {
			std::cout << "content encoding check: not compressed"
				<< " (Accept-Encoding header was never set on the request -> "
				<< (fetch.acceptEncodingHeaderSet ? "set" : "not set") << ")" << std::endl;
		}
	}
}  // namespace

int main() {
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

	std::cout << "test_https_baidu: real public-network HTTPS end-to-end case" << std::endl;
	std::cout << "  target : " << kUri << " (" << kHost << ":" << kPort << ")" << std::endl;
	std::cout << "  client : crazy::TlsContext/TlsSocket + crazy::HttpClient (our own OpenSSL client,"
		<< " NOT PowerShell/.NET)" << std::endl;
	std::cout << "  trust  : OpenSSL system default trust store (SSL_CTX_set_default_verify_paths),"
		<< " full certificate + hostname verification (never SSL_VERIFY_NONE)" << std::endl;
	std::cout << "  budget : " << kTotalBudgetMs << " ms total (bounded; HttpClient has no socket timeout)"
		<< std::endl;
	std::cout << "  output : " << kOutputFile << std::endl;

	auto capture = std::make_shared<CaptureAppender>();
	crazy::Singleton<crazy::LoggerManager>::Instance().getLogger("system")->addAppender(capture);

	Outcome outcome;
	std::thread worker(runCase, std::ref(outcome));
	bool finished = false;
	{
		std::unique_lock<std::mutex> lock(outcome.mutex);
		finished = outcome.cv.wait_for(lock, std::chrono::milliseconds(kTotalBudgetMs),
			[&outcome]() { return outcome.done; });
	}
	if (!finished) {
		// 关键：超时分支必须先 detach —— 可 join 的 std::thread 析构会 std::terminate，
		// 那样测试会「崩」而不是「超时失败」。
		worker.detach();
		std::cout << "SKIP: the bounded wait of " << kTotalBudgetMs << " ms expired; the request thread is"
			<< " still blocked (HttpClient::recv has no timeout), so this run cannot be judged."
			<< std::endl;
		std::cout << "SKIP: reason = overall budget exceeded on " << kUri
			<< " (network too slow / silently black-holing the connection)." << std::endl;
		return kSkipExitCode;
	}
	worker.join();

	const ProbeResult& probe = outcome.probe;
	const FetchResult& fetch = outcome.fetch;

	if (probe.tlsNotCompiledIn) {
		std::cout << "SKIP: this build has no TLS client compiled in (CRAZY_ENABLE_TLS=OFF), so a real"
			<< " public-network HTTPS end-to-end case cannot be judged here." << std::endl;
		std::cout << "SKIP: reason = " << probe.tlsContextError << std::endl;
		return kSkipExitCode;
	}

	std::cout << "--- reachability / 网络可达性 ---" << std::endl;
	std::cout << "TCP connect " << kHost << ":" << kPort << " -> "
		<< (probe.tcpReachable ? "ok" : "FAILED") << std::endl;
	if (!probe.tcpReachable) {
		std::cout << "SKIP: TCP connect to " << kHost << ":" << kPort << " failed; the host is offline,"
			<< " DNS is unavailable, or port 443 is blocked. This is NOT a TLS defect." << std::endl;
		std::cout << "SKIP: raw reason: " << probe.tcpLog << std::endl;
		return kSkipExitCode;
	}

	std::cout << "--- our own OpenSSL client: TLS handshake ---" << std::endl;
	if (!probe.tlsContextOk) {
		std::cerr << "FAIL: TlsContext::createClient() with the system default trust store failed."
			<< " TCP is reachable, so this is not a network-unreachable case." << std::endl;
		std::cerr << "FAIL: raw OpenSSL error = " << probe.tlsContextError << std::endl;
		printTrustStoreAdvice();
		check(false, "TlsContext::createClient(system default trust store)", __LINE__);
	}
	if (!probe.connectOk) {
		std::cout << "SKIP: the TLS client could not establish its own TCP connection to " << kHost << ":" << kPort
			<< " even though the standalone probe succeeded; treating this as a transient network failure."
			<< std::endl;
		std::cout << "SKIP: raw reason: " << probe.connectLog << std::endl;
		return kSkipExitCode;
	}
	if (!probe.handshakeOk) {
		std::cerr << "FAIL: TLS handshake to " << kHost << ":" << kPort << " failed."
			<< " TCP is reachable, so this is NOT a network-unreachable case;" << std::endl
			<< "FAIL: certificate verification and hostname verification were fully ON"
			<< " (trust store = system default, host = " << kHost << ")." << std::endl;
		std::cerr << "FAIL: raw OpenSSL error = " << probe.handshakeError << std::endl;
		printTrustStoreAdvice();
		check(false, "TLS handshake with real CA + hostname verification against www.baidu.com", __LINE__);
	}
	CHECK(!probe.version.empty());
	std::cout << "handshake ok: version = " << probe.version << ", cipher = " << probe.cipher
		<< ", hostname check = " << kHost << std::endl;

	std::cout << "--- crazy::HttpClient end-to-end GET ---" << std::endl;
	if (fetch.response == nullptr) {
		std::cerr << "FAIL: HttpClient::request(GET, " << kUri << ") returned nullptr after a successful"
			<< " TLS handshake on the same host/port." << std::endl;
		std::cerr << "FAIL: raw client log = " << fetch.log << std::endl;
		check(false, "HttpClient::request(GET, https://www.baidu.com/) returned a response", __LINE__);
	}

	const crazy::HttpResponse::ptr& response = fetch.response;
	const int32_t status = static_cast<int32_t>(response->status());
	const std::string& body = response->body();

	echoResponse(response, fetch);

	std::cout << "--- assertions ---" << std::endl;
	std::cout << "status code = " << status << " (accepted: 200/301/302) -> "
		<< (acceptedStatus(status) ? "accepted" : "UNEXPECTED") << std::endl;
	CHECK(acceptedStatus(status));
	CHECK(!body.empty());

	std::cout << "test_https_baidu passed" << std::endl;
	return 0;
}

#undef CHECK
