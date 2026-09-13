/**
 * @file http_session.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 单连接会话.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_router.h"
#include "crazy/net/http/websocket_parser.h"
#include "crazy/net/selector.h"
#include "crazy/net/socket.h"

namespace crazy {
	/**
	 * @brief HTTP 会话.
	 */
	class HttpSession : public std::enable_shared_from_this<HttpSession> {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpSession>;
		/**
		 * @brief WebSocket 帧操作码.
		 */
		enum class WebSocketOpCode : uint8_t {
			continuation = 0x0,  //! 分片延续帧
			text = 0x1,          //! 文本帧
			binary = 0x2,        //! 二进制帧
			close = 0x8,         //! 关闭帧
			ping = 0x9,          //! Ping 帧
			pong = 0xA,          //! Pong 帧
		};
		//! WebSocket 消息回调类型（session 为当前会话，data 为解码后的消息内容，opcode 为 text 或 binary）
		using WebSocketMessageCallback = std::function<void(HttpSession::ptr session, const std::string& data, WebSocketOpCode opcode)>;
		//! WebSocket 关闭回调类型
		using WebSocketCloseCallback = std::function<void()>;
		//! WebSocket 连接回调类型（session 为刚建立连接的会话）
		using WebSocketConnectCallback = std::function<void(HttpSession::ptr session)>;
		/**
		 * @brief 构造函数.
		 */
		HttpSession(Socket::ptr socket, HttpRouter::ptr router);
		/**
		 * @brief 析构函数.
		 */
		~HttpSession();
		/**
		 * @brief 获取 socket.
		 */
		socket_t socket() const;
		/**
		 * @brief 可读事件.
		 */
		void onReadEvent();
		/**
		 * @brief 可写事件.
		 */
		void onWriteEvent();
		/**
		 * @brief 注册事件回调.
		 */
		void registerEventCallback(std::function<void(socket_t, SelectorEventType, std::function<void()>)> callback);
		/**
		 * @brief 取消注册事件回调.
		 */
		void registerUnregisterEventCallback(std::function<void(socket_t, SelectorEventType)> callback);
		/**
		 * @brief 注册会话线程任务回调.
		 */
		void registerPostCallback(std::function<void(std::function<void()>)> callback);
		/**
		 * @brief 注册会话关闭回调.
		 */
		void registerCloseCallback(std::function<void()> callback);
		/**
		 * @brief 关闭 socket.
		 */
		void closeSocket();
		/**
		 * @brief 注册 WebSocket 消息回调.
		 */
		void registerWebSocketMessageCallback(WebSocketMessageCallback callback);
		/**
		 * @brief 注册 WebSocket 连接建立回调.
		 */
		void registerWebSocketConnectCallback(WebSocketConnectCallback callback);
		/**
		 * @brief 注册 WebSocket 关闭回调.
		 */
		void registerWebSocketCloseCallback(WebSocketCloseCallback callback);
		/**
		 * @brief 发送 WebSocket 文本消息.
		 */
		void sendText(const std::string& data);
		/**
		 * @brief 发送 WebSocket 二进制消息.
		 */
		void sendBinary(const std::string& data);
		/**
		 * @brief 发送 WebSocket Ping 帧.
		 */
		void sendPing(const std::string& data = "");
		/**
		 * @brief 发送 WebSocket Pong 帧.
		 */
		void sendPong(const std::string& data = "");
		/**
		 * @brief 发送 WebSocket Close 帧并关闭连接.
		 */
		void sendClose(uint16_t code = 1000);

	protected:
		/**
		 * @brief 处理完整请求.
		 */
		void handleRequest(HttpRequest::ptr request);
		/**
		 * @brief 发送 HTTP 响应.
		 */
		void sendResponse(HttpResponse::ptr response);
		/**
		 * @brief 尝试直接发送缓冲数据.
		 */
		void flushWrite();
		/**
		 * @brief 触发会话关闭.
		 */
		void onClose();
		/**
		 * @brief 处理 WebSocket 握手升级.
		 */
		void handleWebSocketUpgrade(HttpRequest::ptr request);
		/**
		 * @brief 发送原始 WebSocket 帧.
		 */
		void sendFrame(WebSocketOpCode opcode, const std::string& data);
		/**
		 * @brief 处理收到的完整 WebSocket 消息.
		 */
		void handleWebSocketMessage(const std::string& data, WebSocketOpCode opcode);
		/**
		 * @brief 处理收到的 WebSocket 控制帧.
		 */
		void handleWebSocketControlFrame(WebSocketOpCode opcode, const std::string& data);
		/**
		 * @brief websocket_parser 帧头回调.
		 */
		static int32_t onFrameHeader(websocket_parser* parser);
		/**
		 * @brief websocket_parser 帧体回调.
		 */
		static int32_t onFrameBody(websocket_parser* parser, const char* data, size_t length);
		/**
		 * @brief websocket_parser 帧结束回调.
		 */
		static int32_t onFrameEnd(websocket_parser* parser);

	private:
		//! 客户端 socket
		Socket::ptr socket_;
		//! HTTP 路由
		HttpRouter::ptr router_;
		//! 请求解析器
		HttpRequestParser parser_;
		//! 发送缓冲
		std::string sendBuffer_;
		//! 注册事件回调
		std::function<void(socket_t, SelectorEventType, std::function<void()>)> registerEvent_;
		//! 取消注册事件回调
		std::function<void(socket_t, SelectorEventType)> unregisterEvent_;
		//! 关闭会话回调
		std::function<void()> closeCallback_;
		//! 是否注册写事件
		bool writeRegistered_ = false;
		//! 写完后关闭连接
		bool closeAfterWrite_ = false;
		//! 会话线程任务回调
		std::function<void(std::function<void()>)> postCallback_;
		//! 会话是否已关闭
		bool closed_ = false;
		//! WebSocket 帧解析器
		websocket_parser wsParser_;
		//! WebSocket 帧解析器设置
		websocket_parser_settings wsSettings_;
		//! 是否已升级为 WebSocket
		bool upgraded_ = false;
		//! 分片帧缓存（累积 fin=0 的分片数据）
		std::string fragmentBuffer_;
		//! 分片帧的原始操作码（记录第一个分片的 opcode）
		WebSocketOpCode fragmentOpcode_ = WebSocketOpCode::text;
		//! WebSocket 消息回调
		WebSocketMessageCallback wsMessageCallback_;
		//! WebSocket 连接回调
		WebSocketConnectCallback wsConnectCallback_;
		//! WebSocket 关闭回调
		WebSocketCloseCallback wsCloseCallback_;
		//! 已发送 Close 帧标志（避免重复发送）
		bool closeSent_ = false;
	};
}
