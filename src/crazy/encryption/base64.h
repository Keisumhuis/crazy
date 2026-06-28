/**
 * @file base64.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief base64加解密
 * @version 0.1
 * @date 15
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <string>

#include "crazy/nocopyable.h"

namespace crazy {
	/**
	 * @brief base64加解密.
	 */
	class Base64 final : public Noncopyable {
	public:
		/**
		 * @brief 加密.
		 */
		static std::string encryption(const std::string& message);
		/**
		 * @brief 解密.
		 */
		static std::string decryption(const std::string& message);

	private:
		/**
		 * @brief base64字符.
		 */
		static const std::string BASE64_CHARS;
		/**
		 * @brief 判断字符是否为base64字符.
		 */
		static inline bool isBase64(unsigned char c);
	};
}
