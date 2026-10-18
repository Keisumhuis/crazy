/**
 * @file test_https_tls_config.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTPS/TLS 配置用例，覆盖 AC-7 / AC-8.
 * @version 0.1
 * @date 2026-10-09
 *
 * @note 夹具 ini 写到当前工作目录（构建目录下）后用 `Config::LoadConfigFile` 显式加载，
 * 用完 `Config::EraseValue` 清理；**不修改 `tests/app.ini`**.
 * @note 断言一律使用自研 `check()`/`CHECK`（不使用 `assert`）.
 * @note AC-7 / AC-8③ 的口径（Lead 2026-10-10 修订，可用性优先）：显式 `tls_enable=true` 但
 * TLS 初始化失败（证书/私钥缺失或不可读、`tls_min_version` 非法、TLS 未编译等）时，
 * 服务**降级为明文继续监听**该端口并打印醒目 `[TLS ALERT]` 告警（含具体文件路径）。
 * 因此这里断言的是「**仍能监听且明文可用** + 告警文本含具体路径」，不再是「不监听」。
 * 告警文本通过旁挂的内存 appender 确定性捕获，失败原因另由 `HttpApplication::tlsInitError()` 查询。
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "crazy/config.h"
#include "crazy/http_application.h"
#include "crazy/logger.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_session.h"
#include "crazy/net/socket.h"
#include "crazy/net/tls.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

#ifndef CRAZY_TLS_TEST_DIR
#define CRAZY_TLS_TEST_DIR "tests/tls"
#endif

namespace {
	const char* const kServerCert = CRAZY_TLS_TEST_DIR "/server_cert.pem";
	const char* const kServerKey = CRAZY_TLS_TEST_DIR "/server_key.pem";
	const char* const kMissingCert = CRAZY_TLS_TEST_DIR "/no_such_cert.pem";
	const char* const kMissingKey = CRAZY_TLS_TEST_DIR "/no_such_key.pem";
	const int32_t kIoTimeoutMs = 3000;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	/**
	 * @brief 旁挂到 system logger 上的内存捕获 appender.
	 * @note 只**追加**一个 appender，不改动既有的 console/file appender；
	 * 用于确定性地断言启动告警的**文本内容**（不依赖日志文件路径、轮转或工作目录）。
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
	 * @brief 暴露 HttpApplication 的受保护接口用于测试.
	 */
	class TestHttpApplication final : public crazy::HttpApplication {
	public:
		using crazy::HttpApplication::HttpApplication;
		void startServerForTest() {
			startServer();
		}
		crazy::HttpServer::ptr serverForTest() {
			return ensureServer();
		}
	};

	/**
	 * @brief 把一段配置写到当前目录下的临时 ini 并显式加载.
	 */
	std::string writeConfig(const std::string& name, const std::string& content) {
		const std::string path = name;
		std::ofstream stream(path, std::ios::out | std::ios::trunc);
		check(stream.is_open(), "open temp ini", __LINE__);
		stream << content;
		stream.close();
		crazy::Config::LoadConfigFile(path);
		return path;
	}

	void removeConfig(const std::string& path) {
		std::remove(path.c_str());
	}

	void eraseKeys(const std::string& section) {
		crazy::Config::EraseValue(section, "port");
		crazy::Config::EraseValue(section, "address");
		crazy::Config::EraseValue(section, "tls_enable");
		crazy::Config::EraseValue(section, "tls_cert_file");
		crazy::Config::EraseValue(section, "tls_key_file");
		crazy::Config::EraseValue(section, "tls_min_version");
		crazy::Config::EraseValue(section, "tls_ca_file");
		crazy::Config::EraseValue(section, "tls_verify_peer");
	}

	/**
	 * @brief 取一个当前空闲的端口（bind 0 后立即释放）.
	 */
	uint16_t freePort() {
		crazy::Socket probe;
		check(probe.listen(0, "127.0.0.1"), "listen(0)", __LINE__);
		const std::string address = probe.localAddress();
		const uint16_t port = static_cast<uint16_t>(std::stoul(address.substr(address.rfind(':') + 1)));
		probe.close();
		return port;
	}

	/**
	 * @brief 以明文发一次请求并读回响应（同步驱动 accept + onReadEvent）.
	 * @note 只有当端口**确实仍在以明文监听**时，才能读到完整 HTTP 响应；
	 * 这正是 AC-7 / AC-8③ 要断言的「服务保持可用」。
	 */
	std::string plainExchange(TestHttpApplication& app, uint16_t port, const char* request) {
		crazy::Socket plain;
		check(plain.connect("127.0.0.1", port), "plain connect", __LINE__);
		int32_t timeout = kIoTimeoutMs;
		plain.setOption(SOL_SOCKET, SO_RCVTIMEO, timeout);
		check(plain.send(request, std::strlen(request)) > 0, "plain send", __LINE__);
		auto session = app.serverForTest()->acceptSession();
		check(session != nullptr, "acceptSession returned nullptr", __LINE__);
		session->onReadEvent();
		std::string received;
		char buffer[4096];
		for (;;) {
			const int32_t length = plain.recv(buffer, sizeof(buffer));
			if (length <= 0) {
				break;
			}
			received.append(buffer, static_cast<size_t>(length));
		}
		plain.close();
		return received;
	}

	/**
	 * @brief TLS 客户端的一次请求结果（跑在独立线程里，避免与 accept 互锁）.
	 */
	struct TlsClientResult {
		std::atomic<bool> done{ false };
		bool handshakeOk = false;
		bool ok = false;
		std::string version;
		std::string error;
	};

	void runTlsClient(uint16_t port, const std::string& caFile, TlsClientResult& result) {
		crazy::TlsOptions options;
		options.caFile = caFile;
		options.verifyPeer = true;
		options.minVersion = "1.2";
		std::string error;
		auto context = crazy::TlsContext::createClient(options, error);
		if (!context) {
			result.error = error;
			result.done.store(true);
			return;
		}
		auto socket = std::make_shared<crazy::TlsSocket>(context, crazy::TlsSocket::Side::client);
		socket->setVerifyHost("127.0.0.1");
		if (!socket->connect("127.0.0.1", port)) {
			result.error = "tcp connect failed";
			result.done.store(true);
			return;
		}
		if (!socket->handshake(error)) {
			result.error = error;
			socket->abort();
			result.done.store(true);
			return;
		}
		result.handshakeOk = true;
		result.version = socket->negotiatedVersion();

		const std::string request = "GET /ping HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n";
		size_t offset = 0;
		while (offset < request.size()) {
			const int32_t sent = socket->send(request.data() + offset, request.size() - offset);
			if (sent <= 0) {
				result.error = "tls send failed";
				socket->abort();
				result.done.store(true);
				return;
			}
			offset += static_cast<size_t>(sent);
		}

		crazy::HttpResponseParser parser;
		std::string buffer;
		char data[8192];
		while (!parser.isFinished() && !parser.hasError()) {
			if (buffer.empty()) {
				const int32_t received = socket->recv(data, sizeof(data));
				if (received <= 0) {
					result.error = "tls recv failed";
					break;
				}
				buffer.append(data, static_cast<size_t>(received));
			}
			const size_t consumed = parser.execute(buffer.data(), buffer.size());
			if (consumed > 0) {
				buffer.erase(0, consumed);
			}
		}
		auto response = parser.getResponse();
		result.ok = parser.isFinished() && response != nullptr
			&& response->status() == crazy::HttpStatus::OK && response->body() == "pong";
		socket->close();
		result.done.store(true);
	}

	void waitClient(TlsClientResult& result) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!result.done.load()) {
			check(std::chrono::steady_clock::now() < deadline, "tls client timeout", __LINE__);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main(int argc, char** argv) {
#ifndef CRAZY_TLS_ENABLED
	std::cout << "SKIP: TLS is not compiled in (CRAZY_ENABLE_TLS=OFF)" << std::endl;
	return 77;
#endif
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif
	// 清理上一次运行可能残留的临时 ini（Application 构造时会加载当前目录下的 *.ini）。
	// 注意：`Application::initSystem()` 会把工作目录切到**可执行文件所在目录**，
	// 而测试是在构造 Application 之后才写 ini 的，所以残留文件在可执行文件目录里；
	// 启动时的清理必须同时覆盖「当前目录」与「可执行文件目录」两处，
	// 否则残留的 `[HTTP]`/`[http]` 节会污染后续用例的配置输入（曾实测导致 AC-8① 连错端口）。
	const std::filesystem::path executableDir = std::filesystem::absolute(argv[0]).parent_path();
	const char* const kResidualConfigs[] = {
		"crazy_tls_ac7.ini",
		"crazy_tls_ac8_lower.ini",
		"crazy_tls_ac8_plain.ini",
		"crazy_tls_ac8_min_version.ini",
		"crazy_tls_plain_selfcheck.ini",
	};
	for (const char* name : kResidualConfigs) {
		removeConfig(name);
		removeConfig((executableDir / name).string());
	}

	// 旁挂内存捕获 appender：让「启动告警文本」可以被确定性断言
	auto logCapture = std::make_shared<CaptureAppender>();
	crazy::Singleton<crazy::LoggerManager>::Instance().getLogger("system")->addAppender(logCapture);

	// ============ AC-7：证书/私钥缺失 -> 降级明文继续监听 + 醒目告警（含具体路径） ============
	{
		TestHttpApplication app(argc, argv);
		app.registerHttpHandler<crazy::GET>("/ping", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("pong");
			});
		const uint16_t port = freePort();
		// 刻意使用 app.listen()（listenSet_ = true）：TLS 配置的读取不得因为 listenSet_ 被跳过
		app.listen(port, "127.0.0.1");
		const std::string ini = writeConfig("crazy_tls_ac7.ini",
			"[HTTP]\ntls_enable = true\ntls_cert_file = " + std::string(kMissingCert)
			+ "\ntls_key_file = " + std::string(kMissingKey) + "\n");
		CaptureAppender::clear();
		app.startServerForTest();

		// ① 失败原因可查询，且必须指名具体路径
		const std::string error = app.tlsInitError();
		CHECK(!error.empty());
		CHECK(error.find(kMissingCert) != std::string::npos);

		// ② 醒目告警必须真的打印在启动日志里：写明「降级为明文」+ 具体文件路径
		const std::string startedLog = CaptureAppender::captured();
		CHECK(startedLog.find("TLS ALERT") != std::string::npos);
		CHECK(startedLog.find("PLAINTEXT") != std::string::npos);
		CHECK(startedLog.find(kMissingCert) != std::string::npos);
		CHECK(startedLog.find("TLS 已启用但加载失败，现以明文提供服务") != std::string::npos);

		// ③ 服务保持可用：端口仍在监听，明文请求拿到正常 HTTP 响应
		const std::string received = plainExchange(app, port,
			"GET /ping HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
		CHECK(received.find("HTTP/1.1 200 OK") == 0);
		CHECK(received.find("pong") != std::string::npos);

		removeConfig(ini);
		eraseKeys("HTTP");
		std::cout << "AC-7: plaintext still served on port " << port
			<< ", tls init reason = " << error << std::endl;
	}

	// ================= AC-8①：小写 [http] + tls_enable=true -> 只接受 TLS =================
	{
		TestHttpApplication app(argc, argv);
		app.registerHttpHandler<crazy::GET>("/ping", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("pong");
			});
		const uint16_t port = freePort();
		const std::string ini = writeConfig("crazy_tls_ac8_lower.ini",
			"[http]\nport = " + std::to_string(port)
			+ "\naddress = 127.0.0.1\ntls_enable = true\ntls_cert_file = " + std::string(kServerCert)
			+ "\ntls_key_file = " + std::string(kServerKey) + "\n");
		app.startServerForTest();

		// 明文请求：连接被拒（握手失败），HTTP 层一个字节都没收到
		crazy::Socket plain;
		CHECK(plain.connect("127.0.0.1", port));
		int32_t timeout = kIoTimeoutMs;
		plain.setOption(SOL_SOCKET, SO_RCVTIMEO, timeout);
		const char* request = "GET /ping HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
		CHECK(plain.send(request, std::strlen(request)) > 0);
		auto rejected = app.serverForTest()->acceptSession();
		CHECK(rejected == nullptr);
		std::string plainReceived;
		char buffer[4096];
		for (;;) {
			const int32_t length = plain.recv(buffer, sizeof(buffer));
			if (length <= 0) {
				break;
			}
			plainReceived.append(buffer, static_cast<size_t>(length));
		}
		CHECK(plainReceived.find("HTTP/") == std::string::npos);
		plain.close();

		// 真 TLS 客户端：握手 + 200 pong
		TlsClientResult result;
		std::thread client(runTlsClient, port, std::string(kServerCert), std::ref(result));
		auto session = app.serverForTest()->acceptSession();
		CHECK(session != nullptr);
		session->onReadEvent();
		client.join();
		waitClient(result);
		CHECK(result.handshakeOk);
		CHECK(result.ok);
		CHECK(!result.version.empty());

		removeConfig(ini);
		eraseKeys("http");
		std::cout << "AC-8-1: [http] tls_enable=true, plain bytes = " << plainReceived.size()
			<< ", tls version = " << result.version << std::endl;
	}

	// ================= AC-8②：大写 [HTTP] + tls_enable=false -> 明文 HTTP =================
	{
		TestHttpApplication app(argc, argv);
		app.registerHttpHandler<crazy::GET>("/ping", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("pong");
			});
		const uint16_t port = freePort();
		const std::string ini = writeConfig("crazy_tls_ac8_plain.ini",
			"[HTTP]\nport = " + std::to_string(port)
			+ "\naddress = 127.0.0.1\ntls_enable = false\n");
		app.startServerForTest();

		crazy::Socket plain;
		CHECK(plain.connect("127.0.0.1", port));
		int32_t timeout = kIoTimeoutMs;
		plain.setOption(SOL_SOCKET, SO_RCVTIMEO, timeout);
		const char* request = "GET /ping HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
		CHECK(plain.send(request, std::strlen(request)) > 0);
		auto session = app.serverForTest()->acceptSession();
		CHECK(session != nullptr);
		session->onReadEvent();
		std::string received;
		char buffer[4096];
		for (;;) {
			const int32_t length = plain.recv(buffer, sizeof(buffer));
			if (length <= 0) {
				break;
			}
			received.append(buffer, static_cast<size_t>(length));
		}
		plain.close();
		CHECK(received.find("HTTP/1.1 200 OK") == 0);
		CHECK(received.find("pong") != std::string::npos);

		removeConfig(ini);
		eraseKeys("HTTP");
		std::cout << "AC-8-2: [HTTP] tls_enable=false, plaintext response ok" << std::endl;
	}

	// ============ AC-8③ + R7：非法 tls_min_version -> 降级明文 + 醒目告警 ============
	{
		TestHttpApplication app(argc, argv);
		app.registerHttpHandler<crazy::GET>("/ping", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("pong");
			});
		const uint16_t port = freePort();
		app.listen(port, "127.0.0.1");
		const std::string ini = writeConfig("crazy_tls_ac8_min_version.ini",
			"[HTTP]\ntls_enable = true\ntls_cert_file = " + std::string(kServerCert)
			+ "\ntls_key_file = " + std::string(kServerKey) + "\ntls_min_version = 1.1\n");
		CaptureAppender::clear();
		app.startServerForTest();

		// 与 AC-7 同一策略：非法 tls_min_version 也降级明文，而不是拒绝启动/不监听
		const std::string error = app.tlsInitError();
		CHECK(error.find("1.1") != std::string::npos);
		const std::string startedLog = CaptureAppender::captured();
		CHECK(startedLog.find("TLS ALERT") != std::string::npos);
		CHECK(startedLog.find("PLAINTEXT") != std::string::npos);
		CHECK(startedLog.find("1.1") != std::string::npos);

		const std::string received = plainExchange(app, port,
			"GET /ping HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
		CHECK(received.find("HTTP/1.1 200 OK") == 0);
		CHECK(received.find("pong") != std::string::npos);

		removeConfig(ini);
		eraseKeys("HTTP");
		std::cout << "AC-8-3: invalid tls_min_version degraded to plaintext, reason = " << error << std::endl;
	}

	// 明文路径未被 TLS 代码影响：rawExchange 读到的必须是 HTTP 响应（自检工具本身可用）
	{
		TestHttpApplication app(argc, argv);
		app.registerHttpHandler<crazy::GET>("/ping", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("pong");
			});
		const uint16_t port = freePort();
		const std::string ini = writeConfig("crazy_tls_plain_selfcheck.ini",
			"[HTTP]\nport = " + std::to_string(port) + "\naddress = 127.0.0.1\n");
		app.startServerForTest();
		crazy::Socket plain;
		CHECK(plain.connect("127.0.0.1", port));
		int32_t timeout = kIoTimeoutMs;
		plain.setOption(SOL_SOCKET, SO_RCVTIMEO, timeout);
		const char* request = "GET /ping HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
		CHECK(plain.send(request, std::strlen(request)) > 0);
		auto session = app.serverForTest()->acceptSession();
		CHECK(session != nullptr);
		session->onReadEvent();
		std::string received;
		char buffer[4096];
		for (;;) {
			const int32_t length = plain.recv(buffer, sizeof(buffer));
			if (length <= 0) {
				break;
			}
			received.append(buffer, static_cast<size_t>(length));
		}
		plain.close();
		CHECK(received.find("HTTP/1.1 200 OK") == 0);
		removeConfig(ini);
		eraseKeys("HTTP");
	}

	crazy::Singleton<crazy::LoggerManager>::Instance().getLogger("system")->removeAppender(logCapture);

#ifdef _WIN32
	WSACleanup();
#endif
	std::cout << "test_https_tls_config passed" << std::endl;
	return 0;
}

#undef CHECK
