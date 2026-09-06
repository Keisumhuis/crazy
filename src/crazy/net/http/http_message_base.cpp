#include "crazy/net/http/http_message_base.h"

#include <cctype>

namespace crazy {
	namespace {
		int hexValue(char c) {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		}

		std::string percentDecode(const std::string& input) {
			std::string result;
			result.reserve(input.size());
			for (size_t i = 0; i < input.size(); ++i) {
				if (input[i] == '+') {
					result += ' ';
					continue;
				}
				if (input[i] == '%' && i + 2 < input.size()) {
					const int32_t high = hexValue(input[i + 1]);
					const int32_t low = hexValue(input[i + 2]);
					if (high >= 0 && low >= 0) {
						result += static_cast<char>((high << 4) | low);
						i += 2;
						continue;
					}
				}
				result += input[i];
			}
			return result;
		}
	}  // namespace
	HttpVersion HttpMessageBase::version() const {
		return version_;
	}
	void HttpMessageBase::setVersion(const HttpVersion& version) {
		version_ = version;
	}
	HttpHeader& HttpMessageBase::headers() {
		return header_;
	}
	const HttpHeader& HttpMessageBase::headers() const {
		return header_;
	}
	void HttpMessageBase::setHeaders(const HttpHeader& headers) {
		header_ = headers;
		multipart_.reset();
		formUrlEncoded_.clear();
		formUrlEncodedParsed_ = false;
	}
	const std::string& HttpMessageBase::body() const {
		return body_;
	}
	void HttpMessageBase::setBody(const std::string& body) {
		body_ = body;
		multipart_.reset();
		formUrlEncoded_.clear();
		formUrlEncodedParsed_ = false;
	}
	HttpMultipartParser::ptr HttpMessageBase::parseMultipart() const {
		if (multipart_) {
			return multipart_;
		}
		const auto contentType = header_.get("Content-Type");
		if (!contentType) {
			return nullptr;
		}
		const std::string contentTypeValue = StringUtil::ToLower(StringUtil::Trim(*contentType));
		if (contentTypeValue.rfind("multipart/", 0) != 0) {
			return nullptr;
		}
		multipart_ = HttpMultipartParser::parse(*contentType, body_);
		return multipart_;
	}
	std::vector<std::pair<std::string, std::string>> HttpMessageBase::parseFormUrlEncoded() const {
		if (formUrlEncodedParsed_) {
			return formUrlEncoded_;
		}

		const auto contentType = header_.get("Content-Type");
		if (contentType) {
			const std::string contentTypeValue = StringUtil::ToLower(StringUtil::Trim(*contentType));
			if (contentTypeValue.rfind("application/x-www-form-urlencoded", 0) == 0) {
				size_t offset = 0;
				while (offset <= body_.size()) {
					const size_t amp = body_.find('&', offset);
					const size_t end = amp == std::string::npos ? body_.size() : amp;
					const std::string item = body_.substr(offset, end - offset);
					const size_t equal = item.find('=');
					if (equal == std::string::npos) {
						if (!item.empty()) {
							formUrlEncoded_.emplace_back(percentDecode(item), "");
						}
					}
					else {
						formUrlEncoded_.emplace_back(
							percentDecode(item.substr(0, equal)),
							percentDecode(item.substr(equal + 1)));
					}
					if (amp == std::string::npos) {
						break;
					}
					offset = amp + 1;
				}
			}
		}
		formUrlEncodedParsed_ = true;
		return formUrlEncoded_;
	}
}  // namespace crazy
