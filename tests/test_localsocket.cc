#include "crazy.h"

int32_t main(int32_t argc, char** argv) {
	crazy::Application app(argc, argv);

	if (argc >= 2) {
		crazy::LocalSocket socket;
		socket.connect("test_localsocket.cmd");

		socket.send("123123123", strlen("123123123"));
		char buffer[10240] = {};
		auto len = socket.recv(buffer, 10240);
		CRAZY_ROOT_DEBUG() << std::string(buffer, len);

	} 
	else {
		crazy::LocalSocket socket;
		socket.listen("test_localsocket.cmd");

		for (uint32_t i = 0; i < 5; ++i) {
			auto client = socket.accept();

			char buffer[10240] = {};
			auto len = client->recv(buffer, 10240);
			CRAZY_ROOT_DEBUG() << std::string(buffer, len);
			client->send(buffer, len);
		}
	}
	app.exec();
}
