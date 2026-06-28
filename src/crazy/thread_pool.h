/**
 * @file thread_pool.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief 线程池
 * @version 0.1
 * @date 2025-08-31
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <memory>
#include <vector>

#include "crazy/actor_interface.h"
#include "crazy/nocopyable.h"

namespace crazy {
	/**
	 * @brief 线程池.
	 */
	class ThreadPool : Noncopyable {
	public:
		//! 指针指针声明
		using ptr = std::shared_ptr<ThreadPool>;
		/**
		 * @brief 构造函数.
		 */
		explicit ThreadPool(uint32_t size = std::thread::hardware_concurrency());
		/**
		 * @brief 析构函数.
		 */
		virtual ~ThreadPool();
		/**
		 * @brief 启动线程池.
		 */
		void start();
		/**
		 * @brief 停止线程池.
		 */
		void stop();
		/**
		 * @brief 新增异步任务.
		 */
		void enqueueRunnable(std::function<void()> runnable);
		/**
		 * @brief 获取一个线程
		 */
		ActorInterface::ptr getActorImplement();

	private:
		//! 执行线程索引
		uint32_t index_ = 0;
		//! 线程数
		uint32_t threadSize_ = 0;
		//! 线程实例
		std::vector<ActorInterface::ptr> threads_;
	};
}
