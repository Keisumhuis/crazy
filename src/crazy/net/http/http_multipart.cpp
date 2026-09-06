#include "crazy/net/http/http_multipart.h"

#include "crazy/utils.h"

namespace crazy {
	namespace {
		std::string parseBoundary(const std::string& contentType) {
			const std::string lower = StringUtil::ToLower(contentType);
			const size_t boundaryPos = lower.find("boundary=");
			if (boundaryPos == std::string::npos) {
				return "";
			}
			size_t valueStart = boundaryPos + 9;
			while (valueStart < contentType.size() &&
				(contentType[valueStart] == ' ' || contentType[valueStart] == '\t')) {
				++valueStart;
			}
			if (valueStart < contentType.size() && contentType[valueStart] == '"') {
				const size_t valueEnd = contentType.find('"', valueStart + 1);
				return valueEnd == std::string::npos? contentType.substr(valueStart + 1): contentType.substr(valueStart + 1, valueEnd - valueStart - 1);
			}
			const size_t valueEnd = contentType.find(';', valueStart);
			const std::string value = valueEnd == std::string::npos? contentType.substr(valueStart): contentType.substr(valueStart, valueEnd - valueStart);
			return StringUtil::Trim(value);
		}

		std::string getParameter(const std::string& value, const std::string& target) {
			const std::string lowerTarget = StringUtil::ToLower(target);
			size_t offset = 0;
			while (offset <= value.size()) {
				const size_t semicolon = value.find(';', offset);
				const size_t end = semicolon == std::string::npos ? value.size() : semicolon;
				const std::string parameter = StringUtil::Trim(value.substr(offset, end - offset));
				const size_t equal = parameter.find('=');
				if (equal != std::string::npos) {
					const std::string key = StringUtil::Trim(parameter.substr(0, equal));
					if (StringUtil::ToLower(key) == lowerTarget) {
						std::string parameterValue = StringUtil::Trim(parameter.substr(equal + 1));
						if (!parameterValue.empty() && parameterValue.front() == '"') {
							const size_t quoteEnd = parameterValue.find('"', 1);
							parameterValue = quoteEnd == std::string::npos? parameterValue.substr(1): parameterValue.substr(1, quoteEnd - 1);
						}
						return parameterValue;
					}
				}
				if (semicolon == std::string::npos) {
					break;
				}
				offset = semicolon + 1;
			}
			return "";
		}
		HttpMultipartParser* parserFrom(multipart_parser* parser) {
			return static_cast<HttpMultipartParser*>(multipart_parser_get_data(parser));
		}
	}  // namespace
	std::string HttpMultipartPart::name() const {
		const auto disposition = headers.get("Content-Disposition");
		return disposition ? getParameter(*disposition, "name") : "";
	}
	std::string HttpMultipartPart::filename() const {
		const auto disposition = headers.get("Content-Disposition");
		return disposition ? getParameter(*disposition, "filename") : "";
	}
	std::string HttpMultipartPart::contentType() const {
		const auto contentType = headers.get("Content-Type");
		return contentType ? *contentType : "";
	}
	std::string HttpMultipartParser::getBoundary(const std::string& contentType) {
		return parseBoundary(contentType);
	}
	HttpMultipartParser::HttpMultipartParser(const std::string& contentType) {
		const std::string boundary = getBoundary(contentType);
		if (boundary.empty()) {
			error_ = true;
			return;
		}
		settings_.on_part_data_begin = &HttpMultipartParser::onPartDataBegin;
		settings_.on_header_field = &HttpMultipartParser::onHeaderField;
		settings_.on_header_value = &HttpMultipartParser::onHeaderValue;
		settings_.on_headers_complete = &HttpMultipartParser::onHeadersComplete;
		settings_.on_part_data = &HttpMultipartParser::onPartData;
		settings_.on_part_data_end = &HttpMultipartParser::onPartDataEnd;
		settings_.on_body_end = &HttpMultipartParser::onBodyEnd;
		const std::string boundaryMarker = "--" + boundary;
		parser_ = multipart_parser_init(boundaryMarker.c_str(), &settings_);
		if (parser_ == nullptr) {
			error_ = true;
			return;
		}
		multipart_parser_set_data(parser_, this);
	}
	HttpMultipartParser::~HttpMultipartParser() {
		if (parser_ != nullptr) {
			multipart_parser_free(parser_);
			parser_ = nullptr;
		}
	}
	size_t HttpMultipartParser::execute(const char* data, size_t length) {
		if (parser_ == nullptr || data == nullptr || length == 0 || finished_ || error_) {
			return 0;
		}
		const size_t consumed = multipart_parser_execute(parser_, data, length);
		if (consumed != length && !finished_) {
			error_ = true;
		}
		return consumed;
	}
	size_t HttpMultipartParser::execute(const std::string& data) {
		return execute(data.data(), data.size());
	}
	HttpMultipartParser::ptr HttpMultipartParser::parse(const std::string& contentType, const std::string& data) {
		auto parser = std::make_shared<HttpMultipartParser>(contentType);
		parser->execute(data);
		return parser;
	}
	bool HttpMultipartParser::isFinished() const {
		return finished_;
	}
	bool HttpMultipartParser::hasError() const {
		return error_;
	}
	const std::vector<HttpMultipartPart>& HttpMultipartParser::parts() const {
		return parts_;
	}
	int32_t HttpMultipartParser::onPartDataBegin(multipart_parser* parser) {
		HttpMultipartParser* self = parserFrom(parser);
		try {
			self->parts_.emplace_back();
			self->headerField_.clear();
			self->headerValue_.clear();
			self->headerValueSeen_ = false;
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpMultipartParser::onHeaderField(multipart_parser* parser, const char* data, size_t length) {
		HttpMultipartParser* self = parserFrom(parser);
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
	int32_t HttpMultipartParser::onHeaderValue(multipart_parser* parser, const char* data, size_t length) {
		HttpMultipartParser* self = parserFrom(parser);
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
	int32_t HttpMultipartParser::onHeadersComplete(multipart_parser* parser) {
		HttpMultipartParser* self = parserFrom(parser);
		try {
			self->commitHeader();
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpMultipartParser::onPartData(multipart_parser* parser, const char* data, size_t length) {
		HttpMultipartParser* self = parserFrom(parser);
		try {
			if (!self->parts_.empty()) {
				self->parts_.back().body.append(data, length);
			}
			return 0;
		}
		catch (...) {
			self->error_ = true;
			return 1;
		}
	}
	int32_t HttpMultipartParser::onPartDataEnd(multipart_parser*) {
		return 0;
	}
	int32_t HttpMultipartParser::onBodyEnd(multipart_parser* parser) {
		HttpMultipartParser* self = parserFrom(parser);
		self->finished_ = true;
		return 0;
	}
	void HttpMultipartParser::commitHeader() {
		if (!headerField_.empty() && !parts_.empty()) {
			parts_.back().headers.insert(headerField_, headerValue_);
			headerField_.clear();
			headerValue_.clear();
			headerValueSeen_ = false;
		}
	}
}  // namespace crazy
