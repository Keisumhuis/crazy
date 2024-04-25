#include "crazy.h"

class Controller {
public:
	static void Add(crazy::http::Request::Ptr req, crazy::http::Response::Ptr res) {
		res->SetBody("hello world");
		res->SetStatus(crazy::HttpStatus::OK);
	}
};

int main () {
	crazy::IPv4Address::Ptr addr1(new crazy::IPv4Address("0.0.0.0", 8878));
	crazy::http::HttpServer::Ptr httpServer = std::make_shared<crazy::http::HttpServer>();
	httpServer->Bind(addr1);

	httpServer->GetServletDispatch()->AddServlet("/Add", std::bind(&Controller::Add, std::placeholders::_1, std::placeholders::_2));
	
	httpServer->Start();

	while (1) {sleep(1);}
}