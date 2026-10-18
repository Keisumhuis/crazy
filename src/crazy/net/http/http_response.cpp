#include "crazy/net/http/http_response.h"

#include <stdexcept>
#include <utility>

#include "crazy/encryption/base64.h"
#include "crazy/net/http/websocket_parser.h"
#include "crazy/uuid.h"
#include "crazy/utils.h"

namespace crazy {
	namespace {
		HttpResponseParser* parserFrom(http_parser* parser) {
			return static_cast<HttpResponseParser*>(parser->data);
		}
		bool isContentLength(const std::string& key) {
			return StringUtil::ToLower(key) == "content-length";
		}
		bool hasHeaderToken(const HttpHeader& headers, const std::string& key, const std::string& token) {
			const auto value = headers.get(key);
			if (!value) {
				return false;
			}
			const std::string lowerValue = StringUtil::ToLower(StringUtil::Trim(*value));
			const std::string lowerToken = StringUtil::ToLower(token);
			size_t offset = 0;
			while (offset <= lowerValue.size()) {
				const size_t comma = lowerValue.find(',', offset);
				const size_t end = comma == std::string::npos ? lowerValue.size() : comma;
				if (StringUtil::Trim(lowerValue.substr(offset, end - offset)) == lowerToken) {
					return true;
				}
				if (comma == std::string::npos) {
					break;
				}
				offset = comma + 1;
			}
			return false;
		}
	}  // namespace

	HttpStatus HttpResponse::status() const {
		return status_;
	}
	std::string HttpResponse::computeWebSocketAccept(const std::string& key) {
		uuids::detail::sha1 hasher;
		hasher.process_bytes(key.data(), key.size());
		hasher.process_bytes(WEBSOCKET_UUID, sizeof(WEBSOCKET_UUID) - 1);

		uuids::detail::sha1::digest8_t digest;
		hasher.get_digest_bytes(digest);
		return Base64::encryption(std::string(
			reinterpret_cast<const char*>(digest), sizeof(digest)));
	}
	bool HttpResponse::verifyWebSocketAccept(const std::string& key) const {
		if (!isWebSocket()) {
			return false;
		}
		const auto accept = headers().get("Sec-WebSocket-Accept");
		return accept && *accept == computeWebSocketAccept(key);
	}
	void HttpResponse::setStatus(HttpStatus status) {
		status_ = status;
	}
	const std::string& HttpResponse::reasonPhrase() const {
		return reasonPhrase_;
	}
	void HttpResponse::setReasonPhrase(const std::string& reasonPhrase) {
		reasonPhrase_ = reasonPhrase;
	}
	HttpResponse::ptr HttpResponse::defer() {
		if (!sent_.load()) {
			deferred_.store(true);
		}
		return shared_from_this();
	}
	void HttpResponse::send() {
		bool expected = false;
		if (!sent_.compare_exchange_strong(expected, true)) {
			// 非分块响应保持“只发送一次”语义；分块模式下允许多次写出
			if (!chunked_.load()) {
				return;
			}
		}
		if (sendCallback_) {
			sendCallback_(*this);
		}
	}
	bool HttpResponse::isDeferred() const {
		return deferred_.load();
	}
	bool HttpResponse::isSent() const {
		return sent_.load();
	}
	void HttpResponse::setSendCallback(SendCallback callback) {
		sendCallback_ = std::move(callback);
	}
	void HttpResponse::beginChunked(ChunkSource source) {
		if (chunked_.load()) {
			return;
		}
		if (!source) {
			throw std::logic_error("chunked response requires a data source");
		}
		chunkSource_ = std::move(source);
		beginChunked();
	}
	void HttpResponse::beginChunked() {
		if (chunked_.load()) {
			return;
		}
		if (sent_.load()) {
			throw std::logic_error("chunked response can not start after send");
		}
		if (headers().contains("Content-Length")) {
			throw std::logic_error("chunked response must not set Content-Length");
		}
		headers().erase("Transfer-Encoding");
		chunked_.store(true);
		chunkedFinished_.store(false);
		send();
	}
	void HttpResponse::writeChunk(const std::string& data) {
		if (!chunked_.load()) {
			throw std::logic_error("writeChunk requires beginChunked");
		}
		if (chunkedFinished_.load()) {
			throw std::logic_error("writeChunk after endChunked");
		}
		if (data.empty()) {
			// 0 长度块是终止块语义，数据段忽略
			return;
		}
		if (chunkCallback_) {
			chunkCallback_(*this, data, false);
		}
	}
	void HttpResponse::endChunked() {
		if (!chunked_.load()) {
			throw std::logic_error("endChunked requires beginChunked");
		}
		bool expected = false;
		if (!chunkedFinished_.compare_exchange_strong(expected, true)) {
			return;
		}
		if (chunkCallback_) {
			chunkCallback_(*this, std::string(), true);
		}
	}
	bool HttpResponse::isChunked() const {
		return chunked_.load();
	}
	bool HttpResponse::isChunkedFinished() const {
		return chunkedFinished_.load();
	}
	bool HttpResponse::isStreaming() const {
		return chunked_.load() && static_cast<bool>(chunkSource_);
	}
	const HttpResponse::ChunkSource& HttpResponse::chunkSource() const {
		return chunkSource_;
	}
	void HttpResponse::setChunkedFraming(bool enabled) {
		chunkedFraming_.store(enabled);
	}
	bool HttpResponse::isChunkedFraming() const {
		return chunkedFraming_.load();
	}
	void HttpResponse::addTrailer(const std::string& key, const std::string& value) {
		if (chunked_.load()) {
			// 头段已经写出去才追加 trailer，无法再声明 Trailer 头
			throw std::logic_error("trailer must be added before beginChunked");
		}
		trailers().insert(key, value);
	}
	void HttpResponse::setChunkCallback(ChunkCallback callback) {
		chunkCallback_ = std::move(callback);
	}
	std::string HttpResponse::toString() const {
		std::string result = version().toString();
		result += ' ';
		result += std::to_string(static_cast<int>(status_));
		result += ' ';
		if (reasonPhrase_.empty()) {
			result += httpStatusToString(status_);
		}
		else {
			result += reasonPhrase_;
		}
		result += "\r\n";

		const bool chunked = chunked_.load();
		// HTTP/1.0 降级时分块响应不写 Transfer-Encoding / Content-Length，由关闭连接界定响应体
		const bool chunkedFraming = chunked && chunkedFraming_.load();
		const int statusCode = static_cast<int>(status_);
		if (!chunked &&
			!headers().contains("Content-Length") &&
			(statusCode < 100 || statusCode >= 200) &&
			status_ != HttpStatus::NO_CONTENT &&
			status_ != HttpStatus::NOT_MODIFIED) {
			result += "Content-Length: ";
			result += std::to_string(body().size());
			result += "\r\n";
		}

		for (const auto& header : headers()) {
			if (chunked && isContentLength(header.first)) {
				continue;
			}
			result += header.first;
			result += ": ";
			result += header.second;
			result += "\r\n";
		}
		if (chunkedFraming) {
			if (!trailers().empty() && !headers().contains("Trailer")) {
				result += "Trailer: ";
				bool first = true;
				for (const auto& trailer : trailers()) {
					if (!first) {
						result += ", ";
					}
					result += trailer.first;
					first = false;
				}
				result += "\r\n";
			}
			result += "Transfer-Encoding: chunked\r\n";
		}
		result += "\r\n";
		if (!chunked) {
			result += body();
		}
		return result;
	}
	bool HttpResponse::isWebSocket() const {
		if (status_ != HttpStatus::SWITCHING_PROTOCOLS) {
			return false;
		}
		return hasHeaderToken(headers(), "Upgrade", "websocket") &&
			hasHeaderToken(headers(), "Connection", "upgrade");
	}
	bool HttpResponse::isWebSocketHandshake() const {
		if (!isWebSocket()) {
			return false;
		}
		const auto accept = headers().get("Sec-WebSocket-Accept");
		return accept && !accept->empty();
	}
	bool HttpResponse::shouldKeepAlive() const {
		if (version().getMajor() > 1 ||
			(version().getMajor() == 1 && version().getMinor() >= 1)) {
			return !hasHeaderToken(headers(), "Connection", "close");
		}
		return hasHeaderToken(headers(), "Connection", "keep-alive");
	}

	HttpResponseParser::HttpResponseParser()
		: response_(std::make_shared<HttpResponse>()) {
		http_parser_init(&parser_, HTTP_RESPONSE);
		http_parser_settings_init(&settings_);
		parser_.data = this;
		settings_.on_message_begin = &HttpResponseParser::onMessageBegin;
		settings_.on_status = &HttpResponseParser::onStatus;
		settings_.on_header_field = &HttpResponseParser::onHeaderField;
		settings_.on_header_value = &HttpResponseParser::onHeaderValue;
		settings_.on_headers_complete = &HttpResponseParser::onHeadersComplete;
		settings_.on_body = &HttpResponseParser::onBody;
		settings_.on_message_complete = &HttpResponseParser::onMessageComplete;
	}
	size_t HttpResponseParser::execute(const char* data, size_t length) {
		if (data == nullptr || length == 0 || error_) {
			return 0;
		}
		if (finished_) {
			if (!keepAlive_) {
				return 0;
			}
			http_parser_pause(&parser_, 0);
			finished_ = false;
		}
		const size_t consumed = http_parser_execute(&parser_, &settings_, data, length);
		if (HTTP_PARSER_ERRNO(&parser_) != HPE_OK) {
			if (HTTP_PARSER_ERRNO(&parser_) != HPE_PAUSED) {
				error_ = true;
			}
		}
		if (parser_.upgrade) {
			finished_ = true;
		}
		return consumed;
	}
	size_t HttpResponseParser::execute(const std::string& data) {
		return execute(data.data(), data.size());
	}
	bool HttpResponseParser::isFinished() const {
		return finished_;
	}
	bool HttpResponseParser::hasError() const {
		return error_;
	}
	bool HttpResponseParser::shouldKeepAlive() const {
		return keepAlive_;
	}
	HttpResponse::ptr HttpResponseParser::getResponse() const {
		return response_;
	}
	std::size_t HttpResponseParser::interimResponseCount() const {
		return interimResponseCount_;
	}
	void HttpResponseParser::setDataCallback(DataCallback callback) {
		dataCallback_ = std::move(callback);
	}
	int32_t HttpResponseParser::onMessageBegin(http_parser* parser) {
		HttpResponseParser* self = parserFrom(parser);
		self->response_ = std::make_shared<HttpResponse>();
		self->headerField_.clear();
		self->headerValue_.clear();
		self->body_.clear();
		self->reasonPhrase_.clear();
		self->headerValueSeen_ = false;
		self->headersComplete_ = false;
		self->keepAlive_ = false;
		return 0;
	}
	int32_t HttpResponseParser::onStatus(http_parser* parser, const char* data, size_t length) {
		HttpResponseParser* self = parserFrom(parser);
		try {
			self->reasonPhrase_.append(data, length);
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpResponseParser::onHeaderField(http_parser* parser, const char* data, size_t length) {
		HttpResponseParser* self = parserFrom(parser);
		try {
			if (self->headerValueSeen_) {
				self->commitHeader();
				self->headerValueSeen_ = false;
			}
			self->headerField_.append(data, length);
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpResponseParser::onHeaderValue(http_parser* parser, const char* data, size_t length) {
		HttpResponseParser* self = parserFrom(parser);
		try {
			self->headerValueSeen_ = true;
			self->headerValue_.append(data, length);
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpResponseParser::onHeadersComplete(http_parser* parser) {
		HttpResponseParser* self = parserFrom(parser);
		try {
			self->commitHeader();
			self->response_->setVersion(HttpVersion(parser->http_major, parser->http_minor));
			self->response_->setStatus(static_cast<HttpStatus>(parser->status_code));
			self->response_->setReasonPhrase(std::move(self->reasonPhrase_));
			// 此后的头字段属于 trailer（分块消息尾部）
			self->headersComplete_ = true;
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpResponseParser::onBody(http_parser* parser, const char* data, size_t length) {
		HttpResponseParser* self = parserFrom(parser);
		try {
			self->body_.append(data, length);
			if (self->dataCallback_ && length > 0) {
				self->dataCallback_(std::string(data, length));
			}
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpResponseParser::onMessageComplete(http_parser* parser) {
		HttpResponseParser* self = parserFrom(parser);
		try {
			// 临时响应（1xx 且非 101）不结束解析：丢弃后继续等待最终响应
			const int statusCode = static_cast<int>(parser->status_code);
			if (statusCode >= 100 && statusCode < 200 && statusCode != 101) {
				self->interimResponseCount_ += 1;
				self->headersComplete_ = false;
				self->headerValueSeen_ = false;
				return 0;
			}
			self->commitHeader();
			self->response_->setBody(std::move(self->body_));
			self->response_->parseMultipart();
			self->response_->parseFormUrlEncoded();
			self->headerValueSeen_ = false;
			self->keepAlive_ = http_should_keep_alive(parser) != 0;
			self->finished_ = true;
			http_parser_pause(parser, 1);
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	void HttpResponseParser::commitHeader() {
		if (!headerField_.empty()) {
			// 头段结束之后收到的字段属于 trailer，不得混进 headers
			HttpHeader& target = headersComplete_ ? response_->trailers() : response_->headers();
			target.insert(headerField_, headerValue_);
			headerField_.clear();
			headerValue_.clear();
		}
		headerValueSeen_ = false;
	}
}  // namespace crazy
