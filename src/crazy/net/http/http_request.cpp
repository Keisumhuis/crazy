#include "crazy/net/http/http_request.h"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <utility>

#include "crazy/encryption/base64.h"
#include "crazy/utils.h"

namespace crazy {
	namespace {
		HttpRequestParser* parserFrom(http_parser* parser) {
			return static_cast<HttpRequestParser*>(parser->data);
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

	HttpRequest::HttpRequest(const std::string& uri)
		: uri_(uri) {}

	HttpRequest::HttpRequest(const Uri& uri)
		: uri_(uri) {}

	std::string HttpRequest::generateWebSocketKey() {
		std::random_device rd;
		std::mt19937 generator(rd());
		std::uniform_int_distribution<int> distribution(0, 255);

		std::string key(16, '\0');
		for (char& byte : key) {
			byte = static_cast<char>(distribution(generator));
		}
		return Base64::encryption(key);
	}
	HttpRequest::ptr HttpRequest::createWebSocketRequest(const Uri& uri) {
		return createWebSocketRequest(uri, generateWebSocketKey());
	}
	HttpRequest::ptr HttpRequest::createWebSocketRequest(const Uri& uri, const std::string& key) {
		HttpRequest::ptr request = std::make_shared<HttpRequest>(uri);
		request->setVersion(HttpVersion(1, 1));
		request->setMethod(HttpMethod::GET);
		request->headers().insert("Connection", "Upgrade");
		request->headers().insert("Upgrade", "websocket");
		request->headers().insert("Sec-WebSocket-Key", key);
		request->headers().insert("Sec-WebSocket-Version", "13");
		return request;
	}
	HttpMethod HttpRequest::method() const {
		return method_;
	}
	void HttpRequest::setMethod(HttpMethod method) {
		method_ = method;
	}
	const Uri& HttpRequest::uri() const {
		return uri_;
	}
	void HttpRequest::setUri(const Uri& uri) {
		uri_ = uri;
	}
	void HttpRequest::setUri(const std::string& uri) {
		uri_ = Uri(uri);
	}
	std::string HttpRequest::toString() const {
		const bool chunked = chunkedTransferEncoding_;
		if (chunked && headers().contains("Content-Length")) {
			throw std::logic_error("chunked request must not set Content-Length");
		}
		if (chunked &&
			(version().getMajor() < 1 || (version().getMajor() == 1 && version().getMinor() < 1))) {
			// 分块传输编码是 HTTP/1.1 引入的：HTTP/1.0 请求体只能用 Content-Length 界定，
			// 这里直接报错，避免把对方无法解析的报文发出去
			throw std::logic_error("chunked request requires HTTP/1.1");
		}
		std::string result = httpMethodToString(method());
		result += ' ';
		result += uri().toString();
		result += ' ';
		result += version().toString();
		result += "\r\n";

		if (!chunked && !body().empty() && !headers().contains("Content-Length")) {
			result += "Content-Length: ";
			result += std::to_string(body().size());
			result += "\r\n";
		}

		for (const auto& header : headers()) {
			result += header.first;
			result += ": ";
			result += header.second;
			result += "\r\n";
		}
		if (chunked && !headers().contains("Transfer-Encoding")) {
			result += "Transfer-Encoding: chunked\r\n";
		}
		result += "\r\n";
		if (!chunked) {
			result += body();
			return result;
		}

		// 分块上传：按 chunkSize 切分请求体，帧格式见 RFC 7230 第 4.1 节
		const std::size_t chunkSize = chunkSize_ == 0 ? 8192 : chunkSize_;
		std::size_t offset = 0;
		while (offset < body().size()) {
			const std::size_t length = std::min(chunkSize, body().size() - offset);
			result += httpChunkSizeToHex(length);
			result += "\r\n";
			result.append(body(), offset, length);
			result += "\r\n";
			offset += length;
		}
		result += "0\r\n\r\n";
		return result;
	}
	void HttpRequest::setChunkedTransferEncoding(bool enabled) {
		chunkedTransferEncoding_ = enabled;
	}
	bool HttpRequest::isChunkedTransferEncoding() const {
		return chunkedTransferEncoding_;
	}
	void HttpRequest::setChunkSize(std::size_t chunkSize) {
		chunkSize_ = chunkSize == 0 ? 8192 : chunkSize;
	}
	std::size_t HttpRequest::chunkSize() const {
		return chunkSize_;
	}
	bool HttpRequest::isWebSocket() const {
		if (method_ != HttpMethod::GET) {
			return false;
		}
		return hasHeaderToken(headers(), "Upgrade", "websocket") &&
			hasHeaderToken(headers(), "Connection", "upgrade");
	}
	bool HttpRequest::isWebSocketHandshake() const {
		if (!isWebSocket()) {
			return false;
		}
		if (version().getMajor() < 1 ||
			(version().getMajor() == 1 && version().getMinor() < 1)) {
			return false;
		}
		const auto key = headers().get("Sec-WebSocket-Key");
		return key && !key->empty() &&
			hasHeaderToken(headers(), "Sec-WebSocket-Version", "13");
	}
	bool HttpRequest::shouldKeepAlive() const {
		if (version().getMajor() > 1 ||
			(version().getMajor() == 1 && version().getMinor() >= 1)) {
			return !hasHeaderToken(headers(), "Connection", "close");
		}
		return hasHeaderToken(headers(), "Connection", "keep-alive");
	}

	HttpRequestParser::HttpRequestParser()
		: request_(std::make_shared<HttpRequest>(Uri())) {
		http_parser_init(&parser_, HTTP_REQUEST);
		http_parser_settings_init(&settings_);
		parser_.data = this;
		settings_.on_message_begin = &HttpRequestParser::onMessageBegin;
		settings_.on_url = &HttpRequestParser::onUrl;
		settings_.on_header_field = &HttpRequestParser::onHeaderField;
		settings_.on_header_value = &HttpRequestParser::onHeaderValue;
		settings_.on_headers_complete = &HttpRequestParser::onHeadersComplete;
		settings_.on_body = &HttpRequestParser::onBody;
		settings_.on_message_complete = &HttpRequestParser::onMessageComplete;
	}
	size_t HttpRequestParser::execute(const char* data, size_t length) {
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
	size_t HttpRequestParser::execute(const std::string& data) {
		return execute(data.data(), data.size());
	}
	bool HttpRequestParser::isFinished() const {
		return finished_;
	}
	bool HttpRequestParser::hasError() const {
		return error_;
	}
	bool HttpRequestParser::shouldKeepAlive() const {
		return keepAlive_;
	}
	HttpRequest::ptr HttpRequestParser::getRequest() const {
		return request_;
	}
	bool HttpRequestParser::expectContinue() const {
		return expectContinue_;
	}
	int32_t HttpRequestParser::onMessageBegin(http_parser* parser) {
		HttpRequestParser* self = parserFrom(parser);
		self->request_ = std::make_shared<HttpRequest>(Uri());
		self->url_.clear();
		self->headerField_.clear();
		self->headerValue_.clear();
		self->body_.clear();
		self->headerValueSeen_ = false;
		self->headersComplete_ = false;
		self->expectContinue_ = false;
		self->keepAlive_ = false;
		return 0;
	}
	int32_t HttpRequestParser::onUrl(http_parser* parser, const char* data, size_t length) {
		HttpRequestParser* self = parserFrom(parser);
		try {
			self->url_.append(data, length);
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpRequestParser::onHeaderField(http_parser* parser, const char* data, size_t length) {
		HttpRequestParser* self = parserFrom(parser);
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
	int32_t HttpRequestParser::onHeaderValue(http_parser* parser, const char* data, size_t length) {
		HttpRequestParser* self = parserFrom(parser);
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
	int32_t HttpRequestParser::onHeadersComplete(http_parser* parser) {
		HttpRequestParser* self = parserFrom(parser);
		try {
			self->commitHeader();
			self->request_->setVersion(HttpVersion(parser->http_major, parser->http_minor));
			self->request_->setMethod(static_cast<HttpMethod>(parser->method));
			self->request_->setUri(self->url_);
			self->url_.clear();
			// 此后的头字段属于 trailer（分块消息尾部）
			self->headersComplete_ = true;
			// Expect: 100-continue 只在 HTTP/1.1 及以上版本生效
			const HttpVersion version = self->request_->version();
			if (version >= HttpVersions::HTTP_1_1 &&
				hasHeaderToken(self->request_->headers(), "Expect", "100-continue")) {
				self->expectContinue_ = true;
			}
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpRequestParser::onBody(http_parser* parser, const char* data, size_t length) {
		HttpRequestParser* self = parserFrom(parser);
		try {
			self->body_.append(data, length);
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpRequestParser::onMessageComplete(http_parser* parser) {
		HttpRequestParser* self = parserFrom(parser);
		try {
			self->commitHeader();
			self->request_->setBody(std::move(self->body_));
			self->request_->parseMultipart();
			self->request_->parseFormUrlEncoded();
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
	void HttpRequestParser::commitHeader() {
		if (!headerField_.empty()) {
			// 头段结束之后收到的字段属于 trailer，不得混进 headers
			HttpHeader& target = headersComplete_ ? request_->trailers() : request_->headers();
			target.insert(headerField_, headerValue_);
			headerField_.clear();
			headerValue_.clear();
		}
		headerValueSeen_ = false;
	}
}  // namespace crazy
