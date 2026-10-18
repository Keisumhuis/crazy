/**
 * @file test_https_tls_server.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTPS/TLS 服务端用例，覆盖 AC-3 / AC-4 / AC-5 / AC-9 / AC-10.
 * @version 0.1
 * @date 2026-10-09
 *
 * @note 断言一律使用自研 `check()`/`CHECK`（不使用 `assert`，Release 下会失效）.
 * @note 端口一律由系统分配（`listen(0, ...)`），所有等待都有界.
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_router.h"
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
	//! 1 MiB 响应体（AC-9）
	const size_t kBigBodySize = 1024 * 1024;
	//! AC-10 同一连接上的请求数
	const int kKeepAliveRequests = 100;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	std::string hostOf(const std::string& localAddress) {
		const size_t separator = localAddress.rfind(':');
		check(separator != std::string::npos, "localAddress has port", __LINE__);
		return localAddress.substr(separator + 1);
	}

	crazy::TlsContext::ptr makeServerContext(const std::string& minVersion) {
		std::string error;
		crazy::TlsOptions options;
		options.certFile = kServerCert;
		options.keyFile = kServerKey;
		options.minVersion = minVersion;
		auto context = crazy::TlsContext::createServer(options, error);
		check(context != nullptr, "TlsContext::createServer", __LINE__);
		return context;
	}

	void makeRouter(const crazy::HttpRouter::ptr& router) {
		router->add(crazy::HttpMethod::GET, "/ping",
			[](crazy::HttpRequest&, crazy::HttpResponse& response) {
				response.setBody("pong");
			});
	}

	/**
	 * @brief 服务端状态（断言用）.
	 */
	struct ServerState {
		//! 是否已经开始监听
		std::atomic<bool> ready{ false };
		//! accept 次数（AC-10 要求恰好 1 次）
		std::atomic<int> accepted{ 0 };
		//! 握手失败次数
		std::atomic<int> handshakeFailures{ 0 };
		//! HTTP 请求次数
		std::atomic<int> requests{ 0 };
		//! 意外错误次数
		std::atomic<int> failures{ 0 };
		//! 监听端口
		uint16_t port = 0;
		//! 协商出的协议版本
		std::string version;
		//! 协商出的密码套件
		std::string cipher;
	};

	/**
	 * @brief accept 一条连接并完成 TLS 握手.
	 * @return 握手成功返回 `TlsSocket`，失败返回 `nullptr`（并已关闭）.
	 */
	crazy::TlsSocket::ptr acceptOne(crazy::TlsServerSocket& listener, ServerState& state) {
		auto socket = listener.accept();
		if (!socket) {
			state.failures.fetch_add(1);
			return nullptr;
		}
		state.accepted.fetch_add(1);
		auto tlsSocket = std::dynamic_pointer_cast<crazy::TlsSocket>(socket);
		if (!tlsSocket) {
			state.failures.fetch_add(1);
			return nullptr;
		}
		std::string error;
		if (!tlsSocket->handshake(error)) {
			state.handshakeFailures.fetch_add(1);
			tlsSocket->close();
			return nullptr;
		}
		if (state.version.empty()) {
			state.version = tlsSocket->negotiatedVersion();
			state.cipher = tlsSocket->negotiatedCipher();
		}
		return tlsSocket;
	}

	/**
	 * @brief 建立一条到本机服务端的 TLS 客户端连接（校验服务端证书）.
	 */
	crazy::TlsSocket::ptr connectClient(uint16_t port, const std::string& caFile,
		const std::string& minVersion, const std::string& maxVersion, std::string& error) {
		crazy::TlsOptions options;
		options.caFile = caFile;
		options.verifyPeer = true;
		options.minVersion = minVersion;
		options.maxVersion = maxVersion;
		auto context = crazy::TlsContext::createClient(options, error);
		if (!context) {
			return nullptr;
		}
		auto socket = std::make_shared<crazy::TlsSocket>(context, crazy::TlsSocket::Side::client);
		socket->setVerifyHost("127.0.0.1");
		if (!socket->connect("127.0.0.1", port)) {
			error = "tcp connect failed";
			return nullptr;
		}
		if (!socket->handshake(error)) {
			socket->abort();
			return nullptr;
		}
		return socket;
	}

	/**
	 * @brief 一条 TLS 连接上的原始 HTTP 往返（支持 keep-alive 复用）.
	 */
	struct RawHttpConnection {
		crazy::TlsSocket::ptr socket;
		std::string buffer;

		bool send(const std::string& raw, std::string& error) {
			size_t offset = 0;
			while (offset < raw.size()) {
				const int32_t sent = socket->send(raw.data() + offset, raw.size() - offset);
				if (sent <= 0) {
					error = "tls send failed";
					return false;
				}
				offset += static_cast<size_t>(sent);
			}
			return true;
		}

		crazy::HttpResponse::ptr receive(std::string& error) {
			crazy::HttpResponseParser parser;
			char data[8192];
			int32_t guard = 0;
			while (!parser.isFinished() && !parser.hasError()) {
				if (buffer.empty()) {
					const int32_t received = socket->recv(data, sizeof(data));
					if (received <= 0) {
						error = "tls recv failed, result = " + std::to_string(received);
						return nullptr;
					}
					buffer.append(data, static_cast<size_t>(received));
				}
				const size_t consumed = parser.execute(buffer.data(), buffer.size());
				if (consumed > 0) {
					buffer.erase(0, consumed);
				}
				else if (++guard > 16) {
					error = "parser made no progress";
					return nullptr;
				}
			}
			if (!parser.isFinished()) {
				error = "incomplete http response";
				return nullptr;
			}
			return parser.getResponse();
		}

		crazy::HttpResponse::ptr get(const std::string& target, std::string& error) {
			const std::string request = "GET " + target + " HTTP/1.1\r\nHost: 127.0.0.1\r\n"
				"Connection: keep-alive\r\n\r\n";
			if (!send(request, error)) {
				return nullptr;
			}
			return receive(error);
		}
	};

	/**
	 * @brief 起一个 TLS 监听并把端口写回状态.
	 */
	bool startListener(crazy::TlsServerSocket& listener, ServerState& state) {
		if (!listener.listen(0, "127.0.0.1")) {
			state.failures.fetch_add(1);
			state.ready.store(true);
			return false;
		}
		state.port = static_cast<uint16_t>(std::stoul(hostOf(listener.localAddress())));
		state.ready.store(true);
		return true;
	}

	/**
	 * @brief 等待 ready 标志，最多 5 秒.
	 */
	void waitReady(ServerState& state) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!state.ready.load()) {
			check(std::chrono::steady_clock::now() < deadline, "server ready timeout", __LINE__);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	// ------------------------------------------------------------------
	// AC-3：真实 TCP + 真实 TLS 握手，GET /ping -> 200 pong
	// ------------------------------------------------------------------
	void runPingServer(ServerState& state) {
		auto context = makeServerContext("1.2");
		crazy::TlsServerSocket listener(context);
		if (!startListener(listener, state)) {
			return;
		}
		auto tlsSocket = acceptOne(listener, state);
		if (!tlsSocket) {
			return;
		}
		auto router = std::make_shared<crazy::HttpRouter>();
		makeRouter(router);
		auto session = std::make_shared<crazy::HttpSession>(tlsSocket, router);
		session->onReadEvent();
		state.requests.fetch_add(1);
	}

	// ------------------------------------------------------------------
	// AC-4：畸形明文连接被拒绝，服务端随后仍能服务合法 TLS 连接
	// ------------------------------------------------------------------
	void runGarbageThenValidServer(ServerState& state) {
		auto context = makeServerContext("1.2");
		crazy::TlsServerSocket listener(context);
		if (!startListener(listener, state)) {
			return;
		}
		// 第一条：非 TLS 畸形字节，握手必须失败
		auto garbage = acceptOne(listener, state);
		if (garbage) {
			// 握手意外成功：读一次，绝不能产出 HTTP 响应
			char buffer[1024];
			garbage->recv(buffer, sizeof(buffer));
			garbage->close();
		}
		// 第二条：合法 TLS 连接必须照常服务
		auto tlsSocket = acceptOne(listener, state);
		if (!tlsSocket) {
			return;
		}
		auto router = std::make_shared<crazy::HttpRouter>();
		makeRouter(router);
		auto session = std::make_shared<crazy::HttpSession>(tlsSocket, router);
		session->onReadEvent();
		state.requests.fetch_add(1);
	}

	// ------------------------------------------------------------------
	// AC-5：客户端最高版本低于服务端最低版本 -> 握手失败，无 HTTP 响应
	// ------------------------------------------------------------------
	void runVersionFloorServer(ServerState& state) {
		auto context = makeServerContext("1.3");
		crazy::TlsServerSocket listener(context);
		if (!startListener(listener, state)) {
			return;
		}
		auto tlsSocket = acceptOne(listener, state);
		if (tlsSocket) {
			// 握手意外成功：读一次，绝不能产出 HTTP 响应
			char buffer[1024];
			tlsSocket->recv(buffer, sizeof(buffer));
			tlsSocket->close();
		}
	}

	// ------------------------------------------------------------------
	// AC-9：1 MiB 响应体逐字节一致 + close_notify 与截断可区分
	// ------------------------------------------------------------------
	std::string buildBigBody() {
		std::string body;
		body.reserve(kBigBodySize);
		for (size_t i = 0; i < kBigBodySize; ++i) {
			body.push_back(static_cast<char>('a' + (i * 31 + i / 7) % 26));
		}
		return body;
	}

	void runBigBodyServer(ServerState& state, const std::string& body) {
		auto context = makeServerContext("1.2");
		crazy::TlsServerSocket listener(context);
		if (!startListener(listener, state)) {
			return;
		}
		auto tlsSocket = acceptOne(listener, state);
		if (!tlsSocket) {
			return;
		}
		auto router = std::make_shared<crazy::HttpRouter>();
		router->add(crazy::HttpMethod::GET, "/big",
			[&body](crazy::HttpRequest&, crazy::HttpResponse& response) {
				response.setBody(body);
			});
		auto session = std::make_shared<crazy::HttpSession>(tlsSocket, router);
		session->onReadEvent();
		state.requests.fetch_add(1);
	}

	void runCloseNotifyServer(ServerState& state) {
		auto context = makeServerContext("1.2");
		crazy::TlsServerSocket listener(context);
		if (!startListener(listener, state)) {
			return;
		}
		auto tlsSocket = acceptOne(listener, state);
		if (!tlsSocket) {
			return;
		}
		// 正常关闭：发送 close_notify
		tlsSocket->close();
	}

	void runTruncatedServer(ServerState& state) {
		auto context = makeServerContext("1.2");
		crazy::TlsServerSocket listener(context);
		if (!startListener(listener, state)) {
			return;
		}
		auto tlsSocket = acceptOne(listener, state);
		if (!tlsSocket) {
			return;
		}
		// 异常关闭：不发 close_notify 直接断开
		tlsSocket->abort();
	}

	// ------------------------------------------------------------------
	// AC-10：同一连接 100 个请求，accept 恰好 1 次
	// ------------------------------------------------------------------
	void runKeepAliveServer(ServerState& state) {
		auto context = makeServerContext("1.2");
		crazy::TlsServerSocket listener(context);
		if (!startListener(listener, state)) {
			return;
		}
		auto tlsSocket = acceptOne(listener, state);
		if (!tlsSocket) {
			return;
		}
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<int> counter{ 0 };
		router->add(crazy::HttpMethod::GET, "/count",
			[&counter](crazy::HttpRequest&, crazy::HttpResponse& response) {
				response.setBody(std::to_string(counter.fetch_add(1) + 1));
			});
		auto session = std::make_shared<crazy::HttpSession>(tlsSocket, router);
		for (int i = 0; i < kKeepAliveRequests; ++i) {
			session->onReadEvent();
			state.requests.fetch_add(1);
		}
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

	// ---------------- AC-3 ----------------
	{
		ServerState state;
		std::thread server(runPingServer, std::ref(state));
		waitReady(state);
		CHECK(state.failures.load() == 0);

		std::string error;
		auto client = connectClient(state.port, kServerCert, "1.2", "", error);
		CHECK(client != nullptr);
		CHECK(client->handshakeDone());

		RawHttpConnection connection;
		connection.socket = client;
		auto response = connection.get("/ping", error);
		CHECK(response != nullptr);
		CHECK(response->status() == crazy::HttpStatus::OK);
		CHECK(response->body() == "pong");
		client->close();
		server.join();

		CHECK(state.accepted.load() == 1);
		CHECK(state.handshakeFailures.load() == 0);
		CHECK(state.requests.load() == 1);
		// 协商版本与密码套件必须非空：证明真的完成了 TLS 握手而不是明文穿过去
		CHECK(!state.version.empty());
		CHECK(!state.cipher.empty());
		CHECK(client->negotiatedVersion() == state.version);
		std::cout << "AC-3: version=" << state.version << ", cipher=" << state.cipher << std::endl;
	}

	// ---------------- AC-4 ----------------
	{
		ServerState state;
		std::thread server(runGarbageThenValidServer, std::ref(state));
		waitReady(state);

		// 非 TLS 畸形字节
		std::string received;
		{
			crazy::Socket garbageClient;
			CHECK(garbageClient.connect("127.0.0.1", state.port));
			int32_t timeout = 5000;
			garbageClient.setOption(SOL_SOCKET, SO_RCVTIMEO, timeout);
			const char* garbage = "GARBAGE\r\n\r\n";
			CHECK(garbageClient.send(garbage, std::strlen(garbage)) > 0);
			char buffer[1024];
			for (;;) {
				const int32_t length = garbageClient.recv(buffer, sizeof(buffer));
				if (length <= 0) {
					break;
				}
				received.append(buffer, static_cast<size_t>(length));
			}
			garbageClient.close();
		}
		// HTTP 层一个字节都没收到
		CHECK(received.find("HTTP/") == std::string::npos);

		// 服务端仍能服务下一个合法 TLS 连接
		std::string error;
		auto client = connectClient(state.port, kServerCert, "1.2", "", error);
		CHECK(client != nullptr);
		RawHttpConnection connection;
		connection.socket = client;
		auto response = connection.get("/ping", error);
		CHECK(response != nullptr);
		CHECK(response->status() == crazy::HttpStatus::OK);
		CHECK(response->body() == "pong");
		client->close();
		server.join();

		CHECK(state.accepted.load() == 2);
		CHECK(state.handshakeFailures.load() == 1);
		CHECK(state.requests.load() == 1);
		std::cout << "AC-4: garbage bytes received by client = " << received.size()
			<< ", handshake failures = " << state.handshakeFailures.load() << std::endl;
	}

	// ---------------- AC-5 ----------------
	{
		ServerState state;
		std::thread server(runVersionFloorServer, std::ref(state));
		waitReady(state);

		std::string error;
		// 客户端最高只允许 TLS 1.2，服务端最低要求 1.3
		auto client = connectClient(state.port, kServerCert, "1.2", "1.2", error);
		CHECK(client == nullptr);
		CHECK(!error.empty());
		server.join();

		CHECK(state.accepted.load() == 1);
		// 不产生任何 HTTP 响应
		CHECK(state.requests.load() == 0);
		std::cout << "AC-5: handshake failed as expected, reason = " << error << std::endl;
	}

	// ---------------- AC-9 ----------------
	{
		const std::string bigBody = buildBigBody();
		ServerState state;
		std::thread server(runBigBodyServer, std::ref(state), std::cref(bigBody));
		waitReady(state);

		std::string error;
		auto client = connectClient(state.port, kServerCert, "1.2", "", error);
		CHECK(client != nullptr);
		RawHttpConnection connection;
		connection.socket = client;
		auto response = connection.get("/big", error);
		CHECK(response != nullptr);
		CHECK(response->status() == crazy::HttpStatus::OK);
		CHECK(response->body().size() == kBigBodySize);
		CHECK(response->body() == bigBody);
		client->close();
		server.join();
		CHECK(state.requests.load() == 1);
		std::cout << "AC-9(big body): received " << response->body().size()
			<< " bytes, byte-identical = " << (response->body() == bigBody) << std::endl;
	}

	// ---------------- AC-9：close_notify vs 截断 ----------------
	{
		ServerState notifyState;
		std::thread notifyServer(runCloseNotifyServer, std::ref(notifyState));
		waitReady(notifyState);

		std::string error;
		auto client = connectClient(notifyState.port, kServerCert, "1.2", "", error);
		CHECK(client != nullptr);
		char buffer[64];
		const int32_t result = client->recv(buffer, sizeof(buffer));
		notifyServer.join();
		CHECK(result == 0);
		std::cout << "AC-9(close_notify): recv returned " << result << std::endl;
	}

	{
		ServerState truncateState;
		std::thread truncateServer(runTruncatedServer, std::ref(truncateState));
		waitReady(truncateState);

		std::string error;
		auto client = connectClient(truncateState.port, kServerCert, "1.2", "", error);
		CHECK(client != nullptr);
		char buffer[64];
		const int32_t result = client->recv(buffer, sizeof(buffer));
		truncateServer.join();
		CHECK(result < 0);
		std::cout << "AC-9(truncated): recv returned " << result << std::endl;
	}

	// ---------------- AC-10 ----------------
	{
		ServerState state;
		std::thread server(runKeepAliveServer, std::ref(state));
		waitReady(state);

		std::string error;
		auto client = connectClient(state.port, kServerCert, "1.2", "", error);
		CHECK(client != nullptr);
		RawHttpConnection connection;
		connection.socket = client;
		for (int i = 1; i <= kKeepAliveRequests; ++i) {
			auto response = connection.get("/count", error);
			CHECK(response != nullptr);
			CHECK(response->status() == crazy::HttpStatus::OK);
			CHECK(response->body() == std::to_string(i));
		}
		client->close();
		server.join();

		// 一条 TLS 连接 = 一次 accept
		CHECK(state.accepted.load() == 1);
		CHECK(state.requests.load() == kKeepAliveRequests);
		std::cout << "AC-10: requests = " << state.requests.load()
			<< ", accepted = " << state.accepted.load() << std::endl;
	}

#ifdef _WIN32
	WSACleanup();
#endif
	std::cout << "test_https_tls_server passed" << std::endl;
	return 0;
}

#undef CHECK
