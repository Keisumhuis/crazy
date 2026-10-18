/**
 * @file test_http_client_async.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 客户端事件驱动用法测试：requestAsync + Selector 驱动 onReadable/onWritable，
 *        并验证 keep-alive 连接在两次异步请求之间被复用。
 * @version 0.1
 * @date 2026-10-12
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

#include "crazy/net/http/http_client.h"
#include "crazy/net/selector.h"
#include "crazy/net/socket.h"

namespace {
	int failures = 0;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			++failures;
		}
	}

	// 最小 HTTP/1.1 服务端：同一条连接上按 keep-alive 服务 2 个请求，每个都回 "hello"
	void runHttpServer(uint16_t port, std::atomic<bool>& ready, std::atomic<int>& accepts, std::atomic<int>& served) {
		crazy::Socket server;
		if (!server.listen(port, "127.0.0.1")) {
			ready.store(true);
			return;
		}
		ready.store(true);

		auto client = server.accept();
		if (!client) {
			return;
		}
		accepts.fetch_add(1);

		std::string pending;
		char buf[4096];
		while (served.load() < 2) {
			while (pending.find("\r\n\r\n") == std::string::npos) {
				const int n = client->recv(buf, sizeof(buf));
				if (n <= 0) {
					client->close();
					return;
				}
				pending.append(buf, n);
			}
			pending.erase(0, pending.find("\r\n\r\n") + 4);
			const std::string body = "hello";
			const std::string response =
				"HTTP/1.1 200 OK\r\n"
				"Content-Length: " + std::to_string(body.size()) + "\r\n"
				"Connection: keep-alive\r\n"
				"\r\n" + body;
			client->send(response.data(), response.size());
			served.fetch_add(1);
		}
		client->close();
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
	const uint16_t port = 19182;
	std::atomic<bool> ready{ false };
	std::atomic<int> accepts{ 0 };
	std::atomic<int> served{ 0 };
	std::thread serverThread(runHttpServer, port, std::ref(ready), std::ref(accepts), std::ref(served));
	while (!ready.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	crazy::Selector selector;
	crazy::HttpClient client;
	std::atomic<int> responses{ 0 };
	std::atomic<int> statusCode{ 0 };
	std::string body;
	bool timedOut = false;
	bool done = false;

	client.registerSelectorCallbacks(
		[&selector](int32_t fd, crazy::SelectorEventType type, std::function<void()> callback) {
			selector.registerEvent(fd, type, std::move(callback));
		},
		[&selector](int32_t fd, crazy::SelectorEventType type) {
			selector.unregisterEvent(fd, type);
		});
	selector.registerTimer("watchdog", 5000, [&] {
		timedOut = true;
		done = true;
		selector.wakeup();
	});

	const std::string base = "http://127.0.0.1:" + std::to_string(port);
	CHECK(client.requestAsync(crazy::HttpMethod::GET, base + "/one", "", [&](crazy::HttpResponse::ptr response) {
		if (response) {
			statusCode.store(static_cast<int32_t>(response->status()));
			body = response->body();
		}
		responses.fetch_add(1);
		if (responses.load() >= 2) {
			done = true;
			selector.wakeup();
		}
	}));
	CHECK(client.isBusy());

	while (!done) {
		selector.select();
		if (responses.load() == 1 && !client.isBusy() && !timedOut) {
			// 第一个响应完成后，用同一个客户端在同一连接上再发一次
			CHECK(client.requestAsync(crazy::HttpMethod::GET, base + "/two", "", [&](crazy::HttpResponse::ptr response) {
				if (response) {
					statusCode.store(static_cast<int32_t>(response->status()));
					body = response->body();
				}
				responses.fetch_add(1);
				done = true;
				selector.wakeup();
			}));
		}
	}

	CHECK(!timedOut);
	CHECK(responses.load() == 2);
	CHECK(statusCode.load() == 200);
	CHECK(body == "hello");
	// keep-alive：两次请求只应产生一次 accept
	CHECK(accepts.load() == 1);
	CHECK(served.load() == 2);
	CHECK(!client.hasPendingWrite());

	client.detachSelector();
	selector.unregisterTimer("watchdog");
	serverThread.join();

	if (failures == 0) {
		std::cout << "http client async tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}

#undef CHECK
