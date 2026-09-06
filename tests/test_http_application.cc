/**
 * @file test_http_application.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP Application 线程池复用测试.
 * @version 0.1
 * @date 2026-09-06
 */

#include "crazy/http_application.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"

namespace test {
	void globalHello(crazy::HttpRequest&, crazy::HttpResponse& response) {
		response.setBody("global hello");
	}

	class UserController {
	public:
		void index(crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("member index");
		}

		void submit(crazy::HttpRequest& request, crazy::HttpResponse& response) {
			response.setBody("member submit, body=" + request.toString());
		}
	};
}  // namespace test

int main(int argc, char** argv) {
	test::UserController controller;
	crazy::HttpApplication app(argc, argv);

	app.listen(18080, "0.0.0.0");

	app.registerHttpHandler<crazy::GET>("/", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
		response.setBody("hello crazy");
	});
	app.registerHttpHandler<crazy::GET>("/global", test::globalHello);
	app.registerHttpHandler<crazy::GET, crazy::POST>(
		"/user", &test::UserController::index, &controller);
	app.registerHttpHandler<crazy::GET, crazy::POST>(
		"/submit", &test::UserController::submit, &controller);

	app.exec();
	return 0;
}
