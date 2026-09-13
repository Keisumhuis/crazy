/**
 * @file http_response.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 响应消息及解析器.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "crazy/net/http/http11_common.h"
#include "crazy/net/http/http_message_base.h"

namespace crazy {
	/**
	 * @brief HTTP 响应消息.
	 */
	class HttpResponse final
		: public HttpMessageBase
		, public std::enable_shared_from_this<HttpResponse> {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpResponse>;
		//! 响应发送回调类型
		using SendCallback = std::function<void(HttpResponse&)>;
		/**
		 * @brief 根据客户端 key 计算 Sec-WebSocket-Accept.
		 */
		static std::string computeWebSocketAccept(const std::string& key);
		/**
		 * @brief 校验客户端 WebSocket 握手响应.
		 */
		bool verifyWebSocketAccept(const std::string& key) const;
		/**
		 * @brief 获取响应状态码.
		 */
		HttpStatus status() const;
		/**
		 * @brief 设置响应状态码.
		 */
		void setStatus(HttpStatus status);
		/**
		 * @brief 获取响应原因短语.
		 */
		const std::string& reasonPhrase() const;
		/**
		 * @brief 设置响应原因短语.
		 */
		void setReasonPhrase(const std::string& reasonPhrase);
		/**
		 * @brief 判断是否为 WebSocket 握手响应.
		 */
		bool isWebSocket() const;
		/**
		 * @brief 判断是否为合法的 WebSocket 握手响应.
		 */
		bool isWebSocketHandshake() const;
		/**
		 * @brief 将 HTTP 响应序列化为字符串.
		 */
		std::string toString() const override;
		/**
		 * @brief 延迟发送 HTTP 响应.
		 */
		ptr defer();
		/**
		 * @brief 发送 HTTP 响应.
		 */
		void send();
		/**
		 * @brief 判断是否延迟发送 HTTP 响应.
		 */
		bool isDeferred() const;
		/**
		 * @brief 判断是否已经发送 HTTP 响应.
		 */
		bool isSent() const;
		/**
		 * @brief 注册 HTTP 响应发送回调.
		 */
		void setSendCallback(SendCallback callback);

	private:
		//! 响应状态码
		HttpStatus status_ = HttpStatus::OK;
		//! 响应原因短语
		std::string reasonPhrase_;
		//! 是否延迟发送
		std::atomic<bool> deferred_ = false;
		//! 是否已经发送
		std::atomic<bool> sent_ = false;
		//! 响应发送回调
		SendCallback sendCallback_;
	};

	/**
	 * @brief HTTP 响应解析器.
	 */
	class HttpResponseParser {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpResponseParser>;
		/**
		 * @brief 构造函数.
		 */
		HttpResponseParser();
		/**
		 * @brief 增量解析一段 HTTP 响应数据.
		 */
		size_t execute(const char* data, size_t length);
		/**
		 * @brief 增量解析一段 HTTP 响应数据.
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
		 * @brief 获取解析得到的响应对象.
		 */
		HttpResponse::ptr getResponse() const;

	private:
		/**
		 * @brief http_parser 消息开始回调.
		 */
		static int32_t onMessageBegin(http_parser* parser);
		/**
		 * @brief http_parser 响应原因短语回调.
		 */
		static int32_t onStatus(http_parser* parser, const char* data, size_t length);
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
		 * @brief http_parser 响应体回调.
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
		HttpResponse::ptr response_;
		//! 当前头字段
		std::string headerField_;
		//! 当前头值
		std::string headerValue_;
		//! 响应体累积数据
		std::string body_;
		//! 响应原因短语累积数据
		std::string reasonPhrase_;
		//! 当前是否已进入头值状态
		bool headerValueSeen_ = false;
		//! 是否解析完成
		bool finished_ = false;
		//! 是否解析出错
		bool error_ = false;
	};
}
