#include "crazy.h"
#include "crazy/socket.h"
#include "crazy/tcp_server.h"
#include <memory>

class EchoTcpServer :public crazy::TcpServer {
public:
	using Ptr = std::shared_ptr<EchoTcpServer>;
	void HandleClient(crazy::Socket::Ptr sock) override {
		while (true) {
			std::string buffer;
			buffer.resize(10240);
			auto rt = sock->Recv(buffer.data(), 10240);
			if (rt <= 0) {
				CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "close socket = " << sock->GetRemoteAddress()->ToString(); 
				break;
			}
			CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "recv from " << sock->GetRemoteAddress()->ToString() 
				<< " recv len = " << rt;
			sock->Send(buffer.data(), rt);
		}
	}
};

int main() {
	crazy::IPv4Address::Ptr addr1(new crazy::IPv4Address("0.0.0.0", 8877));
	crazy::IPv4Address::Ptr addr2(new crazy::IPv4Address("0.0.0.0", 8878));
	EchoTcpServer::Ptr tcpserver = std::make_shared<EchoTcpServer>();
	std::vector<crazy::Address::Ptr> fails;
	tcpserver->Bind(std::vector<crazy::Address::Ptr>{addr1, addr2}, fails);
	tcpserver->Start();

	while (1) {sleep(1);}
}
