#include "crazy.h"

class Controller {
public:
	static void Add(crazy::http::Request::Ptr req, crazy::http::Response::Ptr res) {
		res->SetBody("hello world");
		res->SetStatus(crazy::HttpStatus::OK);
	}
};

int main () {
	try {
		crazy::IPv4Address::Ptr addr1(new crazy::IPv4Address("0.0.0.0", 8877));
		crazy::IPv4Address::Ptr addr2(new crazy::IPv4Address("0.0.0.0", 8878));
		crazy::http::HttpServer::Ptr httpServer = std::make_shared<crazy::http::HttpServer>();
		std::vector<crazy::Address::Ptr> fails;
		httpServer->Bind(std::vector<crazy::Address::Ptr>{addr1, addr2}, fails);
		// httpServer->LoadCertificates("/root/work_space/crazy/src/crazy/server.crt", "/root/work_space/crazy/src/crazy/server.key");

		httpServer->GetServletDispatch()->AddServlet("/Add", std::bind(&Controller::Add, std::placeholders::_1, std::placeholders::_2));
	
		httpServer->Start();

	}
	catch(...) {
		CRAZY_ASSERT(false);
	}
	
	while (1) {sleep(1);}
}