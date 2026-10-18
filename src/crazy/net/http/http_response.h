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
		//! 分块数据发送回调类型（data 为本段数据，finished 为 true 时为终止块）
		using ChunkCallback = std::function<void(HttpResponse&, const std::string& data, bool finished)>;
		//! 分块数据源类型：向 buffer 追加数据并返回追加的字节数，返回 0 表示数据结束
		using ChunkSource = std::function<size_t(std::string& buffer)>;
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
		 * @brief 判断当前响应完成后连接是否可以复用.
		 */
		bool shouldKeepAlive() const;
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
		/**
		 * @brief 开始分块响应（发送状态行与响应头）.
		 * @note 分块模式下业务显式设置了 Content-Length 时直接报错，不静默移除该头.
		 */
		void beginChunked();
		/**
		 * @brief 以数据源方式开始分块响应（适用于大文件等无法一次放入内存的响应）.
		 * @param source 数据源：会话在可写时按需拉取一段数据，返回 0 表示数据结束.
		 * @note 数据源在会话线程调用，抛异常时会话强制关闭连接（客户端看到的是截断的响应）；
		 * 与 `writeChunk()` 互斥，使用数据源时不要手工再写数据段.
		 */
		void beginChunked(ChunkSource source);
		/**
		 * @brief 追加一段分块数据（须先调用 beginChunked）.
		 * @note 分块模式下可多次调用；空数据段会被忽略.
		 */
		void writeChunk(const std::string& data);
		/**
		 * @brief 结束分块响应（发送终止块 0\r\n\r\n）.
		 */
		void endChunked();
		/**
		 * @brief 判断是否为分块响应.
		 */
		bool isChunked() const;
		/**
		 * @brief 判断分块响应是否由数据源驱动.
		 */
		bool isStreaming() const;
		/**
		 * @brief 获取分块数据源（未设置时为空函数）.
		 */
		const ChunkSource& chunkSource() const;
		/**
		 * @brief 设置分块响应是否使用 chunked 帧格式.
		 * @note HTTP/1.0 客户端不认识分块编码，会话会把该标志置为 false：
		 * 响应不写 `Transfer-Encoding: chunked`、也不写 `Content-Length`，由关闭连接界定响应体.
		 */
		void setChunkedFraming(bool enabled);
		/**
		 * @brief 判断分块响应是否使用 chunked 帧格式.
		 */
		bool isChunkedFraming() const;
		/**
		 * @brief 追加一个 trailer 字段（须在 beginChunked 之前调用）.
		 * @note 头段会自动声明 `Trailer: <名称列表>`；beginChunked 之后调用直接抛 `std::logic_error`.
		 */
		void addTrailer(const std::string& key, const std::string& value);
		/**
		 * @brief 判断分块响应是否已经结束.
		 */
		bool isChunkedFinished() const;
		/**
		 * @brief 注册分块数据发送回调.
		 */
		void setChunkCallback(ChunkCallback callback);

	private:
		//! 响应状态码
		HttpStatus status_ = HttpStatus::OK;
		//! 响应原因短语
		std::string reasonPhrase_;
		//! 是否延迟发送
		std::atomic<bool> deferred_ = false;
		//! 是否已经发送
		std::atomic<bool> sent_ = false;
		//! 是否为分块响应
		std::atomic<bool> chunked_ = false;
		//! 分块响应是否使用 chunked 帧格式（false 表示由关闭连接界定响应体）
		std::atomic<bool> chunkedFraming_ = true;
		//! 分块响应是否已经结束
		std::atomic<bool> chunkedFinished_ = false;
		//! 分块数据源（为空表示由 writeChunk 逐段写出）
		ChunkSource chunkSource_;
		//! 响应发送回调
		SendCallback sendCallback_;
		//! 分块数据发送回调
		ChunkCallback chunkCallback_;
	};

	/**
	 * @brief HTTP 响应解析器.
	 */
	class HttpResponseParser {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpResponseParser>;
		//! 响应体数据回调类型（收到一段响应体数据即回调）
		using DataCallback = std::function<void(const std::string& data)>;
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
		 * @brief 判断当前响应完成后连接是否可以复用.
		 */
		bool shouldKeepAlive() const;
		/**
		 * @brief 获取解析得到的响应对象.
		 */
		HttpResponse::ptr getResponse() const;
		/**
		 * @brief 已跳过的临时响应（1xx，101 除外）数量.
		 * @note `100 Continue` 等临时响应不会结束解析，也不会成为 `getResponse()` 的结果.
		 */
		std::size_t interimResponseCount() const;
		/**
		 * @brief 注册响应体数据回调（数据到达即回调，不等待消息结束）.
		 */
		void setDataCallback(DataCallback callback);

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
		//! 头段是否已经结束（结束后收到的字段属于 trailer）
		bool headersComplete_ = false;
		//! 已跳过的临时响应（1xx，101 除外）数量
		std::size_t interimResponseCount_ = 0;
		//! 是否解析完成
		bool finished_ = false;
		//! 是否解析出错
		bool error_ = false;
		//! 当前响应完成后连接是否可以复用
		bool keepAlive_ = false;
		//! 响应体数据回调
		DataCallback dataCallback_;
	};
}
