/**
 * @file test_http_keep_alive_e2e.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP/1.1 keep-alive 压力测试.
 * @version 0.1
 * @date 2026-09-13
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "crazy/net/http/http_client.h"
#include "crazy/net/http/http_router.h"
#include "crazy/net/http/http_session.h"
#include "crazy/net/socket.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace {
	const int kRequestCount = 10000;

	struct ServerState {
		std::atomic<bool> ready{ false };
		std::atomic<int> accepted{ 0 };
		std::atomic<int> requests{ 0 };
		std::atomic<int> failures{ 0 };
		uint16_t port = 0;
	};

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": "
				<< expression << std::endl;
			std::exit(1);
		}
	}

	void runHttpServer(ServerState& state) {
		crazy::Socket listener;
		if (!listener.listen(0, "127.0.0.1")) {
			state.failures.fetch_add(1);
			state.ready.store(true);
			return;
		}
		const std::string address = listener.localAddress();
		const size_t portSeparator = address.rfind(':');
		if (portSeparator == std::string::npos) {
			state.failures.fetch_add(1);
			state.ready.store(true);
			return;
		}
		state.port = static_cast<uint16_t>(
			std::stoul(address.substr(portSeparator + 1)));
		state.ready.store(true);

		auto client = listener.accept();
		if (!client) {
			state.failures.fetch_add(1);
			return;
		}
		state.accepted.fetch_add(1);

		auto router = std::make_shared<crazy::HttpRouter>();
		router->add(crazy::HttpMethod::GET, "/ping",
			[](crazy::HttpRequest&, crazy::HttpResponse& response) {
				response.setBody("pong");
			});
		auto session = std::make_shared<crazy::HttpSession>(client, router);
		for (int i = 0; i < kRequestCount; ++i) {
			session->onReadEvent();
			state.requests.fetch_add(1);
		}
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

	ServerState state;
	std::thread serverThread(runHttpServer, std::ref(state));
	while (!state.ready.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	CHECK(state.failures.load() == 0);
	CHECK(state.port != 0);

	const auto start = std::chrono::steady_clock::now();
	crazy::HttpClient client;
	for (int i = 0; i < kRequestCount; ++i) {
		auto response = client.request(
			crazy::HttpMethod::GET,
			"http://127.0.0.1:" + std::to_string(state.port) + "/ping",
			"",
			{});
		CHECK(response != nullptr);
		CHECK(response->status() == crazy::HttpStatus::OK);
		CHECK(response->body() == "pong");
	}
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - start);

	serverThread.join();
	CHECK(state.failures.load() == 0);
	CHECK(state.accepted.load() == 1);
	CHECK(state.requests.load() == kRequestCount);

	const double seconds = elapsed.count() / 1000.0;
	const double requestsPerSecond =
		seconds > 0.0 ? kRequestCount / seconds : 0.0;
	std::cout << "http keep-alive pressure test passed: requests="
		<< kRequestCount
		<< ", connections=" << state.accepted.load()
		<< ", elapsed_ms=" << elapsed.count()
		<< ", requests_per_second=" << requestsPerSecond
		<< std::endl;

#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}

#undef CHECK
