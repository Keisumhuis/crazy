/**
 * @file message_base.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 消息类
 * @version 0.1
 * @date 15
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <memory>
#include <string>

#include "crazy/buffer.h"

namespace crazy {
	/**
	 * @brief 消息基类.
	 */
	class MessageBase {
	public:
		using ptr = std::shared_ptr<MessageBase>;
		/**
		 * @brief 析构函数.
		 */
		virtual ~MessageBase() = default;
		/**
		 * @brief 设置来源.
		 */
		void setSource(const std::string& source);
		/**
		 * @brief 获取来源.
		 */
		const std::string& getSource() const;
		/**
		 * @brief 获取会话id.
		 */
		uint64_t getSessionId() const;
		/**
		 * @brief 设置会话id.
		 */
		void setSessionId(uint64_t sessionId);
		/**
		 * @brief 获取cmd.
		 */
		uint64_t getCmd() const;
		/**
		 * @brief 设置cmd.
		 */
		void setCmd(uint64_t cmd);
		/**
		 * @brief 设置消息备注.
		 */
		void setComment(const std::string& comment);
		/**
		 * @brief 获取消息备注.
		 */
		const std::string& getComment() const;
		/**
		 * @brief 获取原始数据.
		 */
		const std::string& getData() const;
		/**
		 * @brief 设置原始数据.
		 */
		void setData(const std::string& data);
		/**
		 * @brief 创建应答报文.
		 */
		MessageBase::ptr createResponse();

	private:
		//! 来源
		std::string source_;
		//! 会话id
		uint64_t sessionId_;
		//! 路由字段
		uint64_t cmd_;
		//! 备注字段
		std::string comment_;
		//! 消息正文
		std::string data_;
	};

	/**
	 * @brief 内部消息.
	 */
	template <typename value_type>
	class InternalMessage : public MessageBase {
	public:
		/**
		 * @brief 设置消息值.
		 */
		void setValue(const value_type& value) {
			value_ = value;
		}
		/**
		 * @brief 获取消息值.
		 */
		const value_type& getValue() const {
			return value_;
		}

	private:
		//! 消息值
		value_type value_;
	};
}
