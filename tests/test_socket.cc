#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "crazy/net/socket.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace {
	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}
}

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

	crazy::Socket listener;
	CHECK(listener.listen(0, "127.0.0.1"));

	const auto listenerAddress = listener.localAddress();
	const auto portStart = listenerAddress.rfind(':');
	CHECK(portStart != std::string::npos);
	const auto port = static_cast<uint16_t>(std::stoul(listenerAddress.substr(portStart + 1)));

	std::thread client([port]() {
		crazy::Socket socket;
		CHECK(socket.connect("127.0.0.1", port));
	});

	auto accepted = listener.accept();
	CHECK(accepted != nullptr);
	client.join();
	accepted->close();

	std::cout << "socket tests passed" << std::endl;
#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}

#undef CHECK
