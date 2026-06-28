/**
 * @file ascii_logo.h
 * @author keisum (Keisumhuis@gmail.com)
 * @brief 日期类
 * @version 0.1
 * @date 3
 *
 * @copyright Copyright (c) 2024
 */
#pragma once

#include <stdint.h>
#include <time.h>

#include <string>

#include "crazy/time_zone.h"

namespace crazy {
	/**
	 * @brief 星期.
	 */
	enum class WeekDay : uint16_t {
		//! 周日
		sunday,
		//! 周一
		monday,
		//! 周二
		tuesday,
		//! 周三
		wednesday,
		//! 周四
		thursday,
		//! 周五
		friday,
		//! 周六
		saturday,
	};
	/**
	 * @brief 日期类.
	 */
	class DateTime final {
	public:
		/**
		 * @brief 构造函数.
		 */
		explicit DateTime(time_t timestamp = time(nullptr), TimeZone timeZone = TimeZone::Local());
		/**
		 * @brief 构造函数.
		 */
		explicit DateTime(uint32_t year, uint32_t month, uint32_t day, uint32_t hour = 0, uint32_t minute = 0, uint32_t second = 0, TimeZone timeZone = TimeZone::Local());
		/**
		 * @brief 拷贝构造函数.
		 */
		DateTime(const DateTime&) = default;
		/**
		 * @brief 赋值函数.
		 */
		DateTime& operator=(const DateTime&) = default;
		/**
		 * @brief 获取当前日期时间.
		 */
		static DateTime now();
		/**
		 * @brief 从字符串中加载日期时间.
		 */
		static DateTime fromString(const std::string& dateTime, const std::string& format = "%Y-%m-%d %H:%M:%S");
		/**
		 * @brief 获取年份.
		 */
		uint32_t year() const;
		/**
		 * @brief 获取月份.
		 */
		uint32_t month() const;
		/**
		 * @brief 获取天.
		 */
		uint32_t day() const;
		/**
		 * @brief 获取小时.
		 */
		uint32_t hour() const;
		/**
		 * @brief 获取分钟.
		 */
		uint32_t minute() const;
		/**
		 * @brief 获取秒数.
		 */
		uint32_t second() const;
		/**
		 * @brief 获取时间戳.
		 */
		uint32_t timestamp() const;
		/**
		 * @brief 获取星期.
		 */
		WeekDay weekDay();
		/**
		 * @brief 获取一年中的第几天 (1-366)
		 */
		uint32_t yearDay();
		/**
		 * @brief 获取当前年份是否为闰年.
		 */
		bool isLeapYear();
		/**
		 * @brief 获取指定年份是否为闰年.
		 */
		static bool isLeapYear(uint32_t year);
		/**
		 * @brief 获取当前年份月份的天数.
		 */
		uint32_t daysInMonth();
		/**
		 * @brief 转换为字符串
		 * @param format 格式字符串，默认为"%Y-%m-%d %H:%M:%S"
		 */
		std::string toString(const std::string& format = "%Y-%m-%d %H:%M:%S");
		/**
		 * @brief 增加年份.
		 */
		DateTime& addYears(int32_t years);
		/**
		 * @brief 增加月份.
		 */
		DateTime& addMonths(int32_t months);
		/**
		 * @brief 增加天数.
		 */
		DateTime& addDays(int32_t days);
		/**
		 * @brief 增加小时.
		 */
		DateTime& addHours(int32_t hours);
		/**
		 * @brief 增加分钟.
		 */
		DateTime& addMinutes(int32_t minutes);
		/**
		 * @brief 增加秒数.
		 */
		DateTime& addSeconds(int32_t seconds);
		/**
		 * @brief 等于比较.
		 */
		bool operator==(const DateTime& other) const;
		/**
		 * @brief 不等于比较.
		 */
		bool operator!=(const DateTime& other) const;
		/**
		 * @brief 小于比较.
		 */
		bool operator<(const DateTime& other) const;
		/**
		 * @brief 小于等于比较.
		 */
		bool operator<=(const DateTime& other) const;
		/**
		 * @brief 大于比较.
		 */
		bool operator>(const DateTime& other) const;
		/**
		 * @brief 大于等于比较.
		 */
		bool operator>=(const DateTime& other) const;

	protected:
		/**
		 * @brief 验证并规范化日期时间.
		 */
		void normalize();
		/**
		 * @brief 从时间戳计算日期时间.
		 */
		void fromTimestamp(uint64_t timestamp);
		/**
		 * @brief 从日期时间计算时间戳.
		 */
		void toTimestamp();
		/**
		 * @brief 获取指定年份月份的天数.
		 */
		static uint32_t daysInMonth(uint32_t year, uint32_t month);
		/**
		 * @brief 获取指定日期是星期几（Zeller's congruence算法）.
		 */
		static WeekDay calculateWeekDay(uint32_t year, uint32_t month, uint32_t day);
		/**
		 * @brief 获取指定日期是一年中的第几天.
		 */
		static uint32_t calculateYearDay(uint32_t year, uint32_t month, uint32_t day);

	private:
		//! 年
		uint32_t year_ = 0;
		//! 月
		uint32_t month_ = 0;
		//! 日
		uint32_t day_ = 0;
		//! 时
		uint32_t hour_ = 0;
		//! 分
		uint32_t minute_ = 0;
		//! 秒
		uint32_t second_ = 0;
		//! 时间戳
		time_t timestamp_ = 0;
		//! 时区
		TimeZone timeZone_ = TimeZone::Local();
	};
}
