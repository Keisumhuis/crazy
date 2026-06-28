/**
 * @file mmap_vector.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief
 * @version 0.1
 * @date 2026-1-8
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include "crazy/mmap/mmap.h"
#include <type_traits>
#include <cstdint>
#include <algorithm>

namespace crazy {

	/**
	 * @brief 向量容器头.
	 */
#pragma pack(push, 1)
	struct MmapVectorHeader {
		//! 大小
		uint64_t size_ = 0;
		//! 容量
		uint64_t capacity_ = 0;
	};
#pragma pack(pop)

	/**
	 * @brief 向量容器-内存映射.
	 */
	template <typename value_type>
	class MmapVector : public MmapInterface {
		static_assert(std::is_trivially_copyable_v<value_type>, "MmapVector only supports trivially copyable types.");
	public:
		/**
		 * @brief 默认构造.
		 */
		MmapVector() = default;
		/**
		 * @brief 构造并立即打开文件.
		 * @param filepath 文件路径
		 * @param capacity 初始容量（仅在创建新文件时生效）
		 */
		explicit MmapVector(const std::string& filepath, uint64_t capacity)
			: MmapInterface(filepath, sizeof(MmapVectorHeader) + sizeof(value_type) * capacity) {
			update_pointers();
			initialize_header_if_new(capacity);
		}
		/**
		 * @brief 显式打开并映射文件.
		 * @param filepath 文件路径
		 * @param capacity 初始容量（仅在创建新文件时生效）
		 */
		void open(const std::string& filepath, uint64_t capacity = 0) {
			MmapInterface::open(filepath, sizeof(MmapVectorHeader) + sizeof(value_type) * capacity);
			update_pointers();
			initialize_header_if_new(capacity);
		}
		/**
		 * @brief 获取当前大小.
		 */
		uint64_t size() const {
			return header_ ? header_->size_ : 0;
		}
		/**
		 * @brief 获取当前容量.
		 */
		uint64_t capacity() const {
			return header_ ? header_->capacity_ : 0;
		}
		/**
		 * @brief 开始元素.
		 */
		value_type* begin() {
			return values_;
		}
		/**
		 * @brief 结束元素.
		 */
		value_type* end() {
			return header_ ? values_ + header_->size_ : nullptr;
		}
		/**
		 * @brief 开始元素（const）.
		 */
		const value_type* begin() const {
			return values_;
		}
		/**
		 * @brief 结束元素（const）.
		 */
		const value_type* end() const {
			return header_ ? values_ + header_->size_ : nullptr;
		}
		/**
		 * @brief 下标访问（可写，自动扩容）.
		 * @param index 元素索引
		 * @return 对应元素的引用（若 index >= capacity，则自动扩容）
		 */
		value_type& operator[](uint64_t index) {
			if (!header_ || index >= header_->capacity_) {
				grow_to_accommodate(index);
			}
			// 更新 size_ if needed (optional: you may want push-like semantics)
			if (index >= header_->size_) {
				header_->size_ = index + 1;
			}
			return values_[index];
		}
		/**
		 * @brief 下标访问（只读）.
		 * @param index 元素索引
		 * @return 对应元素的 const 引用（不扩容，越界行为未定义）
		 */
		const value_type& operator[](uint64_t index) const {
			return values_[index];
		}
		/**
		 * @brief 获取指定元素（带边界检查，不扩容）.
		 * @param index 元素索引
		 * @return 若索引 < size() 则返回指针，否则返回 nullptr
		 */
		value_type* at(uint64_t index) {
			if (header_ && index < header_->size_) {
				return values_ + index;
			}
			return nullptr;
		}
		/**
		 * @brief 获取指定元素（const，带边界检查，不扩容）.
		 */
		const value_type* at(uint64_t index) const {
			if (header_ && index < header_->size_) {
				return values_ + index;
			}
			return nullptr;
		}

	protected:
		/**
		 * @brief 扩容以容纳指定索引（index 必须可被容纳）.
		 */
		void grow_to_accommodate(uint64_t index) {
			if (!header_) return;

			uint64_t new_capacity = index + 1;
			if (header_->capacity_ > 0) {
				new_capacity = new_capacity > header_->capacity_ * 2 ? new_capacity : header_->capacity_ * 2;
			}
			else {
				new_capacity = new_capacity > 1 ? new_capacity : 1;
			}

			uint64_t new_mapping_size = sizeof(MmapVectorHeader) + sizeof(value_type) * new_capacity;
			remmap(new_mapping_size);

			update_pointers();

			if (header_) {
				header_->capacity_ = new_capacity;
			}
		}
		/**
		 * @brief 更新内部指针.
		 */
		virtual void update_pointers() {
			if (data()) {
				header_ = static_cast<MmapVectorHeader*>(data());
				values_ = reinterpret_cast<value_type*>(
					static_cast<char*>(data()) + sizeof(MmapVectorHeader));
			}
			else {
				header_ = nullptr;
				values_ = nullptr;
			}
		}
		/**
		 * @brief 若为新创建的映射，则初始化 header.
		 */
		void initialize_header_if_new(uint64_t capacity) {
			if (header_ && capacity > 0 && header_->capacity_ == 0) {
				header_->size_ = 0;
				header_->capacity_ = capacity;
			}
		}

	protected:
		//! 容器头
		MmapVectorHeader* header_ = nullptr;
		//! 数据首地址
		value_type* values_ = nullptr;
	};
	
}
