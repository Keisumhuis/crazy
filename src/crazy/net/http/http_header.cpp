#include "crazy/net/http/http_header.h"

namespace crazy {
	std::string HttpHeader::normalizeKey(const std::string& key) {
		return StringUtil::ToLower(StringUtil::Trim(key));
	}
	bool HttpHeader::insert(const std::string& key, const std::string& value) {
		headers_.emplace(normalizeKey(key), value);
		return true;
	}
	std::optional<std::string> HttpHeader::get(const std::string& key) const {
		auto it = headers_.find(normalizeKey(key));
		if (it == headers_.end()) {
			return std::nullopt;
		}
		return it->second;
	}
	bool HttpHeader::contains(const std::string& key) const {
		return headers_.find(normalizeKey(key)) != headers_.end();
	}
	bool HttpHeader::erase(const std::string& key) {
		auto range = headers_.equal_range(normalizeKey(key));
		if (range.first == range.second) {
			return false;
		}
		headers_.erase(range.first, range.second);
		return true;
	}
	void HttpHeader::clear() {
		headers_.clear();
	}
	std::size_t HttpHeader::size() const {
		return headers_.size();
	}
	bool HttpHeader::empty() const {
		return headers_.empty();
	}
	HttpHeader::iterator HttpHeader::begin() {
		return headers_.begin();
	}
	HttpHeader::iterator HttpHeader::end() {
		return headers_.end();
	}
	HttpHeader::const_iterator HttpHeader::begin() const {
		return headers_.begin();
	}
	HttpHeader::const_iterator HttpHeader::end() const {
		return headers_.end();
	}
	HttpHeader::const_iterator HttpHeader::cbegin() const {
		return headers_.cbegin();
	}
	HttpHeader::const_iterator HttpHeader::cend() const {
		return headers_.cend();
	}
	HttpHeader::container_type& HttpHeader::data() {
		return headers_;
	}
	const HttpHeader::container_type& HttpHeader::data() const {
		return headers_;
	}
	HttpHeader::iterator HttpHeader::find(const std::string& key) {
		return headers_.find(normalizeKey(key));
	}
	HttpHeader::const_iterator HttpHeader::find(const std::string& key) const {
		return headers_.find(normalizeKey(key));
	}
}
