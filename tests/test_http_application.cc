/**
 * @file test_http_application.cc
 * @author kesium (keisumhuis@gmail.com)
 * @brief HTTP Application 综合示例.
 * @version 0.1
 * @date 2026-09-06
 */

#include <chrono>
#include <thread>

#include "crazy/http_application.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_session.h"
#include "crazy/utils.h"

namespace test {
	/**
	 * @brief 全局函数路由.
	 */
	void globalHello(crazy::HttpRequest&, crazy::HttpResponse& response) {
		response.setBody("global hello");
	}

	/**
	 * @brief HTTP 控制器示例.
	 */
	class UserController {
	public:
		/**
		 * @brief 处理 GET 请求.
		 */
		void index(crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("member index");
		}

		/**
		 * @brief 处理带请求体的 POST 请求.
		 */
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
	app.registerHttpHandler<crazy::GET, crazy::POST>("/user", &test::UserController::index, &controller);
	app.registerHttpHandler<crazy::POST>("/submit", &test::UserController::submit, &controller);

	app.registerHttpHandler<crazy::GET>("/defer", [&app](crazy::HttpRequest&, crazy::HttpResponse& response) {
		auto deferredResponse = response.defer();
		app.enqueueRunnable([deferredResponse]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			deferredResponse->setBody("deferred response");
			deferredResponse->send();
			});
		});

	const std::string staticDirectory = crazy::PathUtil::JoinPath(crazy::PathUtil::GetCurrentWorkingDirectory(), "http_static");
	if (!app.registerStaticDirectory("/static", staticDirectory, true)) {
		CRAZY_SYSTEM_ERROR() << "register static directory failed: " << staticDirectory;
	}
	if (!app.registerStaticDirectory("/files", staticDirectory)) {
		CRAZY_SYSTEM_ERROR() << "register file directory failed: " << staticDirectory;
	}

	app.registerWebSocketConnectCallback([](crazy::HttpSession::ptr session) {
		CRAZY_SYSTEM_INFO() << "WebSocket connected, socket = " << session->socket();
		session->sendText("welcome");
		});
	app.registerWebSocketMessageCallback([](crazy::HttpSession::ptr session, const std::string& data,
		crazy::HttpSession::WebSocketOpCode opcode) {
			if (opcode == crazy::HttpSession::WebSocketOpCode::text) {
				if (data == "close") {
					session->sendClose();
				}
				else {
					session->sendText(data);
				}
			}
			else if (opcode == crazy::HttpSession::WebSocketOpCode::binary) {
				session->sendBinary(data);
			}
		});
	app.registerWebSocketCloseCallback([]() {
		CRAZY_SYSTEM_INFO() << "WebSocket closed";
		});

	app.exec();
	return 0;
}
