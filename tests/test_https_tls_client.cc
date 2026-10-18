/**
 * @file test_https_tls_client.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTPS 客户端证书校验用例，覆盖 AC-6.
 * @version 0.1
 * @date 2026-10-09
 *
 * @note 断言一律使用自研 `check()`/`CHECK`（不使用 `assert`）.
 * @note 端口一律由系统分配，等待全部有界.
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

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
	const char* const kOtherCert = CRAZY_TLS_TEST_DIR "/other_cert.pem";
	const char* const kExpiredCert = CRAZY_TLS_TEST_DIR "/expired_cert.pem";
	const char* const kExpiredKey = CRAZY_TLS_TEST_DIR "/expired_key.pem";
	const char* const kMissingCa = CRAZY_TLS_TEST_DIR "/no_such_ca.pem";

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	/**
	 * @brief 只接受一条连接并做一次 TLS 握手的服务端.
	 */
	struct ServerState {
		std::atomic<bool> ready{ false };
		std::atomic<int> accepted{ 0 };
		std::atomic<int> handshakeSuccess{ 0 };
		std::atomic<int> handshakeFailures{ 0 };
		std::atomic<int> failures{ 0 };
		uint16_t port = 0;
		std::string version;
		std::string cipher;
		std::string failureReason;
	};

	void runServer(ServerState& state, const char* certFile, const char* keyFile) {
		std::string error;
		crazy::TlsOptions options;
		options.certFile = certFile;
		options.keyFile = keyFile;
		options.minVersion = "1.2";
		auto context = crazy::TlsContext::createServer(options, error);
		if (!context) {
			std::cerr << "server context failed: " << error << std::endl;
			state.failures.fetch_add(1);
			state.ready.store(true);
			return;
		}
		crazy::TlsServerSocket listener(context);
		if (!listener.listen(0, "127.0.0.1")) {
			state.failures.fetch_add(1);
			state.ready.store(true);
			return;
		}
		const std::string address = listener.localAddress();
		state.port = static_cast<uint16_t>(std::stoul(address.substr(address.rfind(':') + 1)));
		state.ready.store(true);

		auto socket = listener.accept();
		if (!socket) {
			state.failures.fetch_add(1);
			return;
		}
		state.accepted.fetch_add(1);
		auto tlsSocket = std::dynamic_pointer_cast<crazy::TlsSocket>(socket);
		check(tlsSocket != nullptr, "accepted socket is TlsSocket", __LINE__);
		std::string handshakeError;
		if (tlsSocket->handshake(handshakeError)) {
			state.handshakeSuccess.fetch_add(1);
			state.version = tlsSocket->negotiatedVersion();
			state.cipher = tlsSocket->negotiatedCipher();
			tlsSocket->close();
		}
		else {
			state.handshakeFailures.fetch_add(1);
			state.failureReason = handshakeError;
			tlsSocket->close();
		}
	}

	void waitReady(ServerState& state) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!state.ready.load()) {
			check(std::chrono::steady_clock::now() < deadline, "server ready timeout", __LINE__);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	/**
	 * @brief 客户端按给定 CA 建立连接并握手.
	 * @return 握手成功返回 `true`；`contextCreated` 表示上下文是否创建成功.
	 */
	bool clientHandshake(uint16_t port, const std::string& caFile, bool& contextCreated, std::string& error) {
		crazy::TlsOptions options;
		options.caFile = caFile;
		options.verifyPeer = true;
		options.minVersion = "1.2";
		auto context = crazy::TlsContext::createClient(options, error);
		contextCreated = context != nullptr;
		if (!context) {
			return false;
		}
		auto socket = std::make_shared<crazy::TlsSocket>(context, crazy::TlsSocket::Side::client);
		socket->setVerifyHost("127.0.0.1");
		if (!socket->connect("127.0.0.1", port)) {
			error = "tcp connect failed";
			return false;
		}
		const bool result = socket->handshake(error);
		// 按正常客户端方式关闭：先有界地读走 TLS 1.3 的 session ticket，
		// 避免内核接收缓冲区仍有未读数据时 closesocket 触发 RST（对端会把连接看成被截断）
		if (result) {
			int32_t timeout = 200;
			socket->setOption(SOL_SOCKET, SO_RCVTIMEO, timeout);
			char drain[512];
			socket->recv(drain, sizeof(drain));
		}
		socket->close();
		return result;
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
#ifndef CRAZY_TLS_ENABLED
	std::cout << "SKIP: TLS is not compiled in (CRAZY_ENABLE_TLS=OFF)" << std::endl;
	return 77;
#endif
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

	// ---------------- AC-6①：信任夹具自签证书 -> 握手成功 ----------------
	{
		ServerState state;
		std::thread server(runServer, std::ref(state), kServerCert, kServerKey);
		waitReady(state);
		CHECK(state.failures.load() == 0);

		std::string error;
		bool contextCreated = false;
		const bool handshake = clientHandshake(state.port, kServerCert, contextCreated, error);
		server.join();

		CHECK(contextCreated);
		CHECK(handshake);
		if (state.handshakeFailures.load() != 0) {
			std::cerr << "server side handshake failure: " << state.failureReason << std::endl;
		}
		CHECK(state.handshakeSuccess.load() == 1);
		CHECK(!state.version.empty());
		CHECK(!state.cipher.empty());
		std::cout << "AC-6-1: handshake ok, version=" << state.version << ", cipher=" << state.cipher << std::endl;
	}

	// ---------------- AC-6②：信任另一张自签证书 -> 握手失败 ----------------
	{
		ServerState state;
		std::thread server(runServer, std::ref(state), kServerCert, kServerKey);
		waitReady(state);
		CHECK(state.failures.load() == 0);

		std::string error;
		bool contextCreated = false;
		const bool handshake = clientHandshake(state.port, kOtherCert, contextCreated, error);
		server.join();

		CHECK(contextCreated);
		CHECK(!handshake);
		CHECK(!error.empty());
		// 服务端一侧的结果不作断言：TLS 1.3 下服务端可能在收到客户端 alert 之前就认为握手完成
		CHECK(state.accepted.load() == 1);
		std::cout << "AC-6-2: handshake failed as expected, reason = " << error << std::endl;
	}

	// ---------------- AC-6③：CA 路径不存在 -> 上下文创建即失败 ----------------
	{
		crazy::TlsOptions options;
		options.caFile = kMissingCa;
		options.verifyPeer = true;
		options.minVersion = "1.2";
		std::string error;
		auto context = crazy::TlsContext::createClient(options, error);
		CHECK(context == nullptr);
		CHECK(!error.empty());
		// 失败原因必须指名具体路径
		CHECK(error.find(kMissingCa) != std::string::npos);
		std::cout << "AC-6-3: init failed as expected, reason = " << error << std::endl;
	}

	// ---------------- AC-6④：信任已过期自签证书 -> 握手失败 ----------------
	{
		ServerState state;
		std::thread server(runServer, std::ref(state), kExpiredCert, kExpiredKey);
		waitReady(state);
		CHECK(state.failures.load() == 0);

		std::string error;
		bool contextCreated = false;
		const bool handshake = clientHandshake(state.port, kExpiredCert, contextCreated, error);
		server.join();

		CHECK(contextCreated);
		CHECK(!handshake);
		CHECK(!error.empty());
		// 同上：只断言客户端一侧的判定结果
		CHECK(state.accepted.load() == 1);
		std::cout << "AC-6-4: handshake failed as expected, reason = " << error << std::endl;
	}

#ifdef _WIN32
	WSACleanup();
#endif
	std::cout << "test_https_tls_client passed" << std::endl;
	return 0;
}

#undef CHECK
