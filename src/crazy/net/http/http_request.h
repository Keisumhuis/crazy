/**
 * @file http_request.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 请求消息及解析器.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "crazy/net/http/http11_common.h"
#include "crazy/net/http/http_message_base.h"
#include "crazy/uri.h"

namespace crazy {
	/**
	 * @brief HTTP 请求消息.
	 */
	class HttpRequest final : public HttpMessageBase {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpRequest>;
		/**
		 * @brief 生成客户端 WebSocket 握手 key.
		 */
		static std::string generateWebSocketKey();
		/**
		 * @brief 创建客户端 WebSocket 升级请求.
		 */
		static HttpRequest::ptr createWebSocketRequest(const Uri& uri);
		/**
		 * @brief 使用指定 key 创建客户端 WebSocket 升级请求.
		 */
		static HttpRequest::ptr createWebSocketRequest(const Uri& uri, const std::string& key);
		/**
		 * @brief 使用请求 URI 构造请求.
		 */
		explicit HttpRequest(const std::string& uri);
		/**
		 * @brief 使用请求 URI 构造请求.
		 */
		explicit HttpRequest(const Uri& uri);
		/**
		 * @brief 获取请求方法.
		 */
		HttpMethod method() const;
		/**
		 * @brief 设置请求方法.
		 */
		void setMethod(HttpMethod method);
		/**
		 * @brief 获取请求 URI.
		 */
		const Uri& uri() const;
		/**
		 * @brief 设置请求 URI.
		 */
		void setUri(const Uri& uri);
		/**
		 * @brief 设置请求 URI.
		 */
		void setUri(const std::string& uri);
		/**
		 * @brief 判断是否为 WebSocket 握手请求.
		 */
		bool isWebSocket() const;
		/**
		 * @brief 判断是否为合法的 WebSocket 握手请求.
		 */
		bool isWebSocketHandshake() const;
		/**
		 * @brief 将 HTTP 请求序列化为字符串.
		 */
		std::string toString() const override;

	private:
		//! http请求方法
		HttpMethod method_ = HttpMethod::GET;
		//! http请求uri
		Uri uri_;
	};

	/**
	 * @brief HTTP 请求解析器.
	 */
	class HttpRequestParser {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpRequestParser>;
		/**
		 * @brief 构造函数.
		 */
		HttpRequestParser();
		/**
		 * @brief 增量解析一段 HTTP 请求数据.
		 */
		size_t execute(const char* data, size_t length);
		/**
		 * @brief 增量解析一段 HTTP 请求数据.
		 */
		size_t execute(const std::string& data);
		/**
		 * @brief 是否解析完成.
		 */
		bool isFinished() const;
		/**
		 * @brief 是否解析出错.
		 */
		bool hasError() const;
		/**
		 * @brief 获取解析得到的请求对象.
		 */
		HttpRequest::ptr getRequest() const;

	private:
		/**
		 * @brief http_parser 消息开始回调.
		 */
		static int32_t onMessageBegin(http_parser* parser);
		/**
		 * @brief http_parser 请求目标回调.
		 */
		static int32_t onUrl(http_parser* parser, const char* data, size_t length);
		/**
		 * @brief http_parser 头字段回调.
		 */
		static int32_t onHeaderField(http_parser* parser, const char* data, size_t length);
		/**
		 * @brief http_parser 头值回调.
		 */
		static int32_t onHeaderValue(http_parser* parser, const char* data, size_t length);
		/**
		 * @brief http_parser 头解析完成回调.
		 */
		static int32_t onHeadersComplete(http_parser* parser);
		/**
		 * @brief http_parser 请求体回调.
		 */
		static int32_t onBody(http_parser* parser, const char* data, size_t length);
		/**
		 * @brief http_parser 消息结束回调.
		 */
		static int32_t onMessageComplete(http_parser* parser);
		/**
		 * @brief 提交当前头字段.
		 */
		void commitHeader();

	private:
		//! http_parser 解析器
		http_parser parser_;
		//! http_parser 回调设置
		http_parser_settings settings_;
		//! 解析结果
		HttpRequest::ptr request_;
		//! 请求目标累积数据
		std::string url_;
		//! 当前头字段
		std::string headerField_;
		//! 当前头值
		std::string headerValue_;
		//! 请求体累积数据
		std::string body_;
		//! 当前是否已进入头值状态
		bool headerValueSeen_ = false;
		//! 是否解析完成
		bool finished_ = false;
		//! 是否解析出错
		bool error_ = false;
	};
}
