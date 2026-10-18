#include <cstdlib>
#include <iostream>
#include <string>

#include "crazy/net/acceptor.h"
#include "crazy/net/session.h"
#include "crazy/net/socket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/select.h>
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

namespace {
	//! 等待可读事件；所有等待均有界（默认 5000ms），不使用无限阻塞
	bool waitReadable(crazy::Socket& socket, int32_t timeoutMS = 5000) {
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(socket.socket(), &readSet);
		timeval timeout;
		timeout.tv_sec = timeoutMS / 1000;
		timeout.tv_usec = (timeoutMS % 1000) * 1000;
		return ::select(static_cast<int>(socket.socket() + 1), &readSet, nullptr, nullptr, &timeout) > 0;
	}

	//! 从 tcp://host:port 或 tcp://[ipv6]:port 取端口，无法解析返回 0
	uint16_t extractPort(const std::string& address) {
		const auto pos = address.rfind(':');
		if (pos == std::string::npos) {
			return 0;
		}
		try {
			return static_cast<uint16_t>(std::stoul(address.substr(pos + 1)));
		}
		catch (...) {
			return 0;
		}
	}

	//! 有界 accept
	crazy::Socket::ptr acceptWithTimeout(crazy::Socket& listener) {
		if (!waitReadable(listener)) {
			return nullptr;
		}
		return listener.accept();
	}

	//! 有界收发：客户端发送 payload，服务端必须读回完全相同的内容
	bool exchange(crazy::Socket& client, crazy::Socket& accepted, const std::string& payload) {
		if (client.send(payload.data(), payload.size()) != static_cast<int32_t>(payload.size())) {
			return false;
		}
		if (!waitReadable(accepted)) {
			return false;
		}
		char buffer[128] = {};
		const int32_t received = accepted.recv(buffer, sizeof(buffer));
		if (received != static_cast<int32_t>(payload.size())) {
			return false;
		}
		return std::string(buffer, static_cast<size_t>(received)) == payload;
	}
}

int main() {
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

	// AC-2：默认行为不变 —— 默认/IPv4 地址仍按 IPv4 处理，且没有隐式双栈
	{
		crazy::Socket listener;
		CHECK(listener.listen(0, "0.0.0.0"));
		const uint16_t port = extractPort(listener.localAddress());
		CHECK(port > 0);
		CHECK(listener.localAddress() == "tcp://0.0.0.0:" + std::to_string(port));

		crazy::Socket client;
		CHECK(client.connect("127.0.0.1", port));
		auto accepted = acceptWithTimeout(listener);
		CHECK(accepted != nullptr);
		CHECK(accepted->remoteAddress() == "tcp://127.0.0.1:" + std::to_string(extractPort(client.localAddress())));
		CHECK(exchange(client, *accepted, "ipv4-default"));

		// IPv4 监听者不接受 IPv6 连接
		crazy::Socket ipv6Client;
		CHECK(!ipv6Client.connect("::1", port));

		// G1 决策：非字面量主机名继续只产出 IPv4 候选，connect("localhost") 的族不得静默变化。
		// 若主机名解析变成 IPv6 优先，这里会试图连 ::1:<port> 而被 IPv4-only 监听者拒绝。
		crazy::Socket hostnameClient;
		CHECK(hostnameClient.connect("localhost", port));
		auto hostnameAccepted = acceptWithTimeout(listener);
		CHECK(hostnameAccepted != nullptr);
		CHECK(hostnameAccepted->remoteAddress().rfind("tcp://127.0.0.1:", 0) == 0);
		CHECK(exchange(hostnameClient, *hostnameAccepted, "ipv4-hostname"));
	}

	// AC-2（续）：显式 IPv4 字面量仍为 AF_INET，地址格式逐字节不变
	{
		crazy::Socket listener;
		CHECK(listener.listen(0, "127.0.0.1"));
		const uint16_t port = extractPort(listener.localAddress());
		CHECK(port > 0);
		CHECK(listener.localAddress() == "tcp://127.0.0.1:" + std::to_string(port));

		crazy::Socket client;
		CHECK(client.connect("127.0.0.1", port));
		auto accepted = acceptWithTimeout(listener);
		CHECK(accepted != nullptr);
		CHECK(exchange(client, *accepted, "ipv4-literal"));

		crazy::Socket ipv6Client;
		CHECK(!ipv6Client.connect("::1", port));
	}

	// AC-3：IPv6 字面量 —— 仅 v6 可监听/可连接，地址为 tcp://[::1]:<n>
	{
		crazy::Socket listener;
		CHECK(listener.listen(0, "::1"));
		CHECK(listener.localAddress().rfind("tcp://[::1]:", 0) == 0);
		const uint16_t port = extractPort(listener.localAddress());
		CHECK(port > 0);

		crazy::Socket client;
		CHECK(client.connect("::1", port));
		CHECK(client.localAddress().rfind("tcp://[::1]:", 0) == 0);
		auto accepted = acceptWithTimeout(listener);
		CHECK(accepted != nullptr);
		CHECK(accepted->remoteAddress() == "tcp://[::1]:" + std::to_string(extractPort(client.localAddress())));
		CHECK(exchange(client, *accepted, "ipv6-loopback"));

		// 未隐式双栈：IPv4 客户端连不上
		crazy::Socket ipv4Client;
		CHECK(!ipv4Client.connect("127.0.0.1", port));
	}

	// AC-3（续）：通配 IPv6 地址与带方括号的 IPv6 字面量
	{
		crazy::Socket listener;
		CHECK(listener.listen(0, "::"));
		CHECK(listener.localAddress().rfind("tcp://[::]:", 0) == 0);
		CHECK(extractPort(listener.localAddress()) > 0);
	}
	{
		crazy::Socket listener;
		CHECK(listener.listen(0, "[::1]"));
		CHECK(listener.localAddress().rfind("tcp://[::1]:", 0) == 0);
		CHECK(extractPort(listener.localAddress()) > 0);
	}

	// AC-4 + AC-5：显式双栈 —— 同一端口同时连通 v4/v6，数据一致，v4 对端归一化
	{
		crazy::Socket listener;
		CHECK(listener.listen(0, "::", true));
		CHECK(listener.localAddress().rfind("tcp://[::]:", 0) == 0);
		const uint16_t port = extractPort(listener.localAddress());
		CHECK(port > 0);

		// IPv4 客户端经双栈监听连入
		crazy::Socket ipv4Client;
		CHECK(ipv4Client.connect("127.0.0.1", port));
		auto accepted4 = acceptWithTimeout(listener);
		CHECK(accepted4 != nullptr);
		CHECK(accepted4->remoteAddress() == "tcp://127.0.0.1:" + std::to_string(extractPort(ipv4Client.localAddress())));
		CHECK(accepted4->remoteAddress().find("::ffff:") == std::string::npos);
		CHECK(accepted4->localAddress().find("::ffff:") == std::string::npos);
		CHECK(exchange(ipv4Client, *accepted4, "dual-stack-v4"));

		// IPv6 客户端经同一端口连入
		crazy::Socket ipv6Client;
		CHECK(ipv6Client.connect("::1", port));
		auto accepted6 = acceptWithTimeout(listener);
		CHECK(accepted6 != nullptr);
		CHECK(accepted6->remoteAddress() == "tcp://[::1]:" + std::to_string(extractPort(ipv6Client.localAddress())));
		CHECK(accepted6->remoteAddress().find("::ffff:") == std::string::npos);
		CHECK(exchange(ipv6Client, *accepted6, "dual-stack-v6"));
	}

	// AC-6：Acceptor/Session 路径的地址回填非空且正确
	{
		crazy::Acceptor acceptor;
		CHECK(acceptor.listen(0, "::1"));
		const uint16_t port = extractPort(acceptor.localAddress());
		CHECK(port > 0);

		crazy::Socket client;
		CHECK(client.connect("::1", port));
		CHECK(waitReadable(acceptor));
		crazy::Session::ptr session = acceptor.accept();
		CHECK(session != nullptr);
		CHECK(!session->remoteAddress().empty());
		CHECK(session->remoteAddress() == "tcp://[::1]:" + std::to_string(extractPort(client.localAddress())));
	}
	{
		crazy::Acceptor acceptor;
		CHECK(acceptor.listen(0, "::", true));
		const uint16_t port = extractPort(acceptor.localAddress());
		CHECK(port > 0);

		crazy::Socket client;
		CHECK(client.connect("127.0.0.1", port));
		CHECK(waitReadable(acceptor));
		crazy::Session::ptr session = acceptor.accept();
		CHECK(session != nullptr);
		CHECK(!session->remoteAddress().empty());
		CHECK(session->remoteAddress() == "tcp://127.0.0.1:" + std::to_string(extractPort(client.localAddress())));
		CHECK(session->remoteAddress().find("::ffff:") == std::string::npos);
	}

	// AC-7：非法地址安全失败，且同一对象状态可恢复
	{
		crazy::Socket listener;
		CHECK(!listener.listen(0, "999.1.1.1"));
		CHECK(!listener.listen(0, "::1x"));
		CHECK(!listener.listen(0, "[::1"));
		CHECK(listener.listen(0, "127.0.0.1"));
		CHECK(extractPort(listener.localAddress()) > 0);
	}
	{
		crazy::Socket client;
		CHECK(!client.connect("999.1.1.1", 1));
	}

	std::cout << "socket ipv6 tests passed" << std::endl;

#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}

#undef CHECK
