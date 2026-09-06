/**
 * @file test_websocket_server.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief WebSocket Echo 服务示例，展示 WebSocket 服务的完整写法.
 * @version 0.1
 * @date 2026-09-06
 */

#include "crazy/http_application.h"
#include "crazy/logger.h"
#include "crazy/net/http/http_session.h"

int main(int argc, char** argv) {
	crazy::HttpApplication app(argc, argv);

	// 监听 18081 端口
	app.listen(18081, "0.0.0.0");

	// 注册 WebSocket 连接建立回调：客户端连接上时触发
	app.registerWebSocketConnectCallback([](crazy::HttpSession::ptr session) {
		CRAZY_SYSTEM_INFO() << "WebSocket 客户端已连接，socket = " << session->socket();
		// 连接建立后主动发送一条欢迎消息
		session->sendText("welcome");
	});

	// 注册 WebSocket 消息回调：收到消息后原样回显
	app.registerWebSocketMessageCallback([](crazy::HttpSession::ptr session, const std::string& data, crazy::HttpSession::WebSocketOpCode opcode) {
		switch (opcode) {
		case crazy::HttpSession::WebSocketOpCode::text:
			// 收到文本消息：原样回显
			CRAZY_SYSTEM_INFO() << "收到文本消息: " << data;
			if (data == "close") {
				session->sendClose();
			}
			else {
				session->sendText(data);
			}
			break;
		case crazy::HttpSession::WebSocketOpCode::binary:
			CRAZY_SYSTEM_INFO() << "收到二进制消息，长度 = " << data.size();
			session->sendBinary(data);
			break;
		default:
			break;
		}
		});

	app.registerWebSocketCloseCallback([]() {
		CRAZY_SYSTEM_INFO() << "WebSocket 连接已关闭";
		});

	app.exec();
	return 0;
}
