/**
 * @file test_http_range.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief 静态文件分片传输端到端测试：默认分块发送、Range/206/416、If-Range 与 HTTP/1.0 降级.
 * @version 0.1
 * @date 2026-10-13
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "crazy/net/http/http_client.h"
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

	// 临时目录：析构时递归删除
	class TempDirectory {
	public:
		TempDirectory() {
			const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
			path_ = std::filesystem::temp_directory_path()
				/ ("crazy_range_" + std::to_string(stamp));
			std::filesystem::create_directories(path_);
		}
		~TempDirectory() {
			std::error_code error;
			std::filesystem::remove_all(path_, error);
		}
		TempDirectory(const TempDirectory&) = delete;
		TempDirectory& operator=(const TempDirectory&) = delete;
		const std::filesystem::path& path() const {
			return path_;
		}
		std::string write(const std::string& name, const std::string& content) const {
			const auto file = path_ / name;
			std::ofstream stream(file, std::ios::binary | std::ios::trunc);
			stream.write(content.data(), static_cast<std::streamsize>(content.size()));
			stream.close();
			return file.string();
		}

	private:
		std::filesystem::path path_;
	};

	// 分块报文解析结果
	struct ChunkedBody {
		bool terminated = false;
		size_t chunks = 0;
		std::string data;
	};

	ChunkedBody parseChunked(const std::string& body) {
		ChunkedBody result;
		size_t offset = 0;
		while (offset < body.size()) {
			const size_t crlf = body.find("\r\n", offset);
			if (crlf == std::string::npos) {
				return result;
			}
			const std::string sizeText = body.substr(offset, crlf - offset);
			if (sizeText.empty()) {
				return result;
			}
			size_t size = 0;
			for (const char c : sizeText) {
				int digit = -1;
				if (c >= '0' && c <= '9') digit = c - '0';
				else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
				else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
				if (digit < 0) {
					return result;
				}
				size = size * 16 + static_cast<size_t>(digit);
			}
			offset = crlf + 2;
			if (size == 0) {
				result.terminated = body.compare(offset, 2, "\r\n") == 0;
				return result;
			}
			if (offset + size + 2 > body.size()) {
				return result;
			}
			if (body.compare(offset + size, 2, "\r\n") != 0) {
				return result;
			}
			result.data.append(body, offset, size);
			result.chunks += 1;
			offset += size + 2;
		}
		return result;
	}

	// 抓到的完整响应
	struct ParsedResponse {
		std::string head;
		std::string body;
		std::string rawBody;
		bool chunked = false;
		bool chunkedValid = false;
		size_t chunks = 0;
	};

	ParsedResponse splitResponse(const std::string& raw) {
		ParsedResponse result;
		const size_t headerEnd = raw.find("\r\n\r\n");
		if (headerEnd == std::string::npos) {
			result.head = raw;
			return result;
		}
		result.head = raw.substr(0, headerEnd + 4);
		result.rawBody = raw.substr(headerEnd + 4);
		result.chunked = containsIgnoreCase(result.head, "Transfer-Encoding: chunked");
		if (result.chunked) {
			const ChunkedBody chunked = parseChunked(result.rawBody);
			result.chunkedValid = chunked.terminated;
			result.body = chunked.data;
			result.chunks = chunked.chunks;
		}
		else {
			result.body = result.rawBody;
		}
		return result;
	}

	// 读不到数据或对端关闭前持续读取；带接收超时，不会无限阻塞
	std::string readToEnd(crazy::Socket& socket) {
		std::string received;
		char buffer[8192];
		for (;;) {
			const int32_t length = socket.recv(buffer, sizeof(buffer));
			if (length <= 0) {
				break;
			}
			received.append(buffer, static_cast<size_t>(length));
		}
		return received;
	}

	ParsedResponse requestRaw(uint16_t port, const std::string& request) {
		crazy::Socket client;
		if (!client.connect("127.0.0.1", port)) {
			std::cerr << "connect failed" << std::endl;
			std::exit(1);
		}
		setRecvTimeout(client, kWaitMs);
		CHECK(client.send(request.data(), request.size()) == static_cast<int32_t>(request.size()));
		const std::string raw = readToEnd(client);
		client.close();
		return splitResponse(raw);
	}

	// 逐次驱动一次会话：只在 socket 可读时解析请求，写事件把流式分块推出去
	void runServer(const crazy::Socket::ptr& listener, const crazy::HttpRouter::ptr& router,
		const std::atomic<bool>& done) {
		auto client = listener->accept();
		if (!client) {
			return;
		}
		auto session = std::make_shared<crazy::HttpSession>(client, router);
		const auto deadline = std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(kWaitMs * 2);
		while (!done.load() && std::chrono::steady_clock::now() < deadline) {
			const socket_t fd = session->socket();
			if (fd != INVALID_SOCKET && waitReadable(fd, 2)) {
				session->onReadEvent();
			}
			session->onWriteEvent();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	// 一次静态文件请求：起服务端、发原始请求、读到连接关闭
	ParsedResponse fetch(const std::filesystem::path& directory, const std::string& requestLine,
		const std::string& path, const std::string& extraHeaders = std::string()) {
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		if (!router->addStaticDirectory("/static", directory.string())) {
			std::cerr << "addStaticDirectory failed" << std::endl;
			std::exit(1);
		}
		std::atomic<bool> done{ false };
		std::thread server(runServer, listener, router, std::cref(done));
		const std::string request = requestLine + " /static/" + path + " HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Connection: close\r\n"
			+ extraHeaders + "\r\n";
		auto response = requestRaw(port, request);
		done.store(true);
		server.join();
		return response;
	}

	// 用 HTTP/1.0 请求路径（不带 Host，带 keep-alive）取静态文件
	ParsedResponse fetchHttp10(const std::filesystem::path& directory, const std::string& path) {
		uint16_t port = 0;
		auto listener = makeListener(port);
		auto router = std::make_shared<crazy::HttpRouter>();
		if (!router->addStaticDirectory("/static", directory.string())) {
			std::cerr << "addStaticDirectory failed" << std::endl;
			std::exit(1);
		}
		std::atomic<bool> done{ false };
		std::thread server(runServer, listener, router, std::cref(done));
		const std::string request = "GET /static/" + path + " HTTP/1.0\r\n"
			"Connection: keep-alive\r\n"
			"\r\n";
		auto response = requestRaw(port, request);
		done.store(true);
		server.join();
		return response;
	}
}  // namespace

int main() {
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

	TempDirectory directory;
	const std::string hello = "hello range";
	directory.write("hello.txt", hello);

	// AC-1 静态资源默认按分块方式发送：无 Content-Length，带 Accept-Ranges / ETag
	{
		const ParsedResponse response = fetch(directory.path(), "GET", "hello.txt");
		CHECK(response.head.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
		CHECK(response.chunked);
		CHECK(response.chunkedValid);
		CHECK(!containsIgnoreCase(response.head, "Content-Length"));
		CHECK(containsIgnoreCase(response.head, "Accept-Ranges: bytes"));
		CHECK(containsIgnoreCase(response.head, "ETag: \""));
		CHECK(containsIgnoreCase(response.head, "Content-Type: text/plain"));
		CHECK(response.body == hello);
	}

	// AC-2 单区间：206 + Content-Range，响应体是该区间的分块数据
	{
		const ParsedResponse response = fetch(directory.path(), "GET", "hello.txt",
			"Range: bytes=6-10\r\n");
		CHECK(response.head.rfind("HTTP/1.1 206 ", 0) == 0);
		CHECK(containsIgnoreCase(response.head, "Content-Range: bytes 6-10/11"));
		CHECK(response.chunked);
		CHECK(response.chunkedValid);
		CHECK(response.body == "range");
	}

	// AC-3 开放式区间 bytes=N-
	{
		const ParsedResponse response = fetch(directory.path(), "GET", "hello.txt",
			"Range: bytes=6-\r\n");
		CHECK(response.head.rfind("HTTP/1.1 206 ", 0) == 0);
		CHECK(containsIgnoreCase(response.head, "Content-Range: bytes 6-10/11"));
		CHECK(response.body == "range");
	}

	// AC-4 后缀区间 bytes=-N（断点续传常用）
	{
		const ParsedResponse response = fetch(directory.path(), "GET", "hello.txt",
			"Range: bytes=-5\r\n");
		CHECK(response.head.rfind("HTTP/1.1 206 ", 0) == 0);
		CHECK(containsIgnoreCase(response.head, "Content-Range: bytes 6-10/11"));
		CHECK(response.body == "range");
	}

	// AC-5 越界区间：416 + Content-Range: bytes */size
	{
		const ParsedResponse response = fetch(directory.path(), "GET", "hello.txt",
			"Range: bytes=20-30\r\n");
		CHECK(response.head.rfind("HTTP/1.1 416 ", 0) == 0);
		CHECK(containsIgnoreCase(response.head, "Content-Range: bytes */11"));
		CHECK(!response.chunked);
		CHECK(response.body == "Range Not Satisfiable");
	}

	// AC-6 多区间与非法语法按「忽略 Range」处理：200 + 完整内容
	{
		const ParsedResponse multiple = fetch(directory.path(), "GET", "hello.txt",
			"Range: bytes=0-1,4-5\r\n");
		CHECK(multiple.head.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
		CHECK(!containsIgnoreCase(multiple.head, "Content-Range"));
		CHECK(multiple.body == hello);

		const ParsedResponse illegal = fetch(directory.path(), "GET", "hello.txt",
			"Range: bytes=abc\r\n");
		CHECK(illegal.head.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
		CHECK(!containsIgnoreCase(illegal.head, "Content-Range"));
		CHECK(illegal.body == hello);
	}

	// AC-7 If-Range：匹配 ETag 才走 206，否则退回完整 200
	{
		const ParsedResponse probe = fetch(directory.path(), "GET", "hello.txt");
		CHECK(containsIgnoreCase(probe.head, "ETag: \""));
		const size_t etagStart = toLower(probe.head).find("etag: \"");
		CHECK(etagStart != std::string::npos);
		const size_t valueStart = etagStart + 6;
		const size_t valueEnd = probe.head.find("\r\n", valueStart);
		CHECK(valueEnd != std::string::npos);
		const std::string etag = probe.head.substr(valueStart, valueEnd - valueStart);
		CHECK(!etag.empty());

		const ParsedResponse matched = fetch(directory.path(), "GET", "hello.txt",
			"Range: bytes=0-4\r\nIf-Range: " + etag + "\r\n");
		CHECK(matched.head.rfind("HTTP/1.1 206 ", 0) == 0);
		CHECK(matched.body == "hello");

		const ParsedResponse stale = fetch(directory.path(), "GET", "hello.txt",
			"Range: bytes=0-4\r\nIf-Range: \"stale-etag\"\r\n");
		CHECK(stale.head.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
		CHECK(!containsIgnoreCase(stale.head, "Content-Range"));
		CHECK(stale.body == hello);
	}

	// AC-8 大文件按块流式发送：分多块、内容完整，且不整体进内存
	{
		std::string payload;
		payload.reserve(300 * 1024);
		for (size_t i = 0; i < 300 * 1024; ++i) {
			payload += static_cast<char>(static_cast<unsigned char>(i % 251));
		}
		directory.write("large.bin", payload);

		const ParsedResponse response = fetch(directory.path(), "GET", "large.bin");
		CHECK(response.head.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
		CHECK(response.chunked);
		CHECK(response.chunkedValid);
		CHECK(response.body == payload);
		CHECK(response.chunks > 1);
		CHECK(response.chunks == (payload.size() + crazy::HttpRouter::kStaticStreamChunkSize - 1)
			/ crazy::HttpRouter::kStaticStreamChunkSize);

		const ParsedResponse partial = fetch(directory.path(), "GET", "large.bin",
			"Range: bytes=1000-200000\r\n");
		CHECK(partial.head.rfind("HTTP/1.1 206 ", 0) == 0);
		CHECK(containsIgnoreCase(partial.head, "Content-Range: bytes 1000-200000/307200"));
		CHECK(partial.chunked);
		CHECK(partial.chunkedValid);
		CHECK(partial.body == payload.substr(1000, 200000 - 1000 + 1));
	}

	// AC-9 HTTP/1.0 客户端：分块无法表达，降级为「关闭连接界定响应体」
	{
		const ParsedResponse response = fetchHttp10(directory.path(), "hello.txt");
		CHECK(response.head.rfind("HTTP/1.0 200 OK\r\n", 0) == 0);
		CHECK(!containsIgnoreCase(response.head, "Transfer-Encoding"));
		CHECK(!containsIgnoreCase(response.head, "Content-Length"));
		CHECK(containsIgnoreCase(response.head, "Connection: close"));
		// 非分块：body 就是原始字节，读到连接关闭即完整
		CHECK(response.body == hello);
	}

	// AC-10 空文件：200 + Content-Length: 0（没有数据可流式发送）
	{
		directory.write("empty.txt", "");
		const ParsedResponse response = fetch(directory.path(), "GET", "empty.txt");
		CHECK(response.head.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
		CHECK(!response.chunked);
		CHECK(containsIgnoreCase(response.head, "Content-Length: 0"));
		CHECK(response.body.empty());
	}

	std::cout << "http range tests passed" << std::endl;

#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}

#undef CHECK
