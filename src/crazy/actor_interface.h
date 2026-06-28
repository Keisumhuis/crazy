/**
 * @file actor_interface.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief actor 服务基类
 * @version 0.1
 * @date 15
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "crazy/cond_mutex.h"
#include "crazy/message_base.h"
#include "crazy/net/selector.h"

namespace crazy {
	/**
	 * @brief actor 服务基类.
	 */
	class ActorInterface : protected Selector {
	public:
		using ptr = std::shared_ptr<ActorInterface>;
		/**
		 * @brief 构造函数.
		 */
		explicit ActorInterface(const std::string& name);
		/**
		 * @brief 析构函数.
		 */
		virtual ~ActorInterface();
		/**
		 * @brief 获取名字.
		 */
		const std::string& getName() const;
		/**
		 * @brief 初始化.
		 */
		virtual void init();
		/**
		 * @brief 启动.
		 */
		void start();
		/**
		 * @brief 停止运行.
		 */
		virtual void stop();
		/**
		 * @brief 获取帮助手册.
		 */
		virtual std::map<std::string, std::string> helps();
		/**
		 * @brief 消息入队.
		 * @param message 消息
		 */
		virtual void enqueueMessage(MessageBase::ptr message);
		/**
		 * @brief 异步任务.
		 */
		void enqueueFunction(std::function<void()> function);
		/**
		 * @brief 注册异步任务.
		 */
		void registerAsyncTask(std::function<void()> function);
		/**
		 * @brief 消息队列长度.
		 */
		uint32_t messageQueueSize();
		/**
		 * @brief 异步任务队列长度.
		 */
		uint32_t asyncTaskQueueSize();

	protected:
		/**
		 * @brief 处理命令行消息.
		 * @param message 消息
		 */
		virtual void handleCommandLineMessgaBase(MessageBase::ptr request, MessageBase::ptr response);
		/**
		 * @brief 处理普通消息.
		 * @param message 消息
		 */
		virtual void handleMessgaBase(MessageBase::ptr message);
		/**
		 * @brief 线程执行函数.
		 */
		virtual void run();
		/**
		 * @brief 发送消息.
		 * @param message 消息
		 */
		void sendMessage(MessageBase::ptr message);

	private:
		/**
		 * @brief 处理消息.
		 * @param message 消息
		 */
		void onRecvMessgaBase(MessageBase::ptr message);

	protected:
		//! 名字
		std::string name_;
		//! 执行线程
		std::unique_ptr<std::thread> thread_ = nullptr;
		//! 运行状态
		std::atomic<bool> running_ = false;
		//! 条件变量互斥锁
		CondMutex condMutex_;
		//! 消息队列
		std::deque<MessageBase::ptr> messageQueue_;
		//! 异步执行函数
		std::deque<std::function<void()>> functionQueue_;
	};
}
