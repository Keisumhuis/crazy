#include "crazy/net/http/http_response.h"

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
	size_t HttpResponseParser::execute(const std::string& data) {
		return execute(data.data(), data.size());
	}
	bool HttpResponseParser::isFinished() const {
		return finished_;
	}
	bool HttpResponseParser::hasError() const {
		return error_;
	}
	HttpResponse::ptr HttpResponseParser::getResponse() const {
		return response_;
	}
	int32_t HttpResponseParser::onMessageBegin(http_parser*) {
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
			self->commitHeader();
			self->response_->setBody(std::move(self->body_));
			self->response_->parseMultipart();
			self->response_->parseFormUrlEncoded();
			self->headerValueSeen_ = false;
			self->finished_ = true;
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	void HttpResponseParser::commitHeader() {
		if (!headerField_.empty()) {
			response_->headers().insert(headerField_, headerValue_);
			headerField_.clear();
			headerValue_.clear();
		}
		headerValueSeen_ = false;
	}
}  // namespace crazy
