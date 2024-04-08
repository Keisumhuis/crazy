/**
 * @file format.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_FORMAT_H____
#define ____CRAZY_FORMAT_H____

#include <exception>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace crazy::fmt {

	template<size_t k, typename Tuple, std::enable_if_t<(k >= std::tuple_size_v<Tuple>), int32_t> = 0>
	auto GetArgByIndex(std::stringstream& ss, size_t index, Tuple& tp) {
		throw std::logic_error("arg index out of range");
	}

	template<size_t k, typename Tuple, std::enable_if_t<(k < std::tuple_size_v<Tuple>), int32_t> = 0>
	auto GetArgByIndex(std::stringstream& ss, size_t index, Tuple& tp) {
		if (k == index) {
			ss << std::get<k>(tp);
		} else {
			return GetArgByIndex<k + 1>(ss, index, tp);
		}
	}

	template <typename ...Args>
	static const std::string format_impl(const char* fmt, Args&&...args);
	template <typename ...Args>
	static const std::string format(const std::string& fmt, Args&&...args) {
		return format_impl(fmt.c_str(), std::forward<Args...>(args...));
	}
	template <typename ...Args>
	static const std::string format(const std::string_view fmt, Args&&...args) {
		return format_impl(fmt.data(), std::forward<Args...>(args...));
	}
	template <typename ...Args>
	static const std::string format(const char* fmt, Args&&...args) {
		return format_impl(fmt, std::forward<Args...>(args...));
	}
	template <typename ...Args>
	static const std::string format_impl(const char* fmt, Args&&...args) {
		std::stringstream ss;
		size_t index = 0;
		bool isPlaceholder = false;
		auto tp = std::tuple<Args...>(args...);
		auto pos = fmt;
		while (*pos != '\0') {
			if (*pos == '{') {
				GetArgByIndex<0>(ss, index++, tp);
				isPlaceholder = true;
			}
			else if (*pos == '}') {
				isPlaceholder = false;
			}
			else if (!isPlaceholder) {
				ss << *pos;
			}
			++pos;
		}
		return { ss.str() };
	}

}

#endif // ! ____CRAZY_FORMAT_H____

