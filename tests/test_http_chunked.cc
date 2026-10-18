/**
 * @file test_http_chunked.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 分块传输（chunked）端到端测试.
 * @version 0.1
 * @date 2026-10-09
 */

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include "crazy/net/http/http_client.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_router.h"
#include "crazy/net/http/http_session.h"
#include "crazy/net/socket.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif

namespace {
	//! 所有等待的上限（毫秒），测试内禁止无限阻塞
	const int kWaitMs = 5000;

	// 带超时的可读等待：返回 true 表示可读（或连接已断开）
	bool waitReadable(socket_t fd, int milliseconds) {
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(fd, &readSet);
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = milliseconds * 1000;
#ifdef _WIN32
		return select(0, &readSet, nullptr, nullptr, &timeout) > 0;
#else
		return select(static_cast<int>(fd) + 1, &readSet, nullptr, nullptr, &timeout) > 0;
#endif
	}

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
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

	// 监听 127.0.0.1 的随机端口（不使用固定端口）
	crazy::Socket::ptr makeListener(uint16_t& port) {
		auto listener = std::make_shared<crazy::Socket>();
		if (!listener->listen(0, "127.0.0.1")) {
			std::cerr << "listen 127.0.0.1 failed" << std::endl;
			std::exit(1);
		}
		port = parsePort(listener->localAddress());
		if (port == 0) {
			std::cerr << "resolve local port failed" << std::endl;
			std::exit(1);
		}
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

	// 读取到请求头结束（裸服务端使用，带接收超时，不会无限阻塞）
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

	std::string toLower(const std::string& input) {
		std::string result = input;
		for (char& c : result) {
			if (c >= 'A' && c <= 'Z') {
				c = static_cast<char>(c - 'A' + 'a');
			}
		}
		return result;
	}

	// 头字段名大小写不敏感
	bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
		return toLower(haystack).find(toLower(needle)) != std::string::npos;
	}

	int hexDigit(char c) {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}

	// 按 RFC 7230 解码 chunked body，返回是否以合法的终止块结束
	bool decodeChunked(const std::string& body, std::string& decoded) {
		size_t offset = 0;
		while (offset < body.size()) {
			const size_t crlf = body.find("\r\n", offset);
			if (crlf == std::string::npos) {
				return false;
			}
			const std::string sizeText = body.substr(offset, crlf - offset);
			if (sizeText.empty()) {
				return false;
			}
			size_t size = 0;
			for (const char c : sizeText) {
				const int digit = hexDigit(c);
				if (digit < 0) {
					return false;
				}
				size = size * 16 + static_cast<size_t>(digit);
			}
			offset = crlf + 2;
			if (size == 0) {
				// 终止块：0\r\n\r\n
				return body.compare(offset, 2, "\r\n") == 0;
			}
			if (offset + size + 2 > body.size()) {
				return false;
			}
			if (body.compare(offset + size, 2, "\r\n") != 0) {
				return false;
			}
			decoded.append(body, offset, size);
			offset += size + 2;
		}
		return false;
	}

	// 读取直到出现指定标记（有界等待）
	std::string readUntil(crazy::Socket& socket, const std::string& marker) {
		std::string received;
		char buffer[4096];
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kWaitMs);
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

	// 发送必定失败的 socket：用于验证异常中断时强制关闭连接
	class FailingSendSocket final : public crazy::Socket {
	public:
		int32_t send(const void*, size_t, int32_t) override {
			errno = ECONNRESET;
			return -1;
		}
	};

	// 暴露受保护入口的会话，便于在无真实事件循环时驱动
	class TestHttpSession final : public crazy::HttpSession {
	public:
		using crazy::HttpSession::HttpSession;
		using crazy::HttpSession::handleRequest;
	};

	// 一次性裸 socket 服务端：accept 一次，读到请求头后发送指定字节并关闭连接
	struct RawCaseResult {
		crazy::HttpResponse::ptr response;
		bool callbackCalled = false;
	};

	RawCaseResult requestRawServer(const std::string& responseBytes, const std::string& path) {
		uint16_t port = 0;
		auto listener = makeListener(port);
		std::thread server([listener, responseBytes]() {
			auto client = listener->accept();
			if (!client) {
				return;
			}
			setRecvTimeout(*client, kWaitMs);
			readRequestHeader(client);
			client->send(responseBytes.data(), responseBytes.size());
			client->close();
		});

		RawCaseResult result;
		crazy::HttpClient client;
		result.response = client.request(crazy::HttpMethod::GET, uri(port, path), "",
			[&result](crazy::HttpResponse::ptr) { result.callbackCalled = true; });
		server.join();
		return result;
	}

	// 收到响应体数据段时交付；记录交付次数与拼接结果，并唤醒服务端
	struct DeliveryProbe {
		std::mutex mutex;
		std::condition_variable cv;
		int deliveries = 0;
		std::string data;
		bool serverSignaled = false;
	};

	void runStagedChunkedServer(const crazy::Socket::ptr& listener, DeliveryProbe& probe) {
		auto client = listener->accept();
		if (!client) {
			return;
		}
		setRecvTimeout(*client, kWaitMs);
		readRequestHeader(client);

		const std::string head =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Connection: close\r\n"
			"\r\n";
		client->send(head.data(), head.size());
		const std::string firstChunk = "b\r\nfirst-chunk\r\n";
		client->send(firstChunk.data(), firstChunk.size());

		// 阻塞等待“客户端已交付第 1 段”，上限 5 秒
		{
			std::unique_lock<std::mutex> lock(probe.mutex);
			probe.serverSignaled = probe.cv.wait_for(lock,
				std::chrono::milliseconds(kWaitMs),
				[&probe]() { return probe.deliveries >= 1; });
		}

		const std::string rest = "c\r\nsecond-chunk\r\n0\r\n\r\n";
		client->send(rest.data(), rest.size());
		client->close();
	}

	// 用框架 HttpSession 处理一次连接；handled 由业务 Handler 置位
	void runFrameworkServer(const crazy::Socket::ptr& listener, const crazy::HttpRouter::ptr& router,
		const std::atomic<bool>& handled) {
		auto client = listener->accept();
		if (!client) {
			return;
		}
		auto session = std::make_shared<crazy::HttpSession>(client, router);
		for (int i = 0; i < 64 && !handled.load(); ++i) {
			session->onReadEvent();
		}
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

	// AC-1 服务端分块输出的线上字节
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> chunkedHandled{ false };
		CHECK(router->add(crazy::HttpMethod::GET, "/stream",
			[&chunkedHandled](crazy::HttpRequest&, crazy::HttpResponse& response) {
				auto deferred = response.defer();
				deferred->beginChunked();
				deferred->writeChunk("Hello");
				deferred->writeChunk(" ");
				deferred->writeChunk("World");
				deferred->endChunked();
				chunkedHandled.store(true);
			}));

		std::thread server(runFrameworkServer, listener, router, std::cref(chunkedHandled));

		crazy::Socket client;
		CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, kWaitMs);
		const std::string request =
			"GET /stream HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Connection: keep-alive\r\n"
			"\r\n";
		CHECK(client.send(request.data(), request.size()) == static_cast<int32_t>(request.size()));

		const std::string received = readUntil(client, "0\r\n\r\n");

		CHECK(received.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
		const size_t headerEnd = received.find("\r\n\r\n");
		CHECK(headerEnd != std::string::npos);
		const std::string head = received.substr(0, headerEnd + 4);
		const std::string body = received.substr(headerEnd + 4);
		CHECK(head.find("Transfer-Encoding: chunked\r\n") != std::string::npos);
		CHECK(!containsIgnoreCase(head, "Content-Length"));

		std::string decoded;
		CHECK(decodeChunked(body, decoded));
		CHECK(decoded == "Hello World");
		CHECK(body.find("5\r\nHello\r\n") == 0);

		client.close();
		server.join();
	}

	// AC-2 客户端解析合法 chunked 响应得到完整 body
	{
		const std::string raw =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Connection: close\r\n"
			"\r\n"
			"5\r\nHello\r\n"
			"1\r\n \r\n"
			"5\r\nWorld\r\n"
			"0\r\n\r\n";
		const RawCaseResult result = requestRawServer(raw, "/raw");
		CHECK(result.response != nullptr);
		CHECK(result.callbackCalled);
		CHECK(result.response->status() == crazy::HttpStatus::OK);
		CHECK(result.response->body() == "Hello World");
		CHECK(result.response->shouldKeepAlive() == false);

		crazy::HttpResponseParser parser;
		parser.execute(raw);
		CHECK(!parser.hasError());
		CHECK(parser.isFinished());
		CHECK(parser.getResponse()->body() == "Hello World");
	}

	// AC-3 逐块交付：服务端发第 1 段后等待客户端交付信号，收到后才发第 2 段
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		DeliveryProbe probe;
		std::thread server(runStagedChunkedServer, listener, std::ref(probe));

		crazy::HttpClient client;
		auto response = client.request(crazy::HttpMethod::GET, uri(port, "/stream"), "", {},
			[&probe](const std::string& data) {
				std::lock_guard<std::mutex> lock(probe.mutex);
				probe.deliveries += 1;
				probe.data += data;
				probe.cv.notify_all();
			});
		server.join();

		CHECK(probe.serverSignaled);
		CHECK(probe.deliveries >= 2);
		CHECK(probe.data == "first-chunksecond-chunk");
		CHECK(response != nullptr);
		CHECK(response->body() == "first-chunksecond-chunk");
	}

	// AC-4 非法 chunk 长度
	{
		const std::string raw =
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"Z\r\nillegal\r\n"
			"0\r\n\r\n";

		crazy::HttpResponseParser parser;
		parser.execute(raw);
		CHECK(parser.hasError());
		CHECK(!parser.isFinished());

		const RawCaseResult result = requestRawServer(raw, "/illegal");
		CHECK(result.response == nullptr);
		CHECK(!result.callbackCalled);
	}

	// AC-5 缺少终止块 / 中途断开
	{
		const std::string noTerminator =
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"5\r\nHello\r\n";
		const RawCaseResult first = requestRawServer(noTerminator, "/no-terminator");
		CHECK(first.response == nullptr);
		CHECK(!first.callbackCalled);

		const std::string midChunk =
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"5\r\nHel";
		const RawCaseResult second = requestRawServer(midChunk, "/mid-chunk");
		CHECK(second.response == nullptr);
		CHECK(!second.callbackCalled);
	}

	// AC-6 同时出现 Transfer-Encoding: chunked 与 Content-Length 时按解析错误处理
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
		CHECK(parser.hasError());
		CHECK(!parser.isFinished());

		const RawCaseResult result = requestRawServer(raw, "/conflict");
		CHECK(result.response == nullptr);
		CHECK(!result.callbackCalled);
	}

	// Q2 分块模式下显式设置 Content-Length 时框架直接报错（不静默删头）
	{
		crazy::HttpResponse response;
		response.headers().insert("Content-Length", "5");
		bool threw = false;
		try {
			response.beginChunked();
		}
		catch (const std::logic_error&) {
			threw = true;
		}
		CHECK(threw);
		CHECK(!response.isChunked());
	}
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> handled{ false };
		CHECK(router->add(crazy::HttpMethod::GET, "/bad-length",
			[&handled](crazy::HttpRequest&, crazy::HttpResponse& response) {
				response.headers().insert("Content-Length", "5");
				response.beginChunked();
				response.writeChunk("hello");
				handled.store(true);
			}));
		std::thread server(runFrameworkServer, listener, router, std::cref(handled));

		crazy::Socket client;
		CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, kWaitMs);
		const std::string request =
			"GET /bad-length HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Connection: close\r\n"
			"\r\n";
		CHECK(client.send(request.data(), request.size()) == static_cast<int32_t>(request.size()));

		const std::string received = readUntil(client, "Internal Server Error");
		CHECK(received.rfind("HTTP/1.1 500", 0) == 0);
		CHECK(!containsIgnoreCase(received, "Transfer-Encoding"));
		CHECK(containsIgnoreCase(received, "Content-Length"));
		CHECK(received.find("Internal Server Error") != std::string::npos);

		client.close();
		server.join();
	}

	// Q3 收到终止块后连接可复用：同一连接上的后续普通请求正常完成
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> secondHandled{ false };
		CHECK(router->add(crazy::HttpMethod::GET, "/stream",
			[](crazy::HttpRequest&, crazy::HttpResponse& response) {
				auto deferred = response.defer();
				deferred->beginChunked();
				deferred->writeChunk("Hello");
				deferred->endChunked();
			}));
		CHECK(router->add(crazy::HttpMethod::GET, "/next",
			[&secondHandled](crazy::HttpRequest&, crazy::HttpResponse& response) {
				response.setBody("next");
				secondHandled.store(true);
			}));
		std::thread server(runFrameworkServer, listener, router, std::cref(secondHandled));

		crazy::Socket client;
		CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, kWaitMs);
		const std::string firstRequest =
			"GET /stream HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Connection: keep-alive\r\n"
			"\r\n";
		CHECK(client.send(firstRequest.data(), firstRequest.size()) ==
			static_cast<int32_t>(firstRequest.size()));
		const std::string firstResponse = readUntil(client, "0\r\n\r\n");
		CHECK(firstResponse.find("Transfer-Encoding: chunked\r\n") != std::string::npos);
		CHECK(!containsIgnoreCase(firstResponse, "Content-Length"));

		// 连接未关闭，第二个请求仍能在同一连接上得到完整响应
		const std::string secondRequest =
			"GET /next HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Connection: close\r\n"
			"\r\n";
		CHECK(client.send(secondRequest.data(), secondRequest.size()) ==
			static_cast<int32_t>(secondRequest.size()));
		const std::string secondResponse = readUntil(client, "\r\n\r\nnext");
		CHECK(secondResponse.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
		CHECK(secondResponse.find("\r\n\r\nnext") != std::string::npos);
		CHECK(secondHandled.load());

		client.close();
		server.join();
	}

	// Q3 分块写出过程中发送失败（异常中断）必须强制关闭连接
	{
		auto router = std::make_shared<crazy::HttpRouter>();
		CHECK(router->add(crazy::HttpMethod::GET, "/stream",
			[](crazy::HttpRequest&, crazy::HttpResponse& response) {
				auto deferred = response.defer();
				deferred->beginChunked();
				deferred->writeChunk("Hello");
				deferred->endChunked();
			}));
		auto socket = std::make_shared<FailingSendSocket>();
		auto session = std::make_shared<TestHttpSession>(socket, router);
		auto request = std::make_shared<crazy::HttpRequest>("/stream");
		request->setMethod(crazy::HttpMethod::GET);
		session->handleRequest(request);
		CHECK(!socket->active());
	}

	// AC-8 入站 chunked 请求体在 Handler 中是去分块后的完整 payload
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> observed{ false };
		std::string observedBody;
		CHECK(router->add(crazy::HttpMethod::POST, "/upload",
			[&observed, &observedBody](crazy::HttpRequest& request, crazy::HttpResponse& response) {
				observedBody = request.body();
				response.setBody("ok");
				observed.store(true);
			}));
		std::thread server(runFrameworkServer, listener, router, std::cref(observed));

		crazy::Socket client;
		CHECK(client.connect("127.0.0.1", port));
		const std::string request =
			"POST /upload HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"4\r\nWiki\r\n"
			"5\r\npedia\r\n"
			"0\r\n\r\n";
		CHECK(client.send(request.data(), request.size()) == static_cast<int32_t>(request.size()));

		const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kWaitMs);
		while (!observed.load() && std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		CHECK(observed.load());
		CHECK(observedBody == "Wikipedia");

		client.close();
		server.join();
	}

	// AC-9 HTTP/1.0 客户端：分块编码无法表达，降级为「关闭连接界定响应体」
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> handled{ false };
		CHECK(router->add(crazy::HttpMethod::GET, "/stream",
			[&handled](crazy::HttpRequest&, crazy::HttpResponse& response) {
				auto deferred = response.defer();
				deferred->beginChunked();
				deferred->writeChunk("Hello");
				deferred->writeChunk(" ");
				deferred->writeChunk("World");
				deferred->endChunked();
				handled.store(true);
			}));
		std::thread server(runFrameworkServer, listener, router, std::cref(handled));

		crazy::Socket client;
		CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, kWaitMs);
		const std::string request =
			"GET /stream HTTP/1.0\r\n"
			"Connection: keep-alive\r\n"
			"\r\n";
		CHECK(client.send(request.data(), request.size()) == static_cast<int32_t>(request.size()));

		const std::string received = readUntil(client, "Hello World");
		server.join();

		CHECK(received.rfind("HTTP/1.0 200 OK\r\n", 0) == 0);
		CHECK(!containsIgnoreCase(received, "Transfer-Encoding"));
		CHECK(!containsIgnoreCase(received, "Content-Length"));
		CHECK(containsIgnoreCase(received, "Connection: close"));
		const size_t headerEnd = received.find("\r\n\r\n");
		CHECK(headerEnd != std::string::npos);
		// 降级后响应体就是原始数据，没有分块帧
		CHECK(received.substr(headerEnd + 4) == "Hello World");
		// 响应体由关闭连接界定：服务端写完后必须断开
		char probe = 0;
		CHECK(client.recv(&probe, 1) == 0);
		client.close();
	}

	// AC-10 Trailer：头段声明 Trailer，终止块后带 trailer 字段；客户端解析进 trailers()
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> handled{ false };
		CHECK(router->add(crazy::HttpMethod::GET, "/trailer",
			[&handled](crazy::HttpRequest&, crazy::HttpResponse& response) {
				auto deferred = response.defer();
				deferred->addTrailer("X-Checksum", "abc123");
				deferred->beginChunked();
				deferred->writeChunk("data");
				deferred->endChunked();
				handled.store(true);
			}));
		std::thread server(runFrameworkServer, listener, router, std::cref(handled));

		crazy::Socket client;
		CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, kWaitMs);
		const std::string request =
			"GET /trailer HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Connection: close\r\n"
			"\r\n";
		CHECK(client.send(request.data(), request.size()) == static_cast<int32_t>(request.size()));

		const std::string received = readUntil(client, "abc123");
		server.join();
		client.close();

		CHECK(containsIgnoreCase(received, "Trailer: x-checksum"));
		CHECK(received.find("4\r\ndata\r\n0\r\nx-checksum: abc123\r\n\r\n") != std::string::npos);

		crazy::HttpResponseParser parser;
		parser.execute(received);
		CHECK(!parser.hasError());
		CHECK(parser.isFinished());
		CHECK(parser.getResponse()->body() == "data");
		CHECK(parser.getResponse()->trailers().get("X-Checksum").value_or("") == "abc123");
		CHECK(!parser.getResponse()->headers().contains("X-Checksum"));
	}

	// AC-11 Expect: 100-continue：服务端在请求体到达前先回临时响应，客户端再发请求体
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> handled{ false };
		std::string observedBody;
		CHECK(router->add(crazy::HttpMethod::POST, "/continue",
			[&handled, &observedBody](crazy::HttpRequest& request, crazy::HttpResponse& response) {
				observedBody = request.body();
				response.setBody("done");
				handled.store(true);
			}));
		std::atomic<bool> stop{ false };
		std::thread server([listener, router, &stop]() {
			auto client = listener->accept();
			if (!client) {
				return;
			}
			auto session = std::make_shared<crazy::HttpSession>(client, router);
			const auto deadline = std::chrono::steady_clock::now()
				+ std::chrono::milliseconds(kWaitMs);
			while (!stop.load() && std::chrono::steady_clock::now() < deadline) {
				const socket_t fd = session->socket();
				if (fd != INVALID_SOCKET && waitReadable(fd, 2)) {
					session->onReadEvent();
				}
				session->onWriteEvent();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		});

		crazy::Socket client;
		CHECK(client.connect("127.0.0.1", port));
		setRecvTimeout(client, kWaitMs);
		const std::string head =
			"POST /continue HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Content-Length: 5\r\n"
			"Expect: 100-continue\r\n"
			"Connection: close\r\n"
			"\r\n";
		CHECK(client.send(head.data(), head.size()) == static_cast<int32_t>(head.size()));

		// 此刻请求体还没发：服务端必须先给 100 Continue，否则客户端与服务器互相等待
		const std::string interim = readUntil(client, "\r\n\r\n");
		CHECK(interim.rfind("HTTP/1.1 100 Continue", 0) == 0);

		const std::string body = "hello";
		CHECK(client.send(body.data(), body.size()) == static_cast<int32_t>(body.size()));
		const std::string finalResponse = readUntil(client, "done");
		CHECK(finalResponse.rfind("HTTP/1.1 200", 0) == 0);
		CHECK(finalResponse.find("done") != std::string::npos);

		client.close();
		stop.store(true);
		server.join();
		CHECK(handled.load());
		CHECK(observedBody == "hello");
	}

	// AC-12 客户端忽略临时 1xx 响应：100 Continue 之后的最终响应才是结果
	{
		const std::string raw =
			"HTTP/1.1 100 Continue\r\n"
			"\r\n"
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 5\r\n"
			"Connection: close\r\n"
			"\r\n"
			"hello";

		crazy::HttpResponseParser parser;
		parser.execute(raw);
		CHECK(!parser.hasError());
		CHECK(parser.isFinished());
		CHECK(parser.interimResponseCount() == 1);
		CHECK(parser.getResponse()->status() == crazy::HttpStatus::OK);
		CHECK(parser.getResponse()->body() == "hello");

		const RawCaseResult result = requestRawServer(raw, "/interim");
		CHECK(result.response != nullptr);
		CHECK(result.callbackCalled);
		CHECK(result.response->status() == crazy::HttpStatus::OK);
		CHECK(result.response->body() == "hello");
	}

	std::cout << "http chunked tests passed" << std::endl;

#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}

#undef CHECK
