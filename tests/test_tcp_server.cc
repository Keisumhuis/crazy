#include "crazy.h"

int main() {
	crazy::IPv4Address::Ptr addr1(new crazy::IPv4Address("0.0.0.0", 8877));
	crazy::IPv4Address::Ptr addr2(new crazy::IPv4Address("0.0.0.0", 8878));
	crazy::TcpServer::Ptr tcpserver = std::make_shared<crazy::TcpServer>();
	std::vector<crazy::Address::Ptr> fails;
	tcpserver->Bind(std::vector<crazy::Address::Ptr>{addr1, addr2}, fails);
	tcpserver->Start();

	while (1) {sleep(1);}
}
