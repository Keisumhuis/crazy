/**
 * @file test_http_client_e2e.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 客户端端到端测试（真实网络收发）.
 * @version 0.1
 * @date 2026-09-06
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "crazy/net/http/http_client.h"
#include "crazy/net/socket.h"

namespace {
	int failures = 0;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	// 读取一个完整 HTTP 请求（头 + 按 Content-Length 读取 body）
	std::string readRequest(crazy::Socket::ptr client) {
		std::string request;
		char buf[8192];
		// 读取请求头，直到 \r\n\r\n
		while (request.find("\r\n\r\n") == std::string::npos) {
			const int n = client->recv(buf, sizeof(buf));
			if (n <= 0) {
				return request;
			}
			request.append(buf, n);
		}

		// 提取 Content-Length
		size_t contentLength = 0;
		const std::string clHeader = "Content-Length:";
		const size_t clPos = request.find(clHeader);
		if (clPos != std::string::npos) {
			size_t valueStart = clPos + clHeader.size();
			while (valueStart < request.size() &&
				(request[valueStart] == ' ' || request[valueStart] == '\t')) {
				++valueStart;
			}
			const size_t valueEnd = request.find("\r\n", valueStart);
			contentLength = static_cast<size_t>(
				std::stoul(request.substr(valueStart, valueEnd - valueStart)));
		}

		// 已读 body 长度，继续读取直到完整
		const size_t headerEnd = request.find("\r\n\r\n") + 4;
		size_t bodyLen = request.size() - headerEnd;
		while (bodyLen < contentLength) {
			const int n = client->recv(buf, sizeof(buf));
			if (n <= 0) {
				break;
			}
			request.append(buf, n);
			bodyLen += static_cast<size_t>(n);
		}
		return request;
	}

	// 最小 HTTP 服务端：accept 3 次，每次回显整个请求
	void runHttpServer(uint16_t port, std::atomic<bool>& ready) {
		crazy::Socket server;
		if (!server.listen(port, "127.0.0.1")) {
			ready.store(true);
			return;
		}
		ready.store(true);

		for (int i = 0; i < 3; ++i) {
			auto client = server.accept();
			if (!client) {
				return;
			}
			const std::string request = readRequest(client);
			const std::string response =
				"HTTP/1.1 200 OK\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: " + std::to_string(request.size()) + "\r\n"
				"Connection: close\r\n"
				"\r\n" + request;
			client->send(response.data(), response.size());
			client->close();
		}
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
#ifdef _WIN32
	// Windows 下 socket 使用前必须先初始化 Winsock 库
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed" << std::endl;
		return 1;
	}
#endif
	const uint16_t port = 19090;
	std::atomic<bool> ready{ false };

	std::thread serverThread(runHttpServer, port, std::ref(ready));
	while (!ready.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	crazy::HttpClient client;

	// 1. GET 请求
	{
		crazy::HttpResponse::ptr response;
		client.get("http://127.0.0.1:" + std::to_string(port) + "/hello",
			[&](crazy::HttpResponse::ptr resp) { response = resp; });
		CHECK(response != nullptr);
		CHECK(response->status() == crazy::HttpStatus::OK);
		CHECK(response->body().find("GET /hello") != std::string::npos);
	}

	// 2. POST 请求（带 body）
	{
		crazy::HttpResponse::ptr response;
		client.post("http://127.0.0.1:" + std::to_string(port) + "/echo", "hello body",
			[&](crazy::HttpResponse::ptr resp) { response = resp; });
		CHECK(response != nullptr);
		CHECK(response->body().find("POST /echo") != std::string::npos);
		CHECK(response->body().find("hello body") != std::string::npos);
	}

	// 3. form-data 上传
	{
		std::vector<crazy::FormDataField> fields;
		fields.push_back({ "name", "alice" });
		std::vector<crazy::FormDataFile> files;
		crazy::FormDataFile file;
		file.name = "file";
		file.filename = "a.txt";
		file.contentType = "text/plain";
		file.content = "file-content";
		files.push_back(file);

		crazy::HttpResponse::ptr response;
		client.postForm("http://127.0.0.1:" + std::to_string(port) + "/upload",
			fields, files,
			[&](crazy::HttpResponse::ptr resp) { response = resp; });
		CHECK(response != nullptr);
		CHECK(response->body().find("POST /upload") != std::string::npos);
		CHECK(response->body().find("multipart/form-data") != std::string::npos);
		CHECK(response->body().find("name=\"file\"") != std::string::npos);
		CHECK(response->body().find("file-content") != std::string::npos);
	}

	serverThread.join();

	if (failures == 0) {
		std::cout << "http client e2e tests passed" << std::endl;
	}
#ifdef _WIN32
	WSACleanup();
#endif
	return failures == 0 ? 0 : 1;
}

#undef CHECK
