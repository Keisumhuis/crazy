#include "crazy/net/http/http_request.h"

#include <random>
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
		std::string result = httpMethodToString(method());
		result += ' ';
		result += uri().toString();
		result += ' ';
		result += version().toString();
		result += "\r\n";

		if (!body().empty() && !headers().contains("Content-Length")) {
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
		result += "\r\n";
		result += body();
		return result;
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
		if (data == nullptr || length == 0 || finished_ || error_) {
			return 0;
		}
		const size_t consumed = http_parser_execute(&parser_, &settings_, data, length);
		if (HTTP_PARSER_ERRNO(&parser_) != HPE_OK) {
			error_ = true;
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
	HttpRequest::ptr HttpRequestParser::getRequest() const {
		return request_;
	}
	int32_t HttpRequestParser::onMessageBegin(http_parser*) {
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
			self->finished_ = true;
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	void HttpRequestParser::commitHeader() {
		if (!headerField_.empty()) {
			request_->headers().insert(headerField_, headerValue_);
			headerField_.clear();
			headerValue_.clear();
		}
		headerValueSeen_ = false;
	}
}  // namespace crazy
