/**
 * @file connection.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief socket 连接器
 * @version 0.1
 * @date 21
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
#include "crazy/net/socket.h"
#include "crazy/net/selector.h"

namespace crazy {
	/**
	 * @brief socket 连接器.
	 */
	class Connection final : public Socket {
	public:
		using ptr = std::shared_ptr<Connection>;
		/**
		 * @brief 构造函数.
		 */
		explicit Connection();
		/**
		 * @brief 建立连接.
		 */
		void connect(const std::string& ip, uint16_t port);
		/**
		 * @brief 注册链接回调.
		 */
		void registerConnectedCallback(std::function<void()> callback);
		/**
		 * @brief 注册断开链接回调.
		 */
		void registerDisconnectedCallback(std::function<void()> callback);
		/**
		 * @brief 注册收到完整报文回调.
		 */
		void registerRecvMessageCallback(std::function<void(MessageBase::ptr)> callback);
		/**
		 * @brief 注册注册事件回调.
		 */
		void registerSelectEventRegisterCallback(std::function<void(int32_t, SelectorEventType, std::function<void()>)> registerSelectEvent);
		/**
		 * @brief 注册取消事件回调.
		 */
		void registerSelectEventUnRegisterCallback(std::function<void(int32_t, SelectorEventType)> unregisterSelectEvent);
		/**
		 * @brief 可读事件.
		 */
		void onReadEvent();
		/**
		 * @brief 可写事件.
		 */
		void onWriteEvent();
		/**
		 * @brief 发送消息.
		 */
		void sendMessage(MessageBase::ptr message);
		/**
		 * @brief 获取链接状态.
		 */
		bool isConnected() const;
		/**
		 * @brief 发送心跳包.
		 */
		void sendHeartbeat();

	protected:
		/**
		 * @brief 发送已建立链接.
		 */
		void sendConnected();
		/**
		 * @brief 发送断开链接.
		 */
		void sendDisconnected();
		/**
		 * @brief 发送接收到完整报文.
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
		//! 是否已经连接上服务器
		bool isConnected_ = false;
		//! 连接上服务器
		std::function<void()> onConnected_;
		//! 断开链接
		std::function<void()> onDisconnected_;
		//! 接受到消息
		std::function<void(MessageBase::ptr)> onRecvMessgae_;
		//! 注册事件
		std::function<void(int32_t, SelectorEventType, std::function<void()>)> registerSelectEvent_;
		//! 取消所有事件
		std::function<void(int32_t, SelectorEventType)> unregisterSelectEvent_;
		//! 数据解析器
		Decoder decoder_;
		//! 接收缓冲区
		Buffer recvBuffer_;
		//! 数据字符串化器
		Encoder encoder_;
		//! 发送缓冲区
		Buffer sendBuffer_;
	};
}
