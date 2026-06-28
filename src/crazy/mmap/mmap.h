/**
 * @file mmap.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief
 * @version 0.1
 * @date 2026-1-5
 *
 * @copyright Copyright (c) 2025
 */
#pragma once
#include <stdint.h>
#include <string>
#include "crazy/nocopyable.h"

namespace crazy {
	/**
	 * @brief 内存映射文件接口.
	 */
	class MmapInterface : public Noncopyable {
	public:
		/**
		 * @brief 默认构造.
		 */
		MmapInterface() = default;
		/**
		 * @brief 构造并立即打开文件.
		 */
		explicit MmapInterface(const std::string& filepath, uint64_t size);
		/**
		 * @brief 析构时自动关闭映射和文件句柄.
		 */
		~MmapInterface();
		/**
		 * @brief 显式打开并映射文件.
		 */
		void open(const std::string& filepath, uint64_t size = 0);
		/**
		 * @brief 关闭当前映射和文件.
		 */
		void close();
		/**
		 * @brief 获取映射内存起始地址.
		 */
		void* data() const;
		/**
		 * @brief 获取当前映射的字节大小.
		 */
		uint64_t size() const;
		/**
		 * @brief 获取当前关联的文件路径.
		 */
		const std::string& filepath() const;
		/**
		 * @brief 重新调整映射大小.
		 */
		void remmap(uint64_t new_size);

	protected:
		/**
		 * @brief 执行底层 mmap.
		 */
		void mmap();
		/**
		 * @brief 解除当前内存映射.
		 */
		void unmmap();
		/**
		 * @brief 确保当前未打开文件，否则抛出逻辑错误.
		 */
		void ensure_not_opened() const;
		/**
		 * @brief 将映射内存的更改同步到磁盘.
		 */
		void sync();

	private:
		//! 映射内存起始地址
		void* data_ = nullptr;
		//! 当前映射大小（字节）
		uint64_t size_ = 0;
		//! 文件路径
		std::string filepath_;
		//! 是否已成功打开
		bool is_opened_ = false;

#ifdef _WIN32
		//! Windows 文件句柄
		void* fileHandle_ = nullptr;
		//! Windows 映射对象句柄
		void* mappingHandle_ = nullptr;
#else
		//! POSIX 文件描述符
		int fd_ = -1;
#endif
	};

} // namespace crazy
