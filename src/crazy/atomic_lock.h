/**
 * @file atomic_lock.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 原子锁
 * @version 0.1
 * @date 15
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <atomic>
#include <thread>
#include <chrono>

#include "crazy/nocopyable.h"

namespace crazy {
	/**
	 * @brief 原子锁.
	 */
	class AtomicLock final : public Noncopyable {
	public:
		/**
		 * @brief 构造函数.
		 */
		AtomicLock() noexcept;
		/**
		 * @brief 上锁.
		 */
		void lock() noexcept;
		/**
		 * @brief 尝试上锁.
		 */
		bool try_lock() noexcept;
		/**
		 * @brief 解锁.
		 */
		void unlock() noexcept;

	private:
		//! 原子标记
		std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
	};
	/**
	 * @brief 原子锁守护类.
	 */
	class AtomicLockGuard final : public Noncopyable {
	public:
		/**
		 * @brief 构造函数.
		 */
		explicit AtomicLockGuard(AtomicLock& lock);
		/**
		 * @brief 析构函数.
		 */
		~AtomicLockGuard() noexcept;

	private:
		//! 原子锁
		AtomicLock& lock_;
	};
}
