/**
 * @file websocket_client.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief WebSocket 客户端（阻塞版 + 事件驱动版）.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "crazy/net/http/websocket_parser.h"
#include "crazy/net/selector.h"
#include "crazy/net/socket.h"

namespace crazy {
	class HttpResponseParser;
	/**
	 * @brief WebSocket 客户端.
	 * @note 两种用法：
	 *  - 阻塞：`connect()` 完成握手，`run()` 收帧，回调在调用线程触发；
	 *  - 事件驱动：`connectAsync()` 建连并发出握手请求，之后由 `Selector` / `ActorInterface`
	 *    驱动 `onReadable()` / `onWritable()`，回调在 selector 所在线程触发.
	 *    调用 `registerSelectorCallbacks()` 之后，读/写事件由客户端按自身状态自动注册与注销.
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
		//! 事件注册回调类型（`ActorInterface::registerEventOnThread` / `Selector::registerEvent`）
		using RegisterEventCallback = std::function<void(int32_t, SelectorEventType, std::function<void()>)>;
		//! 事件注销回调类型
		using UnregisterEventCallback = std::function<void(int32_t, SelectorEventType)>;

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
		 * @brief 建连并发出握手请求后立即返回（事件驱动版）.
		 * @param uri WebSocket 地址（ws:// 或 wss://）.
		 * @param error 失败原因.
		 * @return TCP 连接（wss 含 TLS 握手）建立且握手请求已发出返回 true.
		 * @note 建连与 wss 的 TLS 握手仍是阻塞的；握手响应与后续帧改为非阻塞，由
		 * `onReadable()` 驱动，握手完成时才触发连接回调.
		 */
		bool connectAsync(const std::string& uri, std::string& error);

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
		 * @brief socket 可读时调用：完成握手、解析帧并触发回调.
		 * @return 连接仍可继续返回 true；对端关闭或出错返回 false.
		 * @note 事件驱动模式下会把已到达的数据读干净（读到 would-block 为止）.
		 */
		bool onReadable();
		/**
		 * @brief socket 可写时调用：把发送缓冲里剩下的数据写完.
		 * @return 仍有待发送数据返回 true.
		 */
		bool onWritable();
		/**
		 * @brief 是否有待发送数据.
		 */
		bool hasPendingWrite() const;
		/**
		 * @brief 当前 socket，未连接时返回 INVALID_SOCKET.
		 */
		socket_t socket() const;
		/**
		 * @brief 注册事件注册/注销回调.
		 * @note 注册后读事件常驻、写事件只在有积压数据时注册，全部由客户端维护；
		 * 通常直接绑定 `ActorInterface::registerEventOnThread` / `unregisterEventOnThread`.
		 */
		void registerSelectorCallbacks(RegisterEventCallback onRegister, UnregisterEventCallback onUnregister);
		/**
		 * @brief 注销本客户端占用的读写事件（`close()` 内部已调用）.
		 */
		void detachSelector();

		/**
		 * @brief 关闭连接.
		 */
		void close();
		/**
		 * @brief 是否已连接.
		 */
		bool isConnected() const;
		/**
		 * @brief 是否处于事件驱动模式（`connectAsync()` 之后为 true）.
		 */
		bool isAsync() const;

	private:
		/**
		 * @brief 重置连接相关状态（重连前调用）.
		 */
		void resetSession();
		/**
		 * @brief 建立 TCP 连接，wss 还要完成 TLS 握手.
		 */
		bool openTransport(const std::string& uri, std::string& host, uint16_t& port, std::string& error);
		/**
		 * @brief 发送 HTTP Upgrade 请求.
		 */
		bool sendHandshakeRequest(const std::string& uri, const std::string& host);
		/**
		 * @brief 阻塞写完一段数据.
		 */
		bool sendRaw(const std::string& data);
		/**
		 * @brief 处理刚收到的字节：先喂握手解析器，握手完成后再喂帧解析器.
		 */
		void processIncoming(const char* data, size_t length);
		/**
		 * @brief 把发送缓冲尽量写出去.
		 * @return 缓冲已清空返回 true；还有剩余（需要写事件）返回 false.
		 */
		bool flushSendBuffer();
		/**
		 * @brief 按当前状态注册/注销读写事件.
		 */
		void updateInterest();
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
		//! 握手响应解析器（事件驱动模式下等待 101 / 失败响应）
		std::shared_ptr<HttpResponseParser> handshakeParser_;
		//! 本次握手的 Sec-WebSocket-Key
		std::string handshakeKey_;
		//! WebSocket 帧解析器
		websocket_parser wsParser_{};
		//! WebSocket 帧解析器设置
		websocket_parser_settings wsSettings_{};
		//! 分片帧缓存
		std::string fragmentBuffer_;
		//! 分片帧原始操作码
		OpCode fragmentOpcode_ = OpCode::text;
		//! 待发送数据
		std::string sendBuffer_;
		//! 连接回调
		ConnectCallback connectCallback_;
		//! 消息回调
		MessageCallback messageCallback_;
		//! 关闭回调
		CloseCallback closeCallback_;
		//! 事件注册回调
		RegisterEventCallback registerEvent_;
		//! 事件注销回调
		UnregisterEventCallback unregisterEvent_;
		//! 已注册事件的 fd（-1 表示未注册）
		int32_t watchedFd_ = -1;
		//! 读事件是否已注册
		bool readRegistered_ = false;
		//! 写事件是否已注册
		bool writeRegistered_ = false;
		//! 是否已连接（握手完成）
		bool connected_ = false;
		//! 是否在等握手响应
		bool handshakePending_ = false;
		//! 是否已发送关闭帧
		bool closeSent_ = false;
		//! 是否事件驱动模式
		bool async_ = false;
		//! 关闭帧写完后立即关闭连接
		bool closeAfterFlush_ = false;
	};
}
