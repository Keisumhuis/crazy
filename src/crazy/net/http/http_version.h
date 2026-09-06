/**
 * @file http_version.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 版本定义
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 */
#pragma once

#include <stdint.h>

#include <string>

namespace crazy {
	/**
	 * @brief http版本.
	 */
	class HttpVersion final {
	public:
		/**
		 * @brief 构造函数.
		 */
		HttpVersion();
		/**
		 * @brief 构造函数.
		 */
		explicit HttpVersion(uint8_t major, uint8_t minor);
		/**
		 * @brief 获取http主版本.
		 */
		uint8_t getMajor() const;
		/**
		 * @brief 获取http次版本.
		 */
		uint8_t getMinor() const;
		/**
		 * @brief 将http版本转换为字符串.
		 */
		std::string toString() const;
		/**
		 * @brief 比较两个http版本是否相等.
		 */
		bool operator==(const HttpVersion& other) const;
		/**
		 * @brief 比较两个http版本是否小于.
		 */
		bool operator<(const HttpVersion& other) const;
		/**
		 * @brief 比较两个http版本是否不相等.
		 */
		bool operator!=(const HttpVersion& other) const;
		/**
		 * @brief 比较两个http版本是否大于等于.
		 */
		bool operator>=(const HttpVersion& other) const;
		/**
		 * @brief 比较两个http版本是否大于.
		 */
		bool operator>(const HttpVersion& other) const;
		/**
		 * @brief 比较两个http版本是否小于等于.
		 */
		bool operator<=(const HttpVersion& other) const;

	private:
		//! http版本号的主版本
		uint8_t major_ = 1;
		//! http版本号的次版本
		uint8_t minor_ = 1;
	};
	/**
	 * @brief http版本号的默认值.
	 */
	class HttpVersions {
	public:
		//! HTTP/0.9
		static HttpVersion HTTP_0_9;
		//! HTTP/1.0
		static HttpVersion HTTP_1_0;
		//! HTTP/1.1
		static HttpVersion HTTP_1_1;
	};
}
