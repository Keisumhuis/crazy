/**
 * @file test_http_chunked_evi.cc
 * @brief evi 独立验证用例（REQ-20261009-http-chunked）.
 *
 * 目的：不复用 keisum 的断言，独立抓取线上原始字节、独立测量错误路径耗时、
 * 独立覆盖 keisum 未覆盖的边界（beginChunked 之后再插 Content-Length 的兜底、
 * Connection: keep-alive 的正向 shouldKeepAlive、分块未终止时不得处理后续请求）。
 *
 * 说明：本文件只做加法，不修改任何既有测试。
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "crazy/net/http/http_client.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_router.h"
#include "crazy/net/http/http_session.h"
#include "crazy/net/http/websocket_client.h"
#include "crazy/net/socket.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

namespace {
	const int64_t kBudgetMs = 5000;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "EVI check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	uint16_t parsePort(const std::string& address) {
		const size_t separator = address.rfind(':');
		if (separator == std::string::npos) {
			return 0;
		}
		return static_cast<uint16_t>(std::stoul(address.substr(separator + 1)));
	}

	crazy::Socket::ptr makeListener(uint16_t& port) {
		auto listener = std::make_shared<crazy::Socket>();
		if (!listener->listen(0, "127.0.0.1")) {
			std::cerr << "listen failed" << std::endl;
			std::exit(1);
		}
		port = parsePort(listener->localAddress());
		return listener;
	}

	void setRecvTimeout(crazy::Socket& socket, int milliseconds) {
#ifdef _WIN32
		DWORD timeout = static_cast<DWORD>(milliseconds);
#else
		struct timeval timeout;
		timeout.tv_sec = milliseconds / 1000;
		timeout.tv_usec = (milliseconds % 1000) * 1000;
#endif
		socket.setOption(SOL_SOCKET, SO_RCVTIMEO, timeout);
	}

	std::string uri(uint16_t port, const std::string& path) {
		return "http://127.0.0.1:" + std::to_string(port) + path;
	}

	std::string toLower(const std::string& input) {
		std::string result = input;
		for (char& c : result) {
			if (c >= 'A' && c <= 'Z') {
				c = static_cast<char>(c - 'A' + 'a');
			}
		}
		return result;
	}

	bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
		return toLower(haystack).find(toLower(needle)) != std::string::npos;
	}

	std::string readRequestHeader(const crazy::Socket::ptr& client) {
		std::string request;
		char buffer[4096];
		while (request.find("\r\n\r\n") == std::string::npos) {
			const int32_t length = client->recv(buffer, sizeof(buffer));
			if (length <= 0) {
				break;
			}
			request.append(buffer, static_cast<size_t>(length));
		}
		return request;
	}

	std::string readUntil(crazy::Socket& socket, const std::string& marker, int64_t budgetMs) {
		std::string received;
		char buffer[4096];
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budgetMs);
		while (received.find(marker) == std::string::npos &&
			std::chrono::steady_clock::now() < deadline) {
			const int32_t length = socket.recv(buffer, sizeof(buffer));
			if (length <= 0) {
				break;
			}
			received.append(buffer, static_cast<size_t>(length));
		}
		return received;
	}

	// 人类可读的原始字节转义（用于把真实报文贴进测试报告）
	std::string escapeBytes(const std::string& raw) {
		std::string out;
		for (const char c : raw) {
			if (c == '\r') {
				out += "\\r";
			}
			else if (c == '\n') {
				out += "\\n\n";
			}
			else if (c == '\\') {
				out += "\\\\";
			}
			else {
				out += c;
			}
		}
		return out;
	}

	std::string hexDump(const std::string& raw) {
		static const char digits[] = "0123456789abcdef";
		std::string out;
		for (const char c : raw) {
			const unsigned char byte = static_cast<unsigned char>(c);
			out += digits[byte >> 4];
			out += digits[byte & 0xF];
			out += ' ';
		}
		return out;
	}

	// 框架服务端：带总时限地驱动会话（避免无界阻塞）
	void runFrameworkServerBounded(const crazy::Socket::ptr& listener, const crazy::HttpRouter::ptr& router,
		const std::atomic<bool>& stop, int64_t budgetMs) {
		auto client = listener->accept();
		if (!client) {
			return;
		}
		setRecvTimeout(*client, 100);
		auto session = std::make_shared<crazy::HttpSession>(client, router);
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budgetMs);
		while (!stop.load() && std::chrono::steady_clock::now() < deadline) {
			session->onReadEvent();
		}
	}

	struct RawResult {
		crazy::HttpResponse::ptr response;
		bool callbackCalled = false;
		int64_t elapsedMs = 0;
	};

	// 一次性裸 socket 服务端：发送指定字节后立即关闭
	RawResult requestRawServer(const std::string& responseBytes, const std::string& path) {
		uint16_t port = 0;
		auto listener = makeListener(port);
		std::thread server([listener, responseBytes]() {
			auto client = listener->accept();
			if (!client) {
				return;
			}
			setRecvTimeout(*client, 2000);
			readRequestHeader(client);
			client->send(responseBytes.data(), responseBytes.size());
			client->close();
		});

		RawResult result;
		crazy::HttpClient client;
		const auto begin = std::chrono::steady_clock::now();
		result.response = client.request(crazy::HttpMethod::GET, uri(port, path), "",
			[&result](crazy::HttpResponse::ptr) { result.callbackCalled = true; });
		result.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - begin).count();
		server.join();
		return result;
	}
}  // namespace

#define EVI_CHECK(expression) check((expression), #expression, __LINE__)

int main() {
#ifdef _WIN32
	WSADATA wsaData;
	EVI_CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

	// ---------------------------------------------------------------
	// [1] AC-1：抓取框架分块响应的**原始字节**并打印
	// ---------------------------------------------------------------
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> handled{ false };
		EVI_CHECK(router->add(crazy::HttpMethod::GET, "/stream",
			[&handled](crazy::HttpRequest&, crazy::HttpResponse& response) {
				auto deferred = response.defer();
				deferred->beginChunked();
				deferred->writeChunk("Hello");
				deferred->writeChunk(" ");
				deferred->writeChunk("World");
				deferred->endChunked();
				handled.store(true);
			}));
		std::thread server(runFrameworkServerBounded, listener, router, std::cref(handled), 5000);

		crazy::Socket client;
		EVI_CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, 2000);
		const std::string request =
			"GET /stream HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Connection: keep-alive\r\n"
			"\r\n";
		EVI_CHECK(client.send(request.data(), request.size()) == static_cast<int32_t>(request.size()));

		const std::string received = readUntil(client, "0\r\n\r\n", kBudgetMs);
		std::cout << "===== [AC-1] RAW BYTES FROM FRAMEWORK SERVER (" << received.size()
			<< " bytes) =====" << std::endl;
		std::cout << escapeBytes(received) << std::endl;
		std::cout << "----- hex -----" << std::endl;
		std::cout << hexDump(received) << std::endl;
		std::cout << "===== END RAW =====" << std::endl;

		EVI_CHECK(received.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
		const size_t headerEnd = received.find("\r\n\r\n");
		EVI_CHECK(headerEnd != std::string::npos);
		const std::string head = received.substr(0, headerEnd + 4);
		const std::string body = received.substr(headerEnd + 4);

		EVI_CHECK(head.find("\r\nTransfer-Encoding: chunked\r\n") != std::string::npos);
		EVI_CHECK(!containsIgnoreCase(head, "Content-Length"));
		// 原始帧序列必须与 RFC 7230 4.1 完全一致
		EVI_CHECK(body == "5\r\nHello\r\n1\r\n \r\n5\r\nWorld\r\n0\r\n\r\n");
		EVI_CHECK(received.size() >= 2 && received.compare(received.size() - 5, 5, "0\r\n\r\n") == 0);

		client.close();
		server.join();
	}

	// ---------------------------------------------------------------
	// [2] AC-2：合法 chunked 响应 → 完整 body；Connection 两个方向
	// ---------------------------------------------------------------
	{
		const std::string rawKeepAlive =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Connection: keep-alive\r\n"
			"\r\n"
			"5\r\nHello\r\n"
			"1\r\n \r\n"
			"5\r\nWorld\r\n"
			"0\r\n\r\n";
		const RawResult keepAlive = requestRawServer(rawKeepAlive, "/raw-ka");
		EVI_CHECK(keepAlive.response != nullptr);
		EVI_CHECK(keepAlive.response->status() == crazy::HttpStatus::OK);
		EVI_CHECK(keepAlive.response->body() == "Hello World");
		EVI_CHECK(keepAlive.response->shouldKeepAlive() == true);
		std::cout << "[AC-2] Connection: keep-alive -> shouldKeepAlive()="
			<< keepAlive.response->shouldKeepAlive() << " body='" << keepAlive.response->body()
			<< "' elapsedMs=" << keepAlive.elapsedMs << std::endl;

		const std::string rawClose =
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Connection: close\r\n"
			"\r\n"
			"b\r\nfirst-chunk\r\n"
			"c\r\nsecond-chunk\r\n"
			"0\r\n\r\n";
		const RawResult closeMode = requestRawServer(rawClose, "/raw-close");
		EVI_CHECK(closeMode.response != nullptr);
		EVI_CHECK(closeMode.callbackCalled);
		EVI_CHECK(closeMode.response->body() == "first-chunksecond-chunk");
		EVI_CHECK(closeMode.response->shouldKeepAlive() == false);

		crazy::HttpResponseParser parser;
		parser.execute(rawClose);
		EVI_CHECK(!parser.hasError());
		EVI_CHECK(parser.isFinished());
		EVI_CHECK(parser.getResponse()->body() == "first-chunksecond-chunk");
	}

	// ---------------------------------------------------------------
	// [3] AC-4：非法 chunk 长度 —— 独立计时，必须在预算内返回
	// ---------------------------------------------------------------
	{
		const std::string raw =
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"Z\r\nillegal\r\n"
			"0\r\n\r\n";
		crazy::HttpResponseParser parser;
		parser.execute(raw);
		EVI_CHECK(parser.hasError());
		EVI_CHECK(!parser.isFinished());

		const RawResult result = requestRawServer(raw, "/evi-illegal");
		EVI_CHECK(result.response == nullptr);
		EVI_CHECK(!result.callbackCalled);
		EVI_CHECK(result.elapsedMs <= kBudgetMs);
		std::cout << "[AC-4] illegal chunk size -> response=nullptr callbackCalled="
			<< result.callbackCalled << " elapsedMs=" << result.elapsedMs << std::endl;
	}

	// ---------------------------------------------------------------
	// [4] AC-5：缺终止块 / 中途断开 —— 独立计时
	// ---------------------------------------------------------------
	{
		const std::string noTerminator =
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"5\r\nHello\r\n";
		const RawResult first = requestRawServer(noTerminator, "/evi-no-term");
		EVI_CHECK(first.response == nullptr);
		EVI_CHECK(!first.callbackCalled);
		EVI_CHECK(first.elapsedMs <= kBudgetMs);
		std::cout << "[AC-5a] missing terminator -> nullptr elapsedMs=" << first.elapsedMs << std::endl;

		const std::string midChunk =
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"5\r\nHel";
		const RawResult second = requestRawServer(midChunk, "/evi-mid-chunk");
		EVI_CHECK(second.response == nullptr);
		EVI_CHECK(!second.callbackCalled);
		EVI_CHECK(second.elapsedMs <= kBudgetMs);
		std::cout << "[AC-5b] closed mid-chunk -> nullptr elapsedMs=" << second.elapsedMs << std::endl;
	}

	// ---------------------------------------------------------------
	// [5] AC-6：TE 与 CL 同时出现 = 解析错误；且回调不被调用
	// ---------------------------------------------------------------
	{
		const std::string raw =
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Content-Length: 5\r\n"
			"\r\n"
			"5\r\nHello\r\n"
			"0\r\n\r\n";
		crazy::HttpResponseParser parser;
		parser.execute(raw);
		EVI_CHECK(parser.hasError());
		EVI_CHECK(!parser.isFinished());
		EVI_CHECK(parser.getResponse() == nullptr || parser.getResponse()->body().empty());

		const RawResult result = requestRawServer(raw, "/evi-conflict");
		EVI_CHECK(result.response == nullptr);
		EVI_CHECK(!result.callbackCalled);
		EVI_CHECK(result.elapsedMs <= kBudgetMs);
		std::cout << "[AC-6a] TE+CL -> hasError, request()=nullptr elapsedMs=" << result.elapsedMs << std::endl;
	}

	// ---------------------------------------------------------------
	// [6] AC-6② / 兜底：beginChunked 之后再插 Content-Length，
	//     线上字节仍不得同时出现两者（toString 兜底路径）
	// ---------------------------------------------------------------
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> handled{ false };
		EVI_CHECK(router->add(crazy::HttpMethod::GET, "/late-cl",
			[&handled](crazy::HttpRequest&, crazy::HttpResponse& response) {
				auto deferred = response.defer();
				deferred->beginChunked();
				deferred->headers().insert("Content-Length", "5");  // 迟到的 CL
				deferred->writeChunk("Hello");
				deferred->endChunked();
				handled.store(true);
			}));
		std::thread server(runFrameworkServerBounded, listener, router, std::cref(handled), 5000);

		crazy::Socket client;
		EVI_CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, 2000);
		const std::string request =
			"GET /late-cl HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
		EVI_CHECK(client.send(request.data(), request.size()) == static_cast<int32_t>(request.size()));
		const std::string received = readUntil(client, "0\r\n\r\n", kBudgetMs);
		std::cout << "[AC-6b] late Content-Length, raw = " << escapeBytes(received) << std::endl;
		EVI_CHECK(received.find("HTTP/1.1 200") == 0);
		EVI_CHECK(received.find("Transfer-Encoding: chunked\r\n") != std::string::npos);
		EVI_CHECK(!containsIgnoreCase(received, "Content-Length"));
		EVI_CHECK(received.find("5\r\nHello\r\n0\r\n\r\n") != std::string::npos);
		client.close();
		server.join();
	}

	// ---------------------------------------------------------------
	// [7] Q3：分块未写出终止块时，同一连接上的后续请求不得被处理
	//     驱动方式：每次 onReadEvent() 都在数据已就绪时调用（不依赖 recv 超时），
	//     以排除 "超时导致会话关闭" 的混淆因素。
	// ---------------------------------------------------------------
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> streamHandled{ false };
		std::atomic<bool> nextHandled{ false };
		std::atomic<bool> firstRead{ false };
		std::atomic<bool> secondRead{ false };
		EVI_CHECK(router->add(crazy::HttpMethod::GET, "/open-stream",
			[&streamHandled](crazy::HttpRequest&, crazy::HttpResponse& response) {
				auto deferred = response.defer();
				deferred->beginChunked();
				deferred->writeChunk("partial");
				// 故意不调用 endChunked()：连接应保持占用
				streamHandled.store(true);
			}));
		EVI_CHECK(router->add(crazy::HttpMethod::GET, "/after",
			[&nextHandled](crazy::HttpRequest&, crazy::HttpResponse& response) {
				response.setBody("after");
				nextHandled.store(true);
			}));

		std::thread server([listener, router, &firstRead, &secondRead]() {
			auto client = listener->accept();
			if (!client) {
				return;
			}
			auto session = std::make_shared<crazy::HttpSession>(client, router);
			// 第 1 次读：数据就绪，读到 /open-stream 请求
			session->onReadEvent();
			firstRead.store(true);
			// 等客户端把第 2 个请求真正发到同一连接上
			for (int i = 0; i < 2000 && !secondRead.load(); ++i) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			// 第 2 次读：数据同样已就绪（第 2 个请求已到达）
			session->onReadEvent();
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		});

		crazy::Socket client;
		EVI_CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, 2000);
		const std::string firstRequest =
			"GET /open-stream HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n";
		EVI_CHECK(client.send(firstRequest.data(), firstRequest.size()) ==
			static_cast<int32_t>(firstRequest.size()));
		const std::string received = readUntil(client, "7\r\npartial\r\n", kBudgetMs);
		// 服务端线程此刻可能尚未返回 onReadEvent()，有界等待其置位
		for (int i = 0; i < 2000 && !firstRead.load(); ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		EVI_CHECK(firstRead.load());
		EVI_CHECK(streamHandled.load());
		EVI_CHECK(received.find("7\r\npartial\r\n") != std::string::npos);
		// 分块响应尚未结束：终止块不得出现
		EVI_CHECK(received.find("0\r\n\r\n") == std::string::npos);

		const std::string secondRequest =
			"GET /after HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
		EVI_CHECK(client.send(secondRequest.data(), secondRequest.size()) ==
			static_cast<int32_t>(secondRequest.size()));
		secondRead.store(true);
		server.join();

		EVI_CHECK(!nextHandled.load());
		std::cout << "[Q3] streaming not finished -> second request NOT dispatched (nextHandled="
			<< nextHandled.load() << ")" << std::endl;
		client.close();
	}

	// ---------------------------------------------------------------
	// [8] AC-8：入站 chunked 请求体去分块（独立复跑，不复用 keisum 断言）
	// ---------------------------------------------------------------
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> observed{ false };
		std::string observedBody;
		EVI_CHECK(router->add(crazy::HttpMethod::POST, "/evi-upload",
			[&observed, &observedBody](crazy::HttpRequest& request, crazy::HttpResponse& response) {
				observedBody = request.body();
				response.setBody("ok");
				observed.store(true);
			}));
		std::thread server(runFrameworkServerBounded, listener, router, std::cref(observed), 5000);

		crazy::Socket client;
		EVI_CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, 2000);
		const std::string request =
			"POST /evi-upload HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"4\r\nWiki\r\n"
			"5\r\npedia\r\n"
			"0\r\n\r\n";
		EVI_CHECK(client.send(request.data(), request.size()) == static_cast<int32_t>(request.size()));
		const std::string received = readUntil(client, "\r\n\r\nok", kBudgetMs);
		for (int i = 0; i < 2000 && !observed.load(); ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		EVI_CHECK(observed.load());
		EVI_CHECK(observedBody == "Wikipedia");
		EVI_CHECK(received.find("Content-Length: 2") != std::string::npos);
		std::cout << "[AC-8] inbound chunked body de-chunked = '" << observedBody << "'" << std::endl;
		client.close();
		server.join();
	}

	// ---------------------------------------------------------------
	// [9] 既有失败审计：test_websocket_client_e2e 的根因独立复现
	//     (a) 线上握手请求里到底是大写还是小写头名？
	//     (b) 精确大小写查找（复刻该测试服务端）是否必然落空？
	//     (c) 改成大小写不敏感查找后，握手是否成功？
	// ---------------------------------------------------------------
	{
		crazy::Uri u("ws://127.0.0.1:19099/chat");
		auto req = crazy::HttpRequest::createWebSocketRequest(u, "dGhlIHNhbXBsZSBub25jZQ==");
		const std::string wire = req->toString();
		std::cout << "[WS] handshake request wire bytes:\n" << escapeBytes(wire) << std::endl;

		EVI_CHECK(wire.find("sec-websocket-key:") != std::string::npos);
		EVI_CHECK(wire.find("Sec-WebSocket-Key:") == std::string::npos);
		EVI_CHECK(wire.find("upgrade: websocket") != std::string::npos);
		// 复刻 test_websocket_client_e2e.cc:93-98 的精确大小写查找
		EVI_CHECK(wire.find("Sec-WebSocket-Key:") == std::string::npos);

		// 端到端：一个做精确大小写查找的裸服务端（= 该测试的服务端）→ connect 失败
		uint16_t portA = 0;
		auto listenerA = makeListener(portA);
		std::thread serverA([listenerA]() {
			auto client = listenerA->accept();
			if (!client) {
				return;
			}
			setRecvTimeout(*client, 2000);
			const std::string request = readRequestHeader(client);
			if (request.find("Sec-WebSocket-Key:") == std::string::npos) {
				client->close();  // 与 test_websocket_client_e2e.cc:95-98 完全相同
				return;
			}
			client->close();
		});
		crazy::WebSocketClient wsA;
		const bool connectA = wsA.connect("ws://127.0.0.1:" + std::to_string(portA) + "/chat");
		serverA.join();
		EVI_CHECK(connectA == false);
		std::cout << "[WS] exact-case lookup server -> WebSocketClient::connect() == false "
			<< "(reproduces tests/test_websocket_client_e2e.cc:180 failure)" << std::endl;

		// 端到端：把查找改成大小写不敏感（其他完全不变）→ connect 成功
		uint16_t portB = 0;
		auto listenerB = makeListener(portB);
		std::thread serverB([listenerB]() {
			auto client = listenerB->accept();
			if (!client) {
				return;
			}
			setRecvTimeout(*client, 2000);
			const std::string request = readRequestHeader(client);
			const std::string lowered = toLower(request);
			const std::string keyHeader = "sec-websocket-key:";
			const size_t keyPos = lowered.find(keyHeader);
			if (keyPos == std::string::npos) {
				client->close();
				return;
			}
			size_t valueStart = keyPos + keyHeader.size();
			while (valueStart < request.size() &&
				(request[valueStart] == ' ' || request[valueStart] == '\t')) {
				++valueStart;
			}
			const size_t valueEnd = request.find("\r\n", valueStart);
			const std::string key = request.substr(valueStart, valueEnd - valueStart);
			const std::string accept = crazy::HttpResponse::computeWebSocketAccept(key);
			const std::string response =
				"HTTP/1.1 101 Switching Protocols\r\n"
				"Upgrade: websocket\r\n"
				"Connection: Upgrade\r\n"
				"Sec-WebSocket-Accept: " + accept + "\r\n"
				"\r\n";
			client->send(response.data(), response.size());
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			client->close();
		});
		crazy::WebSocketClient wsB;
		const bool connectB = wsB.connect("ws://127.0.0.1:" + std::to_string(portB) + "/chat");
		serverB.join();
		EVI_CHECK(connectB == true);
		EVI_CHECK(wsB.isConnected());
		std::cout << "[WS] case-insensitive lookup server -> WebSocketClient::connect() == true "
			<< "(root cause confirmed: only the header-name case lookup differs)" << std::endl;
	}

	// ---------------------------------------------------------------
	// [10] Q2：分块模式下显式 Content-Length 是"报错"而非"静默移除"
	// ---------------------------------------------------------------
	{
		crazy::HttpResponse response;
		response.headers().insert("Content-Length", "5");
		bool threw = false;
		std::string what;
		try {
			response.beginChunked();
		}
		catch (const std::logic_error& e) {
			threw = true;
			what = e.what();
		}
		EVI_CHECK(threw);
		// 不是静默移除：头还在
		EVI_CHECK(response.headers().contains("Content-Length"));
		EVI_CHECK(response.headers().get("Content-Length").value_or("") == "5");
		// 抛错后未进入分块模式
		EVI_CHECK(!response.isChunked());
		EVI_CHECK(response.toString().find("Transfer-Encoding") == std::string::npos);
		std::cout << "[Q2] explicit Content-Length in chunked mode -> std::logic_error: '"
			<< what << "', header NOT silently removed" << std::endl;

		// 守卫：未 beginChunked 就 writeChunk/endChunked 必须报错
		crazy::HttpResponse plain;
		bool writeThrew = false;
		bool endThrew = false;
		try { plain.writeChunk("x"); } catch (const std::logic_error&) { writeThrew = true; }
		try { plain.endChunked(); } catch (const std::logic_error&) { endThrew = true; }
		EVI_CHECK(writeThrew);
		EVI_CHECK(endThrew);
		std::cout << "[Q2] guards: writeChunk/endChunked without beginChunked throw = "
			<< writeThrew << "/" << endThrew << std::endl;
	}

	// ---------------------------------------------------------------
	// [11] Q3 正向：写出终止块后，同一连接上的后续请求必须被正常处理
	//      （evi 自写的服务端驱动，不复用 keisum 的用例）
	// ---------------------------------------------------------------
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> streamDone{ false };
		std::atomic<bool> nextHandled{ false };
		EVI_CHECK(router->add(crazy::HttpMethod::GET, "/s",
			[&streamDone](crazy::HttpRequest&, crazy::HttpResponse& response) {
				auto deferred = response.defer();
				deferred->beginChunked();
				deferred->writeChunk("AAA");
				deferred->endChunked();
				streamDone.store(true);
			}));
		EVI_CHECK(router->add(crazy::HttpMethod::GET, "/n",
			[&nextHandled](crazy::HttpRequest&, crazy::HttpResponse& response) {
				response.setBody("next");
				nextHandled.store(true);
			}));

		std::thread server([listener, router, &nextHandled]() {
			auto client = listener->accept();
			if (!client) {
				return;
			}
			auto session = std::make_shared<crazy::HttpSession>(client, router);
			for (int i = 0; i < 64 && !nextHandled.load(); ++i) {
				session->onReadEvent();
			}
		});

		crazy::Socket client;
		EVI_CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, 2000);
		const std::string firstRequest =
			"GET /s HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n";
		EVI_CHECK(client.send(firstRequest.data(), firstRequest.size()) ==
			static_cast<int32_t>(firstRequest.size()));
		const std::string firstResponse = readUntil(client, "0\r\n\r\n", kBudgetMs);
		for (int i = 0; i < 2000 && !streamDone.load(); ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		EVI_CHECK(streamDone.load());
		EVI_CHECK(firstResponse.find("HTTP/1.1 200 OK\r\n") == 0);
		EVI_CHECK(firstResponse.find("3\r\nAAA\r\n0\r\n\r\n") != std::string::npos);
		EVI_CHECK(!containsIgnoreCase(firstResponse, "Content-Length"));

		const std::string secondRequest =
			"GET /n HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
		EVI_CHECK(client.send(secondRequest.data(), secondRequest.size()) ==
			static_cast<int32_t>(secondRequest.size()));
		const std::string secondResponse = readUntil(client, "\r\n\r\nnext", kBudgetMs);
		server.join();
		EVI_CHECK(nextHandled.load());
		EVI_CHECK(secondResponse.find("HTTP/1.1 200 OK\r\n") == 0);
		EVI_CHECK(secondResponse.find("\r\n\r\nnext") != std::string::npos);
		std::cout << "[Q3] after terminating chunk -> connection reused, second response = "
			<< escapeBytes(secondResponse) << std::endl;
		client.close();
	}

	// ---------------------------------------------------------------
	// [12] 残留风险量化（非 AC，仅为报告提供真实数据）：
	//      错误路径的"有界返回"依赖对端关闭连接，框架自身不设读超时。
	// ---------------------------------------------------------------
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		std::thread server([listener]() {
			auto client = listener->accept();
			if (!client) {
				return;
			}
			setRecvTimeout(*client, 2000);
			readRequestHeader(client);
			const std::string partial =
				"HTTP/1.1 200 OK\r\n"
				"Transfer-Encoding: chunked\r\n"
				"\r\n"
				"5\r\nHel";
			client->send(partial.data(), partial.size());
			// 对端保持连接不关闭：超过 AC-5 的 5 秒预算
			std::this_thread::sleep_for(std::chrono::milliseconds(6500));
			client->close();
		});

		crazy::HttpClient client;
		const auto begin = std::chrono::steady_clock::now();
		auto response = client.request(crazy::HttpMethod::GET, uri(port, "/evi-stall"), "", {});
		const int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - begin).count();
		server.join();
		EVI_CHECK(response == nullptr);
		std::cout << "[OBS-1] peer kept connection open without terminator -> request() returned "
			<< "nullptr only after peer close, elapsedMs=" << elapsed
			<< " (>5000 => no client-side read timeout; bounded return relies on peer close)"
			<< std::endl;
	}

	std::cout << "evi independent chunked verification passed" << std::endl;
#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}

#undef EVI_CHECK
