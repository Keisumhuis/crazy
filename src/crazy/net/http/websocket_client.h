/**
 * @file websocket_client.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief WebSocket 同步客户端.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "crazy/net/http/websocket_parser.h"
#include "crazy/net/socket.h"

namespace crazy {
	/**
	 * @brief WebSocket 同步客户端.
	 */
	class WebSocketClient {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<WebSocketClient>;
		/**
		 * @brief WebSocket 操作码.
		 */
		enum class OpCode : uint8_t {
			continuation = 0x0,  //! 分片延续帧
			text = 0x1,          //! 文本帧
			binary = 0x2,        //! 二进制帧
			close = 0x8,         //! 关闭帧
			ping = 0x9,          //! Ping 帧
			pong = 0xA,          //! Pong 帧
		};
		//! 消息回调类型
		using MessageCallback = std::function<void(const std::string& data, OpCode opcode)>;
		//! 连接回调类型
		using ConnectCallback = std::function<void()>;
		//! 关闭回调类型
		using CloseCallback = std::function<void()>;

		/**
		 * @brief 构造函数.
		 */
		WebSocketClient();
		/**
		 * @brief 析构函数.
		 */
		~WebSocketClient();

		/**
		 * @brief 注册连接建立回调.
		 */
		void registerConnectCallback(ConnectCallback callback);
		/**
		 * @brief 注册消息回调.
		 */
		void registerMessageCallback(MessageCallback callback);
		/**
		 * @brief 注册连接关闭回调.
		 */
		void registerCloseCallback(CloseCallback callback);

		/**
		 * @brief 连接并完成握手（阻塞）.
		 * @param uri WebSocket 地址（ws:// 或 wss://）.
		 * @return 握手成功返回 true.
		 */
		bool connect(const std::string& uri);

		/**
		 * @brief 发送文本消息.
		 */
		void sendText(const std::string& data);
		/**
		 * @brief 发送二进制消息.
		 */
		void sendBinary(const std::string& data);
		/**
		 * @brief 发送关闭帧并关闭连接.
		 * @param code 关闭状态码.
		 */
		void sendClose(uint16_t code = 1000);

		/**
		 * @brief 阻塞接收循环，直到连接关闭（内部触发消息回调）.
		 */
		void run();

		/**
		 * @brief 关闭连接.
		 */
		void close();
		/**
		 * @brief 是否已连接.
		 */
		bool isConnected() const;

	private:
		/**
		 * @brief 发送原始帧（客户端帧必须带 mask）.
		 */
		void sendFrame(OpCode opcode, const std::string& data);
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
		//! WebSocket 帧解析器
		websocket_parser wsParser_{};
		//! WebSocket 帧解析器设置
		websocket_parser_settings wsSettings_{};
		//! 分片帧缓存
		std::string fragmentBuffer_;
		//! 分片帧原始操作码
		OpCode fragmentOpcode_ = OpCode::text;
		//! 连接回调
		ConnectCallback connectCallback_;
		//! 消息回调
		MessageCallback messageCallback_;
		//! 关闭回调
		CloseCallback closeCallback_;
		//! 是否已连接
		bool connected_ = false;
		//! 是否已发送关闭帧
		bool closeSent_ = false;
	};
}  // namespace crazy
