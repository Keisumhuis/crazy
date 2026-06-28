/**
 * @file decoder.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 协议解析器
 * @version 0.1
 * @date 23
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <functional>
#include <memory>

#include "crazy/buffer.h"
#include "crazy/message_base.h"
#include "crazy/nocopyable.h"

namespace crazy {
	/**
	 * @brief 协议解析状态.
	 */
	enum class DecodeStatus : uint8_t {
		begin, length, command, body
	};
	/**
	 * @brief 协议解析器.
	 */
	class Decoder {
	public:
		/**
		 * @brief 构造函数.
		 */
		explicit Decoder(Buffer& buffer);
		/**
		 * @brief 析构函数.
		 */
		virtual ~Decoder() = default;
		/**
		 * @brief 设置最大报文长度.
		 */
		void setMaxMessgaeLength(uint64_t length);
		/**
		 * @brief 获取最大报文长度.
		 */
		uint64_t getMaxMessgaeLength() const;
		/**
		 * @brief 解析方法.
		 */
		virtual void parse();
		/**
		 * @brief 注册解析成功方法.
		 */
		void registerParseFinishCallback(std::function<void(MessageBase::ptr)> callback);
		/**
		 * @brief 注册解析异常方法.
		 */
		void registerParseExceptionCallback(std::function<void()> callback);
		/**
		 * @brief 重置
		 */
		void reset();

	protected:
		/**
		 * @brief 发送解析完的报文.
		 */
		void sendFinishMessageBase(MessageBase::ptr message);
		/**
		 * @brief 发送异常回调.
		 */
		void sendException();

	private:
		//! 最大报文长度
		uint64_t maxMessageLength_ = 10 * 1024 * 1024;
		//! 数据缓冲区
		Buffer& buffer_;
		//! 解析状态
		DecodeStatus status_ = DecodeStatus::begin;
		//! 报文长度
		uint64_t size_ = 0;
		//! 缓存消息
		MessageBase::ptr cacheMessage_ = nullptr;
		//! 解析完成回调
		std::function<void(MessageBase::ptr)> sendMessage_;
		//! 解析异常回调
		std::function<void()> sendException_;
	};
}
