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
}  // namespace crazy
