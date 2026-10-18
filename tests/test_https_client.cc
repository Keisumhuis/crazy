/**
 * @file test_https_client.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTPS / WSS 客户端用例，覆盖 AC-11（含 TLS 关闭时的显式失败）.
 * @version 0.1
 * @date 2026-10-09
 *
 * @note 本用例在 `CRAZY_ENABLE_TLS=OFF` 时同样构建：此时断言
 * `https://` / `wss://` **显式失败**（返回 `nullptr` / `false`），
 * 且**不建立任何 TCP 连接**、更不会明文发送请求。
 * @note 断言一律使用自研 `check()`/`CHECK`（不使用 `assert`）.
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "crazy/config.h"
#include "crazy/net/http/http_client.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_router.h"
#include "crazy/net/http/http_server.h"
#include "crazy/net/http/http_session.h"
#include "crazy/net/http/websocket_client.h"
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

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	std::string writeConfig(const std::string& path, const std::string& content) {
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
		crazy::Config::EraseValue(section, "tls_enable");
		crazy::Config::EraseValue(section, "tls_cert_file");
		crazy::Config::EraseValue(section, "tls_key_file");
		crazy::Config::EraseValue(section, "tls_min_version");
		crazy::Config::EraseValue(section, "tls_ca_file");
		crazy::Config::EraseValue(section, "tls_verify_peer");
	}

	uint16_t portOf(const std::string& localAddress) {
		check(localAddress.rfind(':') != std::string::npos, "localAddress has port", __LINE__);
		return static_cast<uint16_t>(std::stoul(localAddress.substr(localAddress.rfind(':') + 1)));
	}

	/**
	 * @brief 有界地判断监听 socket 上是否出现了新连接（select 超时即「没有」）.
	 */
	bool listenerHasConnection(crazy::Socket& listener, int32_t timeoutMs) {
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(listener.socket(), &readSet);
		timeval timeout = {};
		timeout.tv_sec = timeoutMs / 1000;
		timeout.tv_usec = (timeoutMs % 1000) * 1000;
		const int32_t result = select(static_cast<int32_t>(listener.socket()) + 1, &readSet, nullptr, nullptr, &timeout);
		return result > 0;
	}

	void registerPing(const crazy::HttpRouter::ptr& router) {
		router->add(crazy::HttpMethod::GET, "/ping",
			[](crazy::HttpRequest&, crazy::HttpResponse& response) {
				response.setBody("pong");
			});
	}

	struct PlainServerState {
		std::atomic<bool> ready{ false };
		std::atomic<int> requests{ 0 };
		std::atomic<int> failures{ 0 };
		uint16_t port = 0;
	};

	void runPlainServer(PlainServerState& state) {
		crazy::Socket listener;
		if (!listener.listen(0, "127.0.0.1")) {
			state.failures.fetch_add(1);
			state.ready.store(true);
			return;
		}
		state.port = portOf(listener.localAddress());
		state.ready.store(true);
		auto clientSocket = listener.accept();
		if (!clientSocket) {
			state.failures.fetch_add(1);
			return;
		}
		auto router = std::make_shared<crazy::HttpRouter>();
		registerPing(router);
		auto session = std::make_shared<crazy::HttpSession>(clientSocket, router);
		session->onReadEvent();
		state.requests.fetch_add(1);
	}

	void waitReady(std::atomic<bool>& ready, const char* what) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!ready.load()) {
			check(std::chrono::steady_clock::now() < deadline, what, __LINE__);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	/**
	 * @brief `https://` 客户端请求结果（跑在线程里，避免与服务端 accept 互锁）.
	 */
	struct HttpsResult {
		std::atomic<bool> done{ false };
		crazy::HttpResponse::ptr response;
	};

	void runHttpsRequest(crazy::HttpClient* client, const std::string& uri, HttpsResult& result) {
		result.response = client->request(crazy::HttpMethod::GET, uri, "", {});
		result.done.store(true);
	}

	void waitDone(std::atomic<bool>& done, const char* what) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!done.load()) {
			check(std::chrono::steady_clock::now() < deadline, what, __LINE__);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif
	removeConfig("crazy_tls_ac11.ini");
	removeConfig("crazy_tls_ac11_bad_ca.ini");

	// ================= 明文回归：http:// 必须照常工作（两种构建都跑） =================
	{
		PlainServerState state;
		std::thread server(runPlainServer, std::ref(state));
		waitReady(state.ready, "plain server ready timeout");
		CHECK(state.failures.load() == 0);

		crazy::HttpClient client;
		client.setHeader("Connection", "close");
		auto response = client.request(crazy::HttpMethod::GET,
			"http://127.0.0.1:" + std::to_string(state.port) + "/ping", "", {});
		server.join();
		CHECK(response != nullptr);
		CHECK(response->status() == crazy::HttpStatus::OK);
		CHECK(response->body() == "pong");
		std::cout << "AC-11(http regression): plain http ok" << std::endl;
	}

#ifdef CRAZY_TLS_ENABLED
	// ================= AC-11：`https://` 真的经 TLS 完成 =================
	{
		std::string error;
		crazy::TlsOptions options;
		options.certFile = kServerCert;
		options.keyFile = kServerKey;
		options.minVersion = "1.2";
		auto context = crazy::TlsContext::createServer(options, error);
		CHECK(context != nullptr);

		crazy::HttpServer server;
		CHECK(server.listen(0, "127.0.0.1", context));
		server.registerHttpHandler<crazy::GET>("/ping", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("pong");
			});
		const uint16_t port = portOf(server.listener()->localAddress());

		const std::string ini = writeConfig("crazy_tls_ac11.ini",
			"[HTTP]\ntls_ca_file = " + std::string(kServerCert) + "\ntls_verify_peer = true\n");

		crazy::HttpClient client;
		client.setHeader("Connection", "close");
		HttpsResult result;
		std::thread requestThread(runHttpsRequest, &client,
			"https://127.0.0.1:" + std::to_string(port) + "/ping", std::ref(result));

		// 服务端：accept + 握手（自己完成，以便读取协商版本）
		auto rawSocket = server.listener()->accept();
		CHECK(rawSocket != nullptr);
		auto tlsSocket = std::dynamic_pointer_cast<crazy::TlsSocket>(rawSocket);
		CHECK(tlsSocket != nullptr);
		CHECK(tlsSocket->handshake(error));
		const std::string negotiated = tlsSocket->negotiatedVersion();
		CHECK(!negotiated.empty());
		CHECK(!tlsSocket->negotiatedCipher().empty());

		auto session = std::make_shared<crazy::HttpSession>(tlsSocket, server.router());
		session->onReadEvent();
		requestThread.join();
		waitDone(result.done, "https request timeout");

		CHECK(result.response != nullptr);
		CHECK(result.response->status() == crazy::HttpStatus::OK);
		CHECK(result.response->body() == "pong");
		std::cout << "AC-11(https): status = " << static_cast<int>(result.response->status())
			<< ", body = " << result.response->body() << ", server tls version = " << negotiated << std::endl;
	}
#endif

#ifdef CRAZY_TLS_ENABLED
	// ================= AC-11：证书校验失败时 request() 返回 nullptr（绝不静默明文） =================
	{
		std::string error;
		crazy::TlsOptions options;
		options.certFile = kServerCert;
		options.keyFile = kServerKey;
		options.minVersion = "1.2";
		auto context = crazy::TlsContext::createServer(options, error);
		CHECK(context != nullptr);

		crazy::HttpServer server;
		CHECK(server.listen(0, "127.0.0.1", context));
		server.registerHttpHandler<crazy::GET>("/ping", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("pong");
			});
		const uint16_t port = portOf(server.listener()->localAddress());

		const std::string ini = writeConfig("crazy_tls_ac11_bad_ca.ini",
			"[HTTP]\ntls_ca_file = " + std::string(kOtherCert) + "\ntls_verify_peer = true\n");

		crazy::HttpClient client;
		client.setHeader("Connection", "close");
		HttpsResult result;
		std::thread requestThread(runHttpsRequest, &client,
			"https://127.0.0.1:" + std::to_string(port) + "/ping", std::ref(result));

		auto session = server.acceptSession();
		(void)session;
		requestThread.join();
		waitDone(result.done, "https bad ca timeout");
		CHECK(result.response == nullptr);
		std::cout << "AC-11(https bad ca): response == nullptr" << std::endl;
	}

	// ================= AC-11（O3 扩展）：`wss://` 真的经 TLS 完成 =================
	{
		std::string error;
		crazy::TlsOptions options;
		options.certFile = kServerCert;
		options.keyFile = kServerKey;
		options.minVersion = "1.2";
		auto context = crazy::TlsContext::createServer(options, error);
		CHECK(context != nullptr);

		crazy::HttpServer server;
		CHECK(server.listen(0, "127.0.0.1", context));
		const uint16_t port = portOf(server.listener()->localAddress());

		const std::string ini = writeConfig("crazy_tls_ac11.ini",
			"[HTTP]\ntls_ca_file = " + std::string(kServerCert) + "\ntls_verify_peer = true\n");

		struct WssResult {
			std::atomic<bool> done{ false };
			bool connected = false;
			std::string error;
		};
		WssResult wssResult;

		std::thread clientThread([&wssResult, port]() {
			crazy::WebSocketClient client;
			if (!client.connect("wss://127.0.0.1:" + std::to_string(port) + "/ws")) {
				wssResult.error = "wss connect failed";
				wssResult.done.store(true);
				return;
			}
			wssResult.connected = true;
			client.sendText("hello-tls");
			// 等待服务端读走这一帧后再关闭
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			client.sendClose();
			wssResult.done.store(true);
			});

		// 服务端：accept + 握手 + 处理 WebSocket 升级
		auto rawSocket = server.listener()->accept();
		CHECK(rawSocket != nullptr);
		auto tlsSocket = std::dynamic_pointer_cast<crazy::TlsSocket>(rawSocket);
		CHECK(tlsSocket != nullptr);
		CHECK(tlsSocket->handshake(error));
		CHECK(!tlsSocket->negotiatedVersion().empty());
		const std::string negotiated = tlsSocket->negotiatedVersion();

		std::string message;
		auto router = std::make_shared<crazy::HttpRouter>();
		auto session = std::make_shared<crazy::HttpSession>(tlsSocket, router);
		session->registerWebSocketMessageCallback(
			[&message](crazy::HttpSession::ptr, const std::string& data, crazy::HttpSession::WebSocketOpCode) {
				message = data;
			});
		session->onReadEvent();   // WebSocket 升级请求 -> 101
		session->onReadEvent();   // 读到客户端发来的文本帧
		clientThread.join();
		waitDone(wssResult.done, "wss client timeout");

		CHECK(wssResult.connected);
		CHECK(message == "hello-tls");
		std::cout << "AC-11(wss): connected = " << wssResult.connected
			<< ", version = " << negotiated << ", server received = " << message << std::endl;
	}
#else
	// ================= AC-11：TLS 关闭时 `https://` / `wss://` 显式失败 =================
	{
		crazy::Socket listener;
		CHECK(listener.listen(0, "127.0.0.1"));
		const uint16_t port = portOf(listener.localAddress());

		crazy::HttpClient client;
		auto response = client.request(crazy::HttpMethod::GET,
			"https://127.0.0.1:" + std::to_string(port) + "/ping", "", {});
		CHECK(response == nullptr);
		// 连 TCP 连接都不应发生：没有静默明文发送
		CHECK(!listenerHasConnection(listener, 500));

		crazy::WebSocketClient websocket;
		CHECK(!websocket.connect("wss://127.0.0.1:" + std::to_string(port) + "/ws"));
		CHECK(!listenerHasConnection(listener, 500));

		listener.close();
		std::cout << "AC-11(TLS off): https:// and wss:// failed explicitly, no plaintext connection" << std::endl;
	}
#endif

	eraseKeys("HTTP");
	eraseKeys("http");
	removeConfig("crazy_tls_ac11.ini");
	removeConfig("crazy_tls_ac11_bad_ca.ini");

#ifdef _WIN32
	WSACleanup();
#endif
	std::cout << "test_https_client passed" << std::endl;
	return 0;
}

#undef CHECK
