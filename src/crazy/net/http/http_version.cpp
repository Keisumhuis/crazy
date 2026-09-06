#include "crazy/net/http/http_version.h"

namespace crazy {
	HttpVersion::HttpVersion()
		: major_(1), minor_(1) {}
	HttpVersion::HttpVersion(uint8_t major, uint8_t minor)
		: major_(major), minor_(minor) {}
	uint8_t HttpVersion::getMajor() const {
		return major_;
	}
	uint8_t HttpVersion::getMinor() const {
		return minor_;
	}
	std::string HttpVersion::toString() const {
		return "HTTP/" + std::to_string(major_) + "." + std::to_string(minor_);
	}
	bool HttpVersion::operator==(const HttpVersion& other) const {
		return major_ == other.major_ && minor_ == other.minor_;
	}
	bool HttpVersion::operator<(const HttpVersion& other) const {
		if (major_ < other.major_) return true;
		if (major_ > other.major_) return false;
		return minor_ < other.minor_;
	}
	bool HttpVersion::operator!=(const HttpVersion& other) const {
		return !(*this == other);
	}
	bool HttpVersion::operator>=(const HttpVersion& other) const {
		return !(*this < other);
	}
	bool HttpVersion::operator>(const HttpVersion& other) const {
		return other < *this;
	}
	bool HttpVersion::operator<=(const HttpVersion& other) const {
		return !(*this > other);
	}
	HttpVersion HttpVersions::HTTP_0_9(0, 9);
	HttpVersion HttpVersions::HTTP_1_0(1, 0);
	HttpVersion HttpVersions::HTTP_1_1(1, 1);
}  // namespace crazy
