#include "crazy/date_time.h"

#include <stdexcept>

namespace crazy {
	// 每月天数表（非闰年）
	const uint32_t DAYS_IN_MONTH[] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	// 每月累计天数表（非闰年）
	const uint32_t CUMULATIVE_DAYS[] = {
		//! 1月之前
		0,
		//! 2月之前
		31,
		//! 3月之前
		59,
		//! 4月之前
		90,
		//! 5月之前
		120,
		//! 6月之前
		151,
		//! 7月之前
		181,
		//! 8月之前
		212,
		//! 9月之前
		243,
		//! 10月之前
		273,
		//! 11月之前
		304,
		//! 12月之前
		334
	};

	//! 1 分钟的秒数
	const int64_t SECONDS_PER_MINUTE = 60;
	//! 1 小时的秒数
	const int64_t SECONDS_PER_HOUR = 3600;
	//! 1 天的秒数
	const int64_t SECONDS_PER_DAY = 86400;
	//! 1 年的天数 
	const int64_t DAYS_PER_YEAR = 365;
	//! 4 年的天数 365 * 4 + 1
	const int64_t DAYS_PER_4_YEARS = 1461;
	//! 100 年的天数 365 * 100 + 24
	const int64_t DAYS_PER_100_YEARS = 36524;
	//! 400 年的天数 365 * 400 + 97
	const int64_t DAYS_PER_400_YEARS = 146097;
	//! 1970年1月1日距离0001年1月1日的天数
	const int64_t UNIX_EPOCH_DAYS = 719162;

	DateTime::DateTime(time_t timestamp, TimeZone timeZone)
		: timestamp_(timestamp), timeZone_(timeZone) {
		fromTimestamp(timestamp);
	}
	DateTime::DateTime(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second, TimeZone timeZone)
		: year_(year), month_(month), day_(day), hour_(hour), minute_(minute), second_(second), timeZone_(timeZone) {
		toTimestamp();
	}
	DateTime DateTime::now() {
		return DateTime();
	}
	DateTime DateTime::fromString(const std::string& dateTime, const std::string& format) {
		DateTime result;
		result.timeZone_ = TimeZone(); // Default timezone, adjust as needed

		size_t pos = 0;
		size_t format_pos = 0;

		// Initialize with default values (January 1, 1970)
		uint32_t year = 1970;
		uint32_t month = 1;
		uint32_t day = 1;
		uint32_t hour = 0;
		uint32_t minute = 0;
		uint32_t second = 0;

		// Parse the format string and extract values from dateTime
		while (format_pos < format.size() && pos < dateTime.size()) {
			if (format[format_pos] == '%' && format_pos + 1 < format.size()) {
				char specifier = format[format_pos + 1];
				format_pos += 2;

				switch (specifier) {
				case 'Y': { // Year (4 digits)
					if (pos + 4 > dateTime.size()) {
						throw std::invalid_argument("Invalid format: insufficient characters for year");
					}
					std::string year_str = dateTime.substr(pos, 4);
					year = static_cast<uint32_t>(std::stoul(year_str));
					pos += 4;
					break;
				}
				case 'y': { // Year (2 digits)
					if (pos + 2 > dateTime.size()) {
						throw std::invalid_argument("Invalid format: insufficient characters for year");
					}
					std::string year_str = dateTime.substr(pos, 2);
					year = 2000 + static_cast<uint32_t>(std::stoul(year_str));
					pos += 2;
					break;
				}
				case 'm': { // Month (01-12)
					if (pos + 2 > dateTime.size()) {
						throw std::invalid_argument("Invalid format: insufficient characters for month");
					}
					std::string month_str = dateTime.substr(pos, 2);
					month = static_cast<uint32_t>(std::stoul(month_str));
					if (month < 1 || month > 12) {
						throw std::invalid_argument("Invalid month value: " + std::to_string(month));
					}
					pos += 2;
					break;
				}
				case 'd': { // Day (01-31)
					if (pos + 2 > dateTime.size()) {
						throw std::invalid_argument("Invalid format: insufficient characters for day");
					}
					std::string day_str = dateTime.substr(pos, 2);
					day = static_cast<uint32_t>(std::stoul(day_str));
					pos += 2;
					break;
				}
				case 'H': { // Hour (00-23)
					if (pos + 2 > dateTime.size()) {
						throw std::invalid_argument("Invalid format: insufficient characters for hour");
					}
					std::string hour_str = dateTime.substr(pos, 2);
					hour = static_cast<uint32_t>(std::stoul(hour_str));
					if (hour > 23) {
						throw std::invalid_argument("Invalid hour value: " + std::to_string(hour));
					}
					pos += 2;
					break;
				}
				case 'M': { // Minute (00-59)
					if (pos + 2 > dateTime.size()) {
						throw std::invalid_argument("Invalid format: insufficient characters for minute");
					}
					std::string minute_str = dateTime.substr(pos, 2);
					minute = static_cast<uint32_t>(std::stoul(minute_str));
					if (minute > 59) {
						throw std::invalid_argument("Invalid minute value: " + std::to_string(minute));
					}
					pos += 2;
					break;
				}
				case 'S': { // Second (00-59)
					if (pos + 2 > dateTime.size()) {
						throw std::invalid_argument("Invalid format: insufficient characters for second");
					}
					std::string second_str = dateTime.substr(pos, 2);
					second = static_cast<uint32_t>(std::stoul(second_str));
					if (second > 59) {
						throw std::invalid_argument("Invalid second value: " + std::to_string(second));
					}
					pos += 2;
					break;
				}
				case 'b': // Abbreviated month name (Jan, Feb, etc.)
				case 'B': { // Full month name
					// Find the next non-letter character or end of string
					size_t name_start = pos;
					while (pos < dateTime.size() && std::isalpha(static_cast<unsigned char>(dateTime[pos]))) {
						pos++;
					}
					std::string month_name = dateTime.substr(name_start, pos - name_start);

					// Simple mapping for month names
					const char* month_names[] = {
						"Jan", "Feb", "Mar", "Apr", "May", "Jun",
						"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
					};
					const char* full_month_names[] = {
						"January", "February", "March", "April", "May", "June",
						"July", "August", "September", "October", "November", "December"
					};

					month = 0;
					if (specifier == 'b') {
						for (int i = 0; i < 12; i++) {
							if (month_name == month_names[i]) {
								month = i + 1;
								break;
							}
						}
					}
					else {
						for (int i = 0; i < 12; i++) {
							if (month_name == full_month_names[i]) {
								month = i + 1;
								break;
							}
						}
					}

					if (month == 0) {
						throw std::invalid_argument("Invalid month name: " + month_name);
					}
					break;
				}
				case 'w': { // Weekday (0-6, Sunday=0)
					if (pos >= dateTime.size()) {
						throw std::invalid_argument("Invalid format: insufficient characters for weekday");
					}
					// Skip the weekday value as it's derived from the date
					pos += 1;
					break;
				}
				default:
					// Unknown format specifier, treat as literal
					if (format[format_pos - 2] != '%') {
						// This shouldn't happen, but just in case
						if (pos < dateTime.size() && dateTime[pos] == format[format_pos - 2]) {
							pos++;
						}
					}
					break;
				}
			}
			else {
				// Match literal characters
				if (pos < dateTime.size() && dateTime[pos] == format[format_pos]) {
					pos++;
					format_pos++;
				}
				else {
					throw std::invalid_argument("Format mismatch at position " + std::to_string(format_pos) +
						": expected '" + format[format_pos] + "', got '" +
						(pos < dateTime.size() ? std::string(1, dateTime[pos]) : "end of string") + "'");
				}
			}
		}

		// Validate the date
		if (month < 1 || month > 12) {
			throw std::invalid_argument("Invalid month: " + std::to_string(month));
		}

		uint32_t max_day = daysInMonth(year, month);
		if (day < 1 || day > max_day) {
			throw std::invalid_argument("Invalid day: " + std::to_string(day) +
				" for month " + std::to_string(month) +
				" (max: " + std::to_string(max_day) + ")");
		}

		// Set the parsed values
		result.year_ = year;
		result.month_ = month;
		result.day_ = day;
		result.hour_ = hour;
		result.minute_ = minute;
		result.second_ = second;

		// Calculate timestamp
		result.toTimestamp();

		return result;
	}
	uint32_t DateTime::year() const {
		return year_;
	}
	uint32_t DateTime::month() const {
		return month_;
	}
	uint32_t DateTime::day() const {
		return day_;
	}
	uint32_t DateTime::hour() const {
		return hour_;
	}
	uint32_t DateTime::minute() const {
		return minute_;
	}
	uint32_t DateTime::second() const {
		return second_;
	}
	uint32_t DateTime::timestamp() const {
		return timestamp_;
	}
	WeekDay DateTime::weekDay() {
		return calculateWeekDay(year_, month_, day_);
	}
	uint32_t DateTime::yearDay() {
		return calculateYearDay(year_, month_, day_);
	}
	bool DateTime::isLeapYear() {
		return isLeapYear(year_);
	}
	uint32_t DateTime::daysInMonth() {
		return daysInMonth(year_, month_);
	}
	bool DateTime::isLeapYear(uint32_t year) {
		return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
	}
	std::string DateTime::toString(const std::string& format) {
		std::string result;
		result.reserve(64);

		for (size_t i = 0; i < format.size(); ++i) {
			if (format[i] == '%' && i + 1 < format.size()) {
				switch (format[++i]) {
				case 'Y': {
					char buf[5];
					snprintf(buf, sizeof(buf), "%04u", year_);
					result += buf;
					break;
				}
				case 'y': {
					char buf[3];
					snprintf(buf, sizeof(buf), "%02u", year_ % 100);
					result += buf;
					break;
				}
				case 'm': {
					char buf[3];
					snprintf(buf, sizeof(buf), "%02u", month_);
					result += buf;
					break;
				}
				case 'd': {
					char buf[3];
					snprintf(buf, sizeof(buf), "%02u", day_);
					result += buf;
					break;
				}
				case 'H': {
					char buf[3];
					snprintf(buf, sizeof(buf), "%02u", hour_);
					result += buf;
					break;
				}
				case 'M': {
					char buf[3];
					snprintf(buf, sizeof(buf), "%02u", minute_);
					result += buf;
					break;
				}
				case 'S': {
					char buf[3];
					snprintf(buf, sizeof(buf), "%02u", second_);
					result += buf;
					break;
				}
				case 'w': {
					result += std::to_string(static_cast<uint32_t>(weekDay()));
					break;
				}
				case 'j': {
					char buf[4];
					snprintf(buf, sizeof(buf), "%03u", yearDay());
					result += buf;
					break;
				}
				default:
					result += format[i];
					break;
				}
			}
			else {
				result += format[i];
			}
		}

		return result;
	}
	DateTime& DateTime::addYears(int32_t years) {
		year_ += years;
		normalize();
		toTimestamp();
		return *this;
	}
	DateTime& DateTime::addMonths(int32_t months) {
		int total_months = static_cast<int>(month_) + months;
		year_ += (total_months - 1) / 12;
		month_ = ((total_months - 1) % 12) + 1;
		normalize();
		toTimestamp();
		return *this;
	}
	DateTime& DateTime::addDays(int32_t days) {
		timestamp_ += days * SECONDS_PER_DAY;
		fromTimestamp(timestamp_);
		return *this;
	}
	DateTime& DateTime::addHours(int32_t hours) {
		timestamp_ += hours * SECONDS_PER_HOUR;
		fromTimestamp(timestamp_);
		return *this;
	}
	DateTime& DateTime::addMinutes(int32_t minutes) {
		timestamp_ += minutes * SECONDS_PER_MINUTE;
		fromTimestamp(timestamp_);
		return *this;
	}
	DateTime& DateTime::addSeconds(int32_t seconds) {
		timestamp_ += seconds;
		fromTimestamp(timestamp_);
		return *this;
	}
	bool DateTime::operator==(const DateTime& other) const {
		return timestamp_ == other.timestamp_;
	}
	bool DateTime::operator!=(const DateTime& other) const {
		return timestamp_ != other.timestamp_;
	}
	bool DateTime::operator<(const DateTime& other) const {
		return timestamp_ < other.timestamp_;
	}
	bool DateTime::operator<=(const DateTime& other) const {
		return timestamp_ <= other.timestamp_;
	}
	bool DateTime::operator>(const DateTime& other) const {
		return timestamp_ > other.timestamp_;
	}
	bool DateTime::operator>=(const DateTime& other) const {
		return timestamp_ >= other.timestamp_;
	}
	void DateTime::normalize() {
		if (second_ >= 60) {
			minute_ += second_ / 60;
			second_ %= 60;
		}
		if (minute_ >= 60) {
			hour_ += minute_ / 60;
			minute_ %= 60;
		}
		if (hour_ >= 24) {
			day_ += hour_ / 24;
			hour_ %= 24;
		}
		while (day_ > daysInMonth(year_, month_)) {
			day_ -= daysInMonth(year_, month_);
			month_++;
			if (month_ > 12) {
				month_ = 1;
				year_++;
			}
		}
		while (month_ > 12) {
			month_ -= 12;
			year_++;
		}
		if (year_ == 0) {
			year_ = 1;
		}
	}
	void DateTime::fromTimestamp(uint64_t timestamp) {
		bool negative = (timestamp + timeZone_.getOffset()) < 0;
		int64_t days = negative ? -1 : 0;

		days += (timestamp + timeZone_.getOffset()) / SECONDS_PER_DAY;

		int64_t remaining_seconds = (timestamp + timeZone_.getOffset()) % SECONDS_PER_DAY;
		if (remaining_seconds < 0) {
			remaining_seconds += SECONDS_PER_DAY;
			days -= 1;
		}

		days += UNIX_EPOCH_DAYS;

		int64_t temp_days = days;

		int64_t n400 = temp_days / DAYS_PER_400_YEARS;
		temp_days %= DAYS_PER_400_YEARS;

		int64_t n100 = temp_days / DAYS_PER_100_YEARS;
		if (n100 == 4) {
			n100 = 3;
			temp_days = DAYS_PER_100_YEARS;
		}
		temp_days %= DAYS_PER_100_YEARS;

		int64_t n4 = temp_days / DAYS_PER_4_YEARS;
		temp_days %= DAYS_PER_4_YEARS;

		int64_t n1 = temp_days / DAYS_PER_YEAR;
		if (n1 == 4) { 
			n1 = 3;
			temp_days = DAYS_PER_YEAR;
		}
		temp_days %= DAYS_PER_YEAR;

		year_ = static_cast<uint32_t>(n400 * 400 + n100 * 100 + n4 * 4 + n1 + 1);

		bool leap = isLeapYear(year_);
		temp_days += 1;

		const int32_t days_in_month[] = { 31, (leap ? 29 : 28), 31, 30, 31, 30,
										  31, 31, 30, 31, 30, 31 };

		month_ = 1;
		uint32_t days_left = static_cast<uint32_t>(temp_days);

		for (int i = 0; i < 12; ++i) {
			uint32_t month_days = days_in_month[i];
			if (days_left <= month_days) {
				day_ = days_left;
				break;
			}
			days_left -= month_days;
			month_++;
		}

		if (month_ > 12) {
			month_ = 12;
			day_ = 31;
		}

		// 计算时分秒
		hour_ = static_cast<uint32_t>(remaining_seconds / SECONDS_PER_HOUR);
		remaining_seconds %= SECONDS_PER_HOUR;
		minute_ = static_cast<uint32_t>(remaining_seconds / SECONDS_PER_MINUTE);
		second_ = static_cast<uint32_t>(remaining_seconds % SECONDS_PER_MINUTE);

		timestamp_ = timestamp;
	}
	void DateTime::toTimestamp() {
		int64_t days = 0;
		uint32_t year = year_ - 1;
		days = year * DAYS_PER_YEAR;
		days += year / 4;
		days -= year / 100;
		days += year / 400;

		days += calculateYearDay(year_, month_, day_) - 1;

		days -= UNIX_EPOCH_DAYS;

		timestamp_ = days * SECONDS_PER_DAY;
		timestamp_ += hour_ * SECONDS_PER_HOUR;
		timestamp_ += minute_ * SECONDS_PER_MINUTE;
		timestamp_ += second_;
		timestamp_ -= timeZone_.getOffset();
	}
	uint32_t DateTime::daysInMonth(uint32_t year, uint32_t month) {
		if (month < 1 || month > 12) return 0;
		if (month == 2) {
			return isLeapYear(year) ? 29 : 28;
		}
		return DAYS_IN_MONTH[month - 1];
	}
	WeekDay DateTime::calculateWeekDay(uint32_t year, uint32_t month, uint32_t day) {
		if (month < 3) {
			month += 12;
			year -= 1;
		}

		uint32_t K = year % 100;
		uint32_t J = year / 100;
		uint32_t h = (day + (13 * (month + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;

		return static_cast<WeekDay>((h + 6) % 7);
	}
	uint32_t DateTime::calculateYearDay(uint32_t year, uint32_t month, uint32_t day) {
		if (month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month)) {
			return 0;
		}

		uint32_t day_of_year = CUMULATIVE_DAYS[month - 1] + day;

		if (month > 2 && isLeapYear(year)) {
			day_of_year += 1;
		}

		return day_of_year;
	}
}
