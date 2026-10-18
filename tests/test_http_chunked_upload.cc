/**
 * @file test_http_chunked_upload.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief 客户端分块上传（Transfer-Encoding: chunked 请求体）测试：线路字节、端到端与错误路径.
 * @version 0.1
 * @date 2026-10-13
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "crazy/net/http/http_client.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_router.h"
#include "crazy/net/http/http_session.h"
#include "crazy/net/socket.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

namespace {
	//! 所有等待的上限（毫秒），测试内禁止无限阻塞
	const int kWaitMs = 5000;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

#define CHECK(expression) check((expression), #expression, __LINE__)

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

	// 读到分块请求的终止块为止（只处理本测试生成的合法帧）
	std::string readChunkedRequest(crazy::Socket& client) {
		std::string buffer;
		char chunk[4096];
		while (buffer.find("\r\n\r\n") == std::string::npos) {
			const int32_t length = client.recv(chunk, sizeof(chunk));
			if (length <= 0) {
				return buffer;
			}
			buffer.append(chunk, static_cast<size_t>(length));
		}
		size_t offset = buffer.find("\r\n\r\n") + 4;
		for (;;) {
			size_t lineEnd = buffer.find("\r\n", offset);
			while (lineEnd == std::string::npos) {
				const int32_t length = client.recv(chunk, sizeof(chunk));
				if (length <= 0) {
					return buffer;
				}
				buffer.append(chunk, static_cast<size_t>(length));
				lineEnd = buffer.find("\r\n", offset);
			}
			size_t size = 0;
			for (size_t i = offset; i < lineEnd; ++i) {
				const char c = buffer[i];
				int digit = -1;
				if (c >= '0' && c <= '9') digit = c - '0';
				else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
				else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
				if (digit < 0) {
					return buffer;
				}
				size = size * 16 + static_cast<size_t>(digit);
			}
			const size_t frameEnd = lineEnd + 2 + size + 2;
			while (buffer.size() < frameEnd) {
				const int32_t length = client.recv(chunk, sizeof(chunk));
				if (length <= 0) {
					return buffer;
				}
				buffer.append(chunk, static_cast<size_t>(length));
			}
			if (size == 0) {
				return buffer;
			}
			offset = frameEnd;
		}
	}

	// 裸服务端：读到终止块后回一个固定响应，并把原始请求字节交给调用方
	void runCaptureServer(const crazy::Socket::ptr& listener, std::string& captured) {
		auto client = listener->accept();
		if (!client) {
			return;
		}
		setRecvTimeout(*client, kWaitMs);
		captured = readChunkedRequest(*client);
		const std::string response =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 2\r\n"
			"Connection: close\r\n"
			"\r\n"
			"ok";
		client->send(response.data(), response.size());
		client->close();
	}

	// 框架服务端：驱动一次会话直到业务置位
	void runFrameworkServer(const crazy::Socket::ptr& listener, const crazy::HttpRouter::ptr& router,
		const std::atomic<bool>& handled) {
		auto client = listener->accept();
		if (!client) {
			return;
		}
		auto session = std::make_shared<crazy::HttpSession>(client, router);
		const auto deadline = std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(kWaitMs * 2);
		while (!handled.load() && std::chrono::steady_clock::now() < deadline) {
			session->onReadEvent();
			session->onWriteEvent();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}  // namespace

int main() {
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

	// AC-1 序列化：分块请求不含 Content-Length，帧格式符合 RFC 7230 4.1，并以终止块收尾
	{
		crazy::HttpRequest request("/upload");
		request.setMethod(crazy::HttpMethod::POST);
		request.setChunkedTransferEncoding(true);
		request.setChunkSize(4);
		request.setBody("abcdefghij");
		request.headers().insert("Host", "127.0.0.1");
		const std::string wire = request.toString();
		CHECK(!containsIgnoreCase(wire, "Content-Length"));
		CHECK(containsIgnoreCase(wire, "Transfer-Encoding: chunked"));
		CHECK(wire.find("\r\n\r\n4\r\nabcd\r\n4\r\nefgh\r\n2\r\nij\r\n0\r\n\r\n") != std::string::npos);
		CHECK(request.isChunkedTransferEncoding());
		CHECK(request.chunkSize() == 4);
	}

	// AC-2 空请求体：只有终止块
	{
		crazy::HttpRequest request("/upload");
		request.setMethod(crazy::HttpMethod::POST);
		request.setChunkedTransferEncoding(true);
		const std::string wire = request.toString();
		CHECK(containsIgnoreCase(wire, "Transfer-Encoding: chunked"));
		CHECK(wire.size() >= 5);
		CHECK(wire.compare(wire.size() - 5, 5, "0\r\n\r\n") == 0);
		CHECK(!containsIgnoreCase(wire, "Content-Length"));
	}

	// AC-3 分块模式下手写 Content-Length：序列化直接报错（不静默删头）
	{
		crazy::HttpRequest request("/upload");
		request.setChunkedTransferEncoding(true);
		request.headers().insert("Content-Length", "3");
		bool threw = false;
		try {
			request.toString();
		}
		catch (const std::logic_error&) {
			threw = true;
		}
		CHECK(threw);
		CHECK(request.headers().contains("Content-Length"));
	}

	// AC-3b 分块传输编码是 HTTP/1.1 的能力：HTTP/1.0 请求序列化直接报错
	{
		crazy::HttpRequest request("/upload");
		request.setMethod(crazy::HttpMethod::POST);
		request.setVersion(crazy::HttpVersions::HTTP_1_0);
		request.setChunkedTransferEncoding(true);
		request.setBody("abc");
		bool threw = false;
		try {
			request.toString();
		}
		catch (const std::logic_error&) {
			threw = true;
		}
		CHECK(threw);
	}

	// AC-4 线路字节：客户端按 setChunkedUpload 的块大小切分，服务端拿到合法分块请求
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		std::string captured;
		std::thread server(runCaptureServer, listener, std::ref(captured));

		crazy::HttpClient client;
		client.setChunkedUpload(true, 4);
		CHECK(client.isChunkedUpload());
		bool callbackCalled = false;
		auto response = client.request(crazy::HttpMethod::POST, uri(port, "/upload"), "0123456789",
			[&callbackCalled](crazy::HttpResponse::ptr) { callbackCalled = true; });
		server.join();

		CHECK(response != nullptr);
		CHECK(callbackCalled);
		CHECK(response->status() == crazy::HttpStatus::OK);
		CHECK(response->body() == "ok");
		CHECK(containsIgnoreCase(captured, "POST /upload HTTP/1.1"));
		CHECK(containsIgnoreCase(captured, "Transfer-Encoding: chunked"));
		CHECK(!containsIgnoreCase(captured, "Content-Length"));
		CHECK(captured.find("\r\n\r\n4\r\n0123\r\n4\r\n4567\r\n2\r\n89\r\n0\r\n\r\n") != std::string::npos);
	}

	// AC-5 端到端：框架服务端把分块请求体去分块后交给 Handler
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		std::atomic<bool> handled{ false };
		std::string observedBody;
		CHECK(router->add(crazy::HttpMethod::POST, "/upload",
			[&handled, &observedBody](crazy::HttpRequest& request, crazy::HttpResponse& response) {
				observedBody = request.body();
				response.setBody("stored");
				handled.store(true);
			}));
		std::thread server(runFrameworkServer, listener, router, std::cref(handled));

		std::string payload;
		for (size_t i = 0; i < 20000; ++i) {
			payload += static_cast<char>('a' + static_cast<int>(i % 26));
		}
		crazy::HttpClient client;
		client.setChunkedUpload(true, 1024);
		auto response = client.request(crazy::HttpMethod::POST, uri(port, "/upload"), payload, nullptr);
		server.join();

		CHECK(handled.load());
		CHECK(observedBody == payload);
		CHECK(response != nullptr);
		CHECK(response->status() == crazy::HttpStatus::OK);
		CHECK(response->body() == "stored");
	}

	// AC-6 分块上传与默认 Content-Length 冲突：不发送任何字节，直接失败
	{
		crazy::HttpClient client;
		client.setChunkedUpload(true);
		client.setHeader("Content-Length", "3");
		CHECK(client.request(crazy::HttpMethod::POST, "http://127.0.0.1:1/upload", "abc", nullptr) == nullptr);
		CHECK(!client.requestAsync(crazy::HttpMethod::POST, "http://127.0.0.1:1/upload", "abc", nullptr));
	}

	// AC-7 事件驱动路径：requestAsync 也使用分块上传
	{
		uint16_t port = 0;
		auto listener = makeListener(port);
		std::string captured;
		std::thread server(runCaptureServer, listener, std::ref(captured));

		crazy::HttpClient client;
		client.setChunkedUpload(true, 3);
		std::atomic<bool> done{ false };
		crazy::HttpResponse::ptr response;
		CHECK(client.requestAsync(crazy::HttpMethod::PUT, uri(port, "/upload"), "abcdef",
			[&done, &response](crazy::HttpResponse::ptr result) {
				response = result;
				done.store(true);
			}));
		const auto deadline = std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(kWaitMs);
		while (!done.load() && std::chrono::steady_clock::now() < deadline) {
			client.onWritable();
			client.onReadable();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		server.join();

		CHECK(done.load());
		CHECK(response != nullptr);
		CHECK(response->body() == "ok");
		CHECK(containsIgnoreCase(captured, "PUT /upload HTTP/1.1"));
		CHECK(containsIgnoreCase(captured, "Transfer-Encoding: chunked"));
		CHECK(!containsIgnoreCase(captured, "Content-Length"));
		CHECK(captured.find("\r\n\r\n3\r\nabc\r\n3\r\ndef\r\n0\r\n\r\n") != std::string::npos);
	}

	std::cout << "http chunked upload tests passed" << std::endl;

#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}

#undef CHECK
