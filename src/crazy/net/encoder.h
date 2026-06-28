/**
 * @file encoder.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 协议序列化器
 * @version 0.1
 * @date 23
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <memory>

#include "crazy/buffer.h"
#include "crazy/message_base.h"

namespace crazy {
	/**
	 * @brief 协议序列化器.
	 */
	class Encoder {
	public:
		/**
		 * @brief 构造函数.
		 */
		explicit Encoder(Buffer& buffer);
		/**
		 * @brief .
		 */
		void stringify(MessageBase::ptr message);
		/**
		 * @brief 重置
		 */
		void reset();

	private:
		//! 数据缓冲区
		Buffer& buffer_;
	};
}
