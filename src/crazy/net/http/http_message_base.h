/**
 * @file http_message_base.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 消息基类.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "crazy/net/http/http_header.h"
#include "crazy/net/http/http_multipart.h"
#include "crazy/net/http/http_version.h"

namespace crazy {
	/**
	 * @brief HTTP 消息基类.
	 */
	class HttpMessageBase {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpMessageBase>;
		/**
		 * @brief 析构函数.
		 */
		virtual ~HttpMessageBase() = default;
		/**
		 * @brief 获取 HTTP 版本.
		 */
		HttpVersion version() const;
		/**
		 * @brief 设置 HTTP 版本.
		 */
		void setVersion(const HttpVersion& version);
		/**
		 * @brief 获取 HTTP 头容器.
		 */
		HttpHeader& headers();
		/**
		 * @brief 获取 HTTP 头容器（常量）.
		 */
		const HttpHeader& headers() const;
		/**
		 * @brief 设置整个 HTTP 头容器.
		 */
		void setHeaders(const HttpHeader& headers);
		/**
		 * @brief 获取消息体.
		 */
		const std::string& body() const;
		/**
		 * @brief 设置消息体.
		 */
		void setBody(const std::string& body);
		/**
		 * @brief 将 HTTP 消息序列化为字符串.
		 */
		virtual std::string toString() const = 0;
		/**
		 * @brief 判断是否为 multipart 并懒解析消息体.
		 * @return multipart 解析器，非 multipart 返回 nullptr.
		 */
		HttpMultipartParser::ptr parseMultipart() const;
		/**
		 * @brief 判断是否为 x-www-form-urlencoded 并懒解析消息体.
		 * @return urlencoded 键值列表，非 urlencoded 返回空列表.
		 */
		std::vector<std::pair<std::string, std::string>> parseFormUrlEncoded() const;

	private:
		//! http版本
		HttpVersion version_;
		//! http头部
		HttpHeader header_;
		//! 消息体
		std::string body_;
		//! multipart 解析结果缓存
		mutable HttpMultipartParser::ptr multipart_;
		//! urlencoded 解析结果缓存
		mutable std::vector<std::pair<std::string, std::string>> formUrlEncoded_;
		//! urlencoded 是否已解析
		mutable bool formUrlEncodedParsed_ = false;
	};
}
