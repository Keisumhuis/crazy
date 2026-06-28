/**
* @file time_zone.h
* @brief 时区类
* @author Keisum
* @date 2025-12-09
* @copyright
* @version 1.0
*/
#pragma once

#include <string>
#include <ctime>

namespace crazy {
    /**
     * @brief 时区类
     */
    class TimeZone final {
    public:
        /**
         * @brief 默认构造函数，使用系统本地时区
         */
        TimeZone();
        /**
         * @brief 构造函数，指定时区偏移量
         * @param offset 相对于UTC的偏移量（秒）
         * @param name 时区名称
         */
        explicit TimeZone(int32_t offset, const std::string& name = "");
        /**
         * @brief 获取UTC时区
         * @return UTC时区对象
         */
        static TimeZone UTC();
        /**
         * @brief 获取系统本地时区
         * @return 本地时区对象
         */
        static TimeZone Local();
        /**
         * @brief 获取时区名称
         * @return 时区名称
         */
        const std::string& getName() const;
        /**
         * @brief 获取时区偏移量（秒）
         * @return 相对于UTC的偏移量
         */
        int32_t getOffset() const;
        /**
         * @brief 设置时区名称
         * @param name 时区名称
         */
        void setName(const std::string& name);
        /**
         * @brief 设置时区偏移量
         * @param offset 相对于UTC的偏移量（秒）
         */
        void setOffset(int32_t offset);
        /**
         * @brief 转换为UTC时间
         * @param local_time 本地时间戳
         * @return UTC时间戳
         */
        time_t toUTC(time_t local_time) const;
        /**
         * @brief 转换为本地时间
         * @param utc_time UTC时间戳
         * @return 本地时间戳
         */
        time_t toLocal(time_t utc_time) const;
        /**
         * @brief 检查时区是否有效
         * @return 时区是否有效
         */
        bool isValid() const;
        /**
         * @brief 比较时区是否相等
         */
        bool operator==(const TimeZone& other) const;
        /**
         * @brief 比较时区是否不相等
         */
        bool operator!=(const TimeZone& other) const;

    private:
        /**
         * @brief .计算系统本地时区偏移量
         */
        static int32_t calculateSystemOffset();
        /**
         * @brief 获取系统本地时区名称.
         */
        static std::string getSystemTimeZoneName();

    private:
        //! 时区名
        std::string name_;
        //! 相对于UTC的偏移量（秒）
        int32_t offset_ = 0;  
    };

} // namespace crazy
