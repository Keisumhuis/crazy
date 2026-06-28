/**
 * @file nocpoyable.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 禁止拷贝类
 * @version 0.1
 * @date 01
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

namespace crazy {
	/**
	 * @brief 禁止拷贝类.
	 */
	class Noncopyable {
	public:
		/**
		 * @brief 拷贝构造函数.
		 */
		Noncopyable(const Noncopyable&) = delete;
		/**
		 * @brief 赋值运输符.
		 */
		void operator=(const Noncopyable&) = delete;

	protected:
		/**
		 * @brief 构造函数.
		 */
		Noncopyable() = default;
		/**
		 * @brief 析构函数.
		 */
		~Noncopyable() = default;
	};
}
