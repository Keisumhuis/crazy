/**
 * @file file_lock.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief
 * @version 0.1
 * @date 2026-2-25
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#if _WIN32
#include <Windows.h>
#else
#include <stdio.h>
#include <sys/file.h>
#endif

#include <string>

#include "crazy/nocopyable.h"

namespace crazy {
	/**
	 * 文件锁
	 */
	class FileLock final : public Noncopyable{
	public:
		/**
		 * @brief 构造函数
		 */
		explicit FileLock() = default;
		/**
		 * @brief 析构函数
		 */
		~FileLock();
		/**
		 * @brief 构造函数
		 */
		explicit FileLock(const std::string& filePath);
		/**
		 * @brief 设置文件路径
		 */
		void setFilePath(const std::string& filePath);
		/**
		 * @brief 上锁
		 */
		bool lock();
		/**
		 * @brief 解锁
		 */
		bool unlock();
		/**
		 * @brief 尝试上锁
		 */
		bool tryLock();

	private:
		//! 文件路径
		std::string filePath_;
#if _WIN32
		HANDLE file_ = nullptr;
#else
		//! 文件句柄
		FILE* file_ = nullptr;
#endif
	};
}
