#include "crazy/net/http/http11_common.h"

namespace crazy {
	std::string httpMethodToString(HttpMethod method) {
		return http_method_str(static_cast<enum http_method>(method));
	}
	HttpMethod httpMethodFromString(const std::string& method) {
		for (int32_t i = 0; i < http_method::HTTP_SOURCE + 1; ++i) {
			const char* str = http_method_str(static_cast<enum http_method>(i));
			if (str != nullptr && method == str) {
				return static_cast<HttpMethod>(i);
			}
		}
		return HttpMethod::GET;
	}
	std::string httpStatusToString(HttpStatus status) {
		return http_status_str(static_cast<enum http_status>(status));
	}
	HttpStatus httpStatusFromString(const std::string& status) {
		for (int32_t i = 100; i < http_status::HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED + 1; ++i) {
			const char* str = http_status_str(static_cast<enum http_status>(i));
			if (str != nullptr && status == str) {
				return static_cast<HttpStatus>(i);
			}
		}
		return HttpStatus::OK;
	}
	std::string httpChunkSizeToHex(std::size_t size) {
		static const char digits[] = "0123456789abcdef";
		std::string text;
		do {
			text += digits[size & 0xF];
			size >>= 4;
		} while (size != 0);
		return std::string(text.rbegin(), text.rend());
	}
	std::string httpEncodeChunk(const std::string& data) {
		if (data.empty()) {
			// 空数据不产生数据块（0 长度是终止块语义，由调用方单独写出）
			return std::string();
		}
		std::string result = httpChunkSizeToHex(data.size());
		result += "\r\n";
		result += data;
		result += "\r\n";
		return result;
	}
}  // namespace crazy
