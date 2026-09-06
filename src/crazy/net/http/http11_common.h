/**
 * @file http11_common.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief HTTP/1.1 基础定义
 * @version 0.1
 * @date 16
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <string>

#include "crazy/net/http/http_parser.h"
#include "crazy/net/http/multipart_parser.h"
#include "crazy/net/http/websocket_parser.h"

namespace crazy {
	/**
	 * @brief http请求方法.
	 */
	enum class HttpMethod : int32_t {
#undef DELETE
#define XX(num, name, string) name = num,
		HTTP_METHOD_MAP(XX)
#undef XX
	};
#define CRAZY_HTTP_METHOD_CONSTANT(num, name, string) \
	inline constexpr HttpMethod name = HttpMethod::name;
	HTTP_METHOD_MAP(CRAZY_HTTP_METHOD_CONSTANT)
#undef CRAZY_HTTP_METHOD_CONSTANT
	/**
	 * @brief 将http方法转换为字符串.
	 */
	std::string httpMethodToString(HttpMethod method);
	/**
	 * @brief 将http方法字符串转换为枚举.
	 */
	HttpMethod httpMethodFromString(const std::string& method);
	/**
	 * @brief http请求状态码.
	 */
	enum class HttpStatus : int32_t {
#define XX(code, name, string) name = code,
		HTTP_STATUS_MAP(XX)
#undef XX
	};
	/**
	 * @brief 将http状态码转换为字符串.
	 */
	std::string httpStatusToString(HttpStatus status);
	/**
	 * @brief 将http状态码字符串转换为枚举.
	 */
	HttpStatus httpStatusFromString(const std::string& status);
}
