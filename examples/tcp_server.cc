#include "crazy.h"

class EchoServer : public crazy::TcpServer {
public:
    using Ptr = std::shared_ptr<EchoServer>;
    void HandleClient(crazy::Socket::Ptr sock) {
        while (true) {
            std::string buffer;
			buffer.resize(10240);
			auto rt = sock->Recv(buffer.data(), 10240);
			if (rt == 0) {
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
	EchoServer::Ptr echo_server = std::make_shared<EchoServer>();
	std::vector<crazy::Address::Ptr> fails;
	echo_server->Bind(std::vector<crazy::Address::Ptr>{addr1, addr2}, fails);
	echo_server->Start();

	while (1) {sleep(1);}
}
