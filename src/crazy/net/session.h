/**
 * @file session.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 会话管理
 * @version 0.1
 * @date 16
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <functional>
#include <memory>

#include "crazy/buffer.h"
#include "crazy/common.h"
#include "crazy/net/decoder.h"
#include "crazy/net/encoder.h"
#include "crazy/message_base.h"
#include "crazy/net/selector.h"
#include "crazy/net/socket.h"

namespace crazy {
	/**
	 * @brief 会话管理.
	 */
	class Session {
	public:
		using ptr = std::shared_ptr<Session>;
		/**
		 * @brief 构造函数.
		 */
		explicit Session(Socket::ptr socket);
		/**
		 * @brief 析构函数.
		 */
		~Session();
		/**
		 * @brief 获取会话id.
		 */
		uint64_t getSessionId() const;
		/**
		 * @brief 获取socket描述符.
		 */
		socket_t socket() const;
		/**
		 * @brief 断开链接.
		 */
		void close();
		/**
		 * @brief 可读事件.
		 */
		void onReadEvent();
		/**
		 * @brief 可写事件.
		 */
		void onWriteEvent();
		/**
		 * @brief 获取对端地址.
		 */
		const std::string& remoteAddress();
		/**
		 * @brief 注册断开连接回调.
		 */
		void registerDisconnectCallback(std::function<void()> callback);
		/**
		 * @brief 注册收到完整报文回调.
		 */
		void registerMessageCallback(std::function<void(MessageBase::ptr)> callback);
		/**
		 * @brief 注册注册select 事件函数.
		 */
		void registerSelectEventRegisterCallback(std::function<void(int32_t, SelectorEventType, std::function<void()>)> registerSelectEvent);
		/**
		 * @brief 注册取消注册select 事件函数.
		 */
		void registerSelectEventUnRegisterCallback(std::function<void(int32_t, SelectorEventType)> unregisterSelectEvent);
		/**
		 * @brief 发送消息.
		 */
		void sendMessage(MessageBase::ptr message);
		/**
		 * @brief 最后一个心跳包时间戳.
		 */
		uint64_t lastHeartbeatTimestamp() const;

	protected:
		/**
		 * @brief 发送断开链接.
		 */
		void sendDisconnect();
		/**
		 * @brief 发送报文.
		 */
		void sendRecvMessage(MessageBase::ptr message);
		/**
		 * @brief 解析到完整报文.
		 */
		void onParseFinishMessgae(MessageBase::ptr message);
		/**
		 * @brief 解析异常.
		 */
		void onParseException();

	private:
		//! 会话id
		uint64_t sessionId_ = 0;
		//! 客户端socket
		Socket::ptr socket_;
		//! 断开链接回调
		std::function<void()> disconnectCallback_;
		//! 收到完整报文回调
		std::function<void(MessageBase::ptr)> onMessageCallback_;
		//! 注册select 事件回调
		std::function<void(int32_t, SelectorEventType, std::function<void()>)> registerSelectEvent_;
		//! 取消注册注册select 事件回调
		std::function<void(int32_t, SelectorEventType)> unregisterSelectEvent_;
		//! 最后一次收到心跳的时间戳
		uint64_t lastHeartbeatTimestamp_;
		//! 数据解析器
		Decoder decoder_;
		//! 接收缓冲区
		Buffer recvBuffer_;
		//! 数据字符串化器
		Encoder encoder_;
		//! 发送缓冲区
		Buffer sendBuffer_;
		//! 注册select写事件标志
		bool isSelectWriteEventRegistered_ = false;
	};
}
