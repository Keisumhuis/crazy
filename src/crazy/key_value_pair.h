/**
 * @file key_value_pair.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 键值对
 * @version 0.1
 * @date 01
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include <optional>
#include <unordered_map>

#include "crazy/date_time.h"

namespace crazy {
    /**
     * @brief 键值对.
     */
    class KeyValuePair final {
        using value_type = std::variant<uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t, double, std::string, DateTime>;
    public:
        enum class ValueType : int32_t {
            UINT8, UINT16, UINT32, UINT64, INT8, INT16, INT32, INT64, DOUBLE, STRING, DATE_TIME
        };
        /**
         * @brief 构造函数.
         */
        KeyValuePair() = default;
        /**
         * @brief 构造函数.
         */
        template <typename T>
        KeyValuePair(const std::string& key, const T& value)
            : key_(key), value_(value) {
            static_assert(
                std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t> ||
                std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
                std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t> ||
                std::is_same_v<T, float> || std::is_same_v<T, double> || 
                std::is_same_v<T, std::string> || std::is_same_v<T, DateTime>,
                "Unsupported type for KeyValuePair"
                );
        }
        /**
         * @brief 拷贝构造函数.
         */
        KeyValuePair(const KeyValuePair&) = default;
        /**
         * @brief 赋值运算符.
         */
        KeyValuePair& operator=(const KeyValuePair&) = default;
        /**
         * @brief 获取键名
         * @return 键名字符串
         */
        std::string key() const {
            return key_;
        }
        /**
         * @brief 设置键名
         * @param key 新的键名
         */
        void setKey(std::string key) {
            key_ = std::move(key);
        }
        /**
         * @brief 获取值（字符串形式）
         * @return 值的字符串表示
         */
        std::string value() const {
            return std::visit([](const auto& arg)-> std::string {
                using type = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<type, std::string>) {
                    return arg;
                }
                else if constexpr (std::is_same_v<type, DateTime>) {
                    // 创建副本调用非const方法
                    DateTime copy = arg;
                    return copy.toString();
                }
                else if constexpr (std::is_arithmetic_v<type>) {
                    return std::to_string(arg);
                }
                else {
                    // 永远不会到达这里
                    return "";
                }
                }, value_);
        }
        /**
         * @brief 设置值
         * @tparam type 值类型
         * @param value 新值
         */
        template<typename T>
        void setValue(const T& value) {
            static_assert(
                std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t> ||
                std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
                std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t> ||
                std::is_same_v<T, double> || std::is_same_v<T, std::string> ||
                std::is_same_v<T, DateTime>,
                "Unsupported type for KeyValuePair"
                );
            value_ = value;
        }

        /**
         * @brief 获取值类型
         */
        ValueType getValueType() const {
            return static_cast<ValueType>(value_.index());
        }

        /**
         * @brief 获取原始值（模板方法）
         */
        template<typename T>
        std::optional<T> get() const {
            if (auto* ptr = std::get_if<T>(&value_)) {
                return *ptr;
            }
            return std::nullopt;
        }

        /**
         * @brief 检查是否为特定类型
         */
        template<typename T>
        bool isType() const {
            return std::holds_alternative<T>(value_);
        }

        /**
         * @brief 获取字符串值的便捷方法
         */
        std::optional<std::string> getString() const {
            return get<std::string>();
        }

        /**
         * @brief 获取整型值的便捷方法
         */
        template<typename T>
        std::optional<T> getInteger() const {
            static_assert(std::is_integral_v<T>, "T must be integral type");
            return get<T>();
        }

        /**
         * @brief 获取浮点型值的便捷方法
         */
        template<typename T>
        std::optional<T> getFloat() const {
            static_assert(std::is_floating_point_v<T>, "T must be floating point type");
            return get<T>();
        }

        /**
         * @brief 获取DateTime值的便捷方法
         */
        std::optional<DateTime> getDateTime() const {
            return get<DateTime>();
        }

        /**
         * @brief 判断是否为空
         */
        bool empty() const {
            return key_.empty() || value_.valueless_by_exception();
        }

        /**
         * @brief 清空键值对
         */
        void clear() {
            key_.clear();
            value_ = std::string{};
        }

        /**
         * @brief 比较操作符
         */
        bool operator==(const KeyValuePair& other) const {
            return key_ == other.key_ && value_ == other.value_;
        }

        bool operator!=(const KeyValuePair& other) const {
            return !(*this == other);
        }

    private:
        //! 键
        std::string key_;
        //! 值
        value_type value_;
    };

    /**
     * key_value_pair 序列化为格式字符串.
     */
    inline std::string dataformat(const std::vector<KeyValuePair>& values, const std::string& delimiter = ":") {
        std::stringstream ss;
        for (auto row = values.cbegin(); row != values.cend(); ++row) {
            ss << row->key() << delimiter << row->value();
            if (std::next(row) != values.cend()) {
                ss << std::endl;
            }
        }
        return ss.str();
    }

    /**
     * key_value_pair 序列化为csv格式字符串.
     */
    inline std::string datacsv(const std::vector<KeyValuePair>& values, const std::string& delimiter = ",") {
        std::stringstream ss;
        for (auto row = values.begin(); row != values.end(); ) {
            ss << row->value();
            ++row;
            if (row != values.end()) {
                ss << delimiter;
            }
        }
        return ss.str();
    }

    /**
     * @brief key_value_pair 序列化为csv格式字符串.
     */
    inline std::string datacsv(const std::vector<std::vector<KeyValuePair>>& values) {
        std::stringstream ss;
        for (auto& rows : values) {
            ss << datacsv(rows) << std::endl;
        }
        return ss.str();
    }

    /**
     * @brief 转换为JSON格式
     */
    inline std::string toJson(const std::vector<KeyValuePair>& values, bool pretty = false) {
        std::stringstream ss;
        ss << "{";
        if (pretty) ss << "\n";

        for (size_t i = 0; i < values.size(); ++i) {
            if (pretty) ss << "  ";
            ss << "\"" << values[i].key() << "\": \"" << values[i].value() << "\"";
            if (i != values.size() - 1) {
                ss << ",";
            }
            if (pretty) ss << "\n";
        }

        ss << "}";
        return ss.str();
    }

    /**
     * @brief 转换为map
     */
    inline std::unordered_map<std::string, std::string> toMap(const std::vector<KeyValuePair>& values) {
        std::unordered_map<std::string, std::string> result;
        for (const auto& kv : values) {
            result[kv.key()] = kv.value();
        }
        return result;
    }

} // namespace crazy
