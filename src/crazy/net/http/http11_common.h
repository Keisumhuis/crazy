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

#include <cstddef>
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
	/**
	 * @brief 把分块长度格式化为小写十六进制文本.
	 */
	std::string httpChunkSizeToHex(std::size_t size);
	/**
	 * @brief 按 RFC 7230 第 4.1 节编码一个数据块：<十六进制长度>\r\n<数据>\r\n.
	 * @note 空数据不产生数据块（终止块由调用方单独写出）.
	 */
	std::string httpEncodeChunk(const std::string& data);
}
