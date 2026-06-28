#include "crazy/uri.h"
#include <cctype>
#include <stdexcept>
#include <sstream>

namespace crazy {

	Uri::Uri() : port_(0) {}

	Uri::Uri(const char* uri, int32_t length) 
		: Uri(std::string(uri, length)) {
	}

	Uri::Uri(const std::string& uri)
		: port_(0), originalUri_(uri) {
		parseFrom(originalUri_);
	}

	std::string Uri::getScheme() const {
		return scheme_;
	}

	void Uri::setScheme(const std::string& scheme) {
		scheme_ = scheme;
		originalUri_.clear();
	}

	std::string Uri::getHost() const {
		return host_;
	}

	void Uri::setHost(const std::string& host) {
		host_ = host;
		originalUri_.clear();
	}

	std::string Uri::getUserInfo() const {
		return userInfo_;
	}

	void Uri::setUserInfo(const std::string& userInfo) {
		userInfo_ = userInfo;
		originalUri_.clear();
	}

	std::string Uri::getUserName() const {
		size_t colon_pos = userInfo_.find(':');
		if (colon_pos != std::string::npos) {
			return userInfo_.substr(0, colon_pos);
		}
		return userInfo_;
	}

	std::string Uri::getPassword() const {
		size_t colon_pos = userInfo_.find(':');
		if (colon_pos != std::string::npos) {
			return userInfo_.substr(colon_pos + 1);
		}
		return "";
	}

	std::string Uri::getPath() const {
		return path_;
	}

	void Uri::setPath(const std::string& path) {
		path_ = path;
		if (!path_.empty()) {
			path_ = simplifyPath(path_);
		}
		originalUri_.clear();
	}

	std::string Uri::getQuery() const {
		return query_;
	}

	void Uri::setQuery(const std::string& query) {
		query_ = query;
		queryParamsDirty_ = true;
		originalUri_.clear();
	}

	std::string Uri::getFragment() const {
		return fragment_;
	}

	void Uri::setFragment(const std::string& fragment) {
		fragment_ = fragment;
		originalUri_.clear();
	}

	uint16_t Uri::getPort() const {
		return port_;
	}

	void Uri::setPort(uint16_t port) {
		port_ = port;
		originalUri_.clear();
	}

	void Uri::addQueryParam(const std::string& key, const std::string& value) {
		if (queryParamsDirty_ && !query_.empty()) {
			parseQueryParams();
		}
		queryParams_.push_back({ key, value });
		queryParamsDirty_ = false;
		buildQueryString();
		originalUri_.clear();
	}

	std::string Uri::getQueryParam(const std::string& key) const {
		if (queryParamsDirty_ && !query_.empty()) {
			const_cast<Uri*>(this)->parseQueryParams();
		}
		for (const auto& param : queryParams_) {
			if (param.first == key) {
				return param.second;
			}
		}
		return "";
	}

	std::vector<std::pair<std::string, std::string>> Uri::getQueryParams() const {
		if (queryParamsDirty_ && !query_.empty()) {
			const_cast<Uri*>(this)->parseQueryParams();
		}
		return queryParams_;
	}

	void Uri::clearQueryParams() {
		queryParams_.clear();
		query_.clear();
		queryParamsDirty_ = false;
		originalUri_.clear();
	}

	std::string Uri::toString() const {
		if (!originalUri_.empty() && scheme_.empty() && host_.empty() &&
			path_.empty() && query_.empty() && fragment_.empty()) {
			return originalUri_;
		}

		std::string result;

		if (!scheme_.empty()) {
			result += scheme_;
			result += ':';
		}

		if (!host_.empty()) {
			result += "//";

			if (!userInfo_.empty()) {
				result += userInfo_;
				result += '@';
			}

			result += host_;

			if (port_ != 0) {
				result += ':';
				result += std::to_string(port_);
			}
		}

		if (!path_.empty()) {
			if (!host_.empty() && path_[0] != '/') {
				result += '/';
			}
			result += path_;
		}

		if (!query_.empty()) {
			result += '?';
			result += query_;
		}

		if (!fragment_.empty()) {
			result += '#';
			result += fragment_;
		}

		return result;
	}

	Uri Uri::authority() const {
		Uri result;
		result.host_ = host_;
		result.userInfo_ = userInfo_;
		result.port_ = port_;
		return result;
	}

	Uri Uri::resource() const {
		Uri result;
		result.path_ = path_;
		result.query_ = query_;
		result.fragment_ = fragment_;
		return result;
	}

	bool Uri::isEmpty() const {
		return originalUri_.empty() || originalUri_ == "/";
	}

	bool Uri::isHostLoopback() const {
		return !isEmpty() &&
			(host_ == "localhost" || host_ == "127.0.0.1" ||
				host_ == "::1" || host_ == "0:0:0:0:0:0:0:1");
	}

	bool Uri::isHostWildcard() const {
		return !isEmpty() && (host_ == "*" || host_ == "+");
	}

	bool Uri::isHostPortable() const {
		return !(isEmpty() || isHostLoopback() || isHostWildcard());
	}

	bool Uri::isPortDefault() const {
		return !isEmpty() && port_ == 0;
	}

	bool Uri::isAuthority() const {
		return !isEmpty() && isPathEmpty() && query_.empty() && fragment_.empty();
	}

	bool Uri::isPathEmpty() const {
		return path_.empty() || path_ == "/";
	}

	bool Uri::hasSameAuthority(const Uri& other) const {
		return !isEmpty() && this->authority().toString() == other.authority().toString();
	}

	std::string Uri::resolveUri(const std::string& relativeUri) const {
		Uri relative(relativeUri);
		Uri resolved = resolve(relative);
		return resolved.toString();
	}

	Uri Uri::resolve(const Uri& relative) const {
		// 如果相对URI是绝对的，直接返回
		if (!relative.scheme_.empty()) {
			return relative;
		}

		Uri result = *this;

		// 如果相对URI有authority，替换整个authority
		if (!relative.host_.empty()) {
			result.host_ = relative.host_;
			result.userInfo_ = relative.userInfo_;
			result.port_ = relative.port_;
			result.path_ = relative.path_;
			result.query_ = relative.query_;
			result.fragment_ = relative.fragment_;

			// 规范化路径
			if (result.path_.empty()) {
				result.path_ = "/";
			}

			return result;
		}

		// 处理相对路径
		if (!relative.path_.empty()) {
			if (relative.path_[0] == '/') {
				// 绝对路径：替换整个路径
				result.path_ = relative.path_;
			}
			else {
				// 相对路径：合并路径
				if (result.path_.empty()) {
					result.path_ = "/" + relative.path_;
				}
				else {
					// 找到最后一个 '/' 的位置
					size_t lastSlash = result.path_.find_last_of('/');
					if (lastSlash != std::string::npos) {
						result.path_ = result.path_.substr(0, lastSlash + 1) + relative.path_;
					}
					else {
						result.path_ = "/" + relative.path_;
					}
				}

				// 简化路径（处理 . 和 ..）
				result.path_ = simplifyPath(result.path_);
			}
			result.query_ = relative.query_;
			result.fragment_ = relative.fragment_;
		}
		else {
			// 路径为空，处理 query 和 fragment
			if (!relative.query_.empty()) {
				result.query_ = relative.query_;
				result.fragment_ = relative.fragment_;
			}
			else {
				result.fragment_ = relative.fragment_;
			}
		}

		return result;
	}

	bool Uri::operator==(const Uri& other) const {
		return toString() == other.toString();
	}

	bool Uri::operator!=(const Uri& other) const {
		return !(*this == other);
	}

	bool Uri::operator<(const Uri& other) const {
		return toString() < other.toString();
	}

	void Uri::clear() {
		scheme_.clear();
		host_.clear();
		userInfo_.clear();
		path_.clear();
		query_.clear();
		fragment_.clear();
		port_ = 0;
		originalUri_.clear();
		queryParams_.clear();
		queryParamsDirty_ = false;
	}

	void Uri::parseFrom(const std::string& other) {
		scheme_.clear();
		host_.clear();
		userInfo_.clear();
		path_.clear();
		query_.clear();
		fragment_.clear();
		port_ = 0;

		const char* p = other.c_str();

		bool isRelativeReference = true;
		const char* p2 = p;
		for (; *p2 != '/' && *p2 != '\0'; ++p2) {
			if (*p2 == ':') {
				isRelativeReference = false;
				break;
			}
		}

		if (!isRelativeReference) {
			if (!isalpha(*p)) {
				throw std::invalid_argument("Invalid URI: scheme must start with a letter");
			}

			const char* schemeStart = p++;
			for (; *p != ':'; ++p) {
				if (!isSchemeChar(*p)) {
					throw std::invalid_argument("Invalid URI: invalid character in scheme");
				}
			}
			scheme_ = std::string(schemeStart, p);
			++p;
		}

		if (*p == '/' && *(p + 1) == '/') {
			p += 2;
			const char* authorityStart = p;

			while (*p != '/' && *p != '?' && *p != '#' && *p != '\0') {
				if (!isAuthorityChar(*p)) {
					throw std::invalid_argument("Invalid URI: invalid character in authority");
				}
				++p;
			}

			const char* authorityEnd = p;

			if (authorityStart != authorityEnd) {
				parseAuthority(authorityStart, authorityEnd);
			}
		}

		if (*p == '/') {
			const char* pathStart = p;
			while (*p != '?' && *p != '#' && *p != '\0') {
				if (!isPathChar(*p)) {
					throw std::invalid_argument("Invalid URI: invalid character in path");
				}
				++p;
			}
			path_ = std::string(pathStart, p);
		}
		else if (*p != '?' && *p != '#' && *p != '\0') {
			const char* pathStart = p;
			while (*p != '?' && *p != '#' && *p != '\0') {
				if (!isPathChar(*p)) {
					throw std::invalid_argument("Invalid URI: invalid character in path");
				}
				++p;
			}
			path_ = std::string(pathStart, p);
		}

		if (*p == '?') {
			++p;
			const char* queryStart = p;
			while (*p != '#' && *p != '\0') {
				if (!isQueryChar(*p)) {
					throw std::invalid_argument("Invalid URI: invalid character in query");
				}
				++p;
			}
			query_ = std::string(queryStart, p);
			queryParamsDirty_ = true;
		}

		if (*p == '#') {
			++p;
			const char* fragmentStart = p;
			while (*p != '\0') {
				if (!isFragmentChar(*p)) {
					throw std::invalid_argument("Invalid URI: invalid character in fragment");
				}
				++p;
			}
			fragment_ = std::string(fragmentStart, p);
		}
	}

	void Uri::parseAuthority(const char* start, const char* end) {
		const char* atPos = nullptr;
		const char* p = start;

		for (; p != end; ++p) {
			if (*p == '@') {
				atPos = p;
				break;
			}
		}

		const char* hostStart = start;
		const char* hostEnd = end;

		if (atPos != nullptr) {
			userInfo_ = std::string(start, atPos);
			hostStart = atPos + 1;
		}

		bool isIPv6 = false;
		const char* colonPos = nullptr;

		if (hostStart < hostEnd && *hostStart == '[') {
			isIPv6 = true;
			const char* ipv6End = hostStart + 1;
			for (; ipv6End != hostEnd; ++ipv6End) {
				if (*ipv6End == ']') {
					break;
				}
			}

			if (ipv6End != hostEnd) {
				host_ = std::string(hostStart, ipv6End + 1);
				hostStart = ipv6End + 1;

				if (hostStart < hostEnd && *hostStart == ':') {
					colonPos = hostStart;
				}
			}
		}
		else {
			p = hostEnd - 1;
			for (; p >= hostStart; --p) {
				if (*p == ':') {
					colonPos = p;
					break;
				}
			}
		}

		if (colonPos != nullptr) {
			if (!isIPv6) {
				host_ = std::string(hostStart, colonPos);
			}

			const char* portStart = colonPos + 1;
			uint16_t portValue = 0;
			for (const char* q = portStart; q != hostEnd; ++q) {
				if (!isdigit(*q)) {
					throw std::invalid_argument("Invalid URI: port must contain only digits");
				}
				portValue = portValue * 10 + (*q - '0');
				if (portValue > 65535) {
					throw std::invalid_argument("Invalid URI: port out of range (0-65535)");
				}
			}
			port_ = portValue;
		}
		else if (!isIPv6) {
			host_ = std::string(hostStart, hostEnd);
		}
	}

	void Uri::parseQueryParams() {
		queryParams_.clear();
		if (query_.empty()) {
			queryParamsDirty_ = false;
			return;
		}

		size_t start = 0;
		size_t end = query_.find('&');
		while (true) {
			std::string param;
			if (end == std::string::npos) {
				param = query_.substr(start);
			}
			else {
				param = query_.substr(start, end - start);
			}

			size_t eq_pos = param.find('=');
			if (eq_pos != std::string::npos) {
				std::string key = param.substr(0, eq_pos);
				std::string value = param.substr(eq_pos + 1);
				queryParams_.push_back({ key, value });
			}
			else if (!param.empty()) {
				queryParams_.push_back({ param, "" });
			}

			if (end == std::string::npos) break;
			start = end + 1;
			end = query_.find('&', start);
		}
		queryParamsDirty_ = false;
	}

	void Uri::buildQueryString() {
		if (queryParams_.empty()) {
			query_.clear();
			return;
		}

		std::stringstream ss;
		for (size_t i = 0; i < queryParams_.size(); ++i) {
			if (i > 0) ss << "&";
			ss << queryParams_[i].first;
			if (!queryParams_[i].second.empty()) {
				ss << "=" << queryParams_[i].second;
			}
		}
		query_ = ss.str();
	}

	bool Uri::isSchemeChar(char c) {
		return (c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '+' || c == '-' || c == '.';
	}

	bool Uri::isAuthorityChar(char c) {
		return (c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~' ||
			c == ':' || c == '[' || c == ']' || c == '@' ||
			c == '!' || c == '$' || c == '&' || c == '\'' ||
			c == '(' || c == ')' || c == '*' || c == '+' ||
			c == ',' || c == ';' || c == '=';
	}

	bool Uri::isUserInfoChar(char c) {
		return (c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~' ||
			c == '!' || c == '$' || c == '&' || c == '\'' ||
			c == '(' || c == ')' || c == '*' || c == '+' ||
			c == ',' || c == ';' || c == '=' || c == ':';
	}

	bool Uri::isPathChar(char c) {
		return (c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~' ||
			c == '/' || c == ':' || c == '@' || c == '!' ||
			c == '$' || c == '&' || c == '\'' || c == '(' ||
			c == ')' || c == '*' || c == '+' || c == ',' ||
			c == ';' || c == '=' || c == '?';
	}

	bool Uri::isQueryChar(char c) {
		return (c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~' ||
			c == '/' || c == '?' || c == ':' || c == '@' ||
			c == '!' || c == '$' || c == '&' || c == '\'' ||
			c == '(' || c == ')' || c == '*' || c == '+' ||
			c == ',' || c == ';' || c == '=';
	}

	bool Uri::isFragmentChar(char c) {
		return (c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~' ||
			c == '/' || c == '?' || c == ':' || c == '@' ||
			c == '!' || c == '$' || c == '&' || c == '\'' ||
			c == '(' || c == ')' || c == '*' || c == '+' ||
			c == ',' || c == ';' || c == '=';
	}

	std::string Uri::simplifyPath(const std::string& path) {
		if (path.empty()) {
			return "/";
		}

		std::vector<std::string> segments;
		std::string segment;

		for (char c : path) {
			if (c == '/') {
				if (!segment.empty()) {
					if (segment == "..") {
						if (!segments.empty()) {
							segments.pop_back();
						}
					}
					else if (segment != ".") {
						segments.push_back(segment);
					}
					segment.clear();
				}
			}
			else {
				segment += c;
			}
		}

		if (!segment.empty()) {
			if (segment == "..") {
				if (!segments.empty()) {
					segments.pop_back();
				}
			}
			else if (segment != ".") {
				segments.push_back(segment);
			}
		}

		std::string result;
		for (const auto& seg : segments) {
			result += "/" + seg;
		}

		if (result.empty()) {
			return "/";
		}

		if (path.back() == '/' && result != "/") {
			result += '/';
		}

		return result;
	}
}
