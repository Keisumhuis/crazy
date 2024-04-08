#include "../src/crazy.h"
#include "crazy/address.h"
#include "crazy/logger.h"
#include "crazy/socket.h"
#include <memory>

int main () {
	crazy::IPv4Address::Ptr addr(new crazy::IPv4Address("39.100.72.123", 80));
	auto socket = crazy::Socket::CreateTCPSocket();
	auto rt = socket->Connect(addr);
	if (rt) {
		const std::string data = R"(GET / HTTP/1.1
host:www.sylar.com

)";
		auto rtsize = socket->Send(data.c_str(), data.size());
		std::string recvbuf;
		recvbuf.resize(1024);
		rtsize = socket->Recv(recvbuf.data(), 1024);
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "recvbuf = " << recvbuf;
	}

	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "local addr  = " << socket->GetLocalAddress()->ToString();
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "remote addr  = " << socket->GetRemoteAddress()->ToString();

	crazy::IPv4Address::Ptr serveraddr(new crazy::IPv4Address("0.0.0.0", 8083));
	auto serversocket = crazy::Socket::CreateTCPSocket();
	auto ret = serversocket->Bind(serveraddr);
	serversocket->Listen();
	while (1) {
		auto client = serversocket->Accept();
		co [client]() {
			while (1) {
				std::string buffer;
				buffer.resize(1024);
				auto recv_len = client->Recv(buffer.data(), 1024);
				if (recv_len < 0) {
					break;
				}
				CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "recvbuf = " << buffer;
				client->Send(buffer.data(), buffer.size());
			}
		};
	}
}
