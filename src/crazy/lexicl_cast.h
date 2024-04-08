/**
 * @file lexical_cast.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_LEXICAL_H____
#define ____CRAZY_LEXICAL_H____

#include <cstdlib>
#include <iostream>
#include <stdint.h>
#include <string.h>

#include <type_traits>
#include <string>

namespace crazy {

	constexpr char* strue = "true";
	constexpr char* sfalse = "false";

	template <typename to, typename from>
	struct Converter {};

	template <>
	struct Converter<bool, std::string> {
		static bool converter(const std::string& value) {
			return value == strue ? true : false;
		}
	};

	template <>
	struct Converter<bool, char*> {
		static bool converter(const char* value) {
			if (4 == strlen(value) && !strcmp(value, strue)) {
				return true;
			}
			return false;
		}
	};
	
	template <size_t N>
	struct Converter <bool, char[N]> {
		static bool converter(const char(&value)[N]) {
			if(5 == N && !strcmp(value, strue)) {
				return true;
			}
			return false;
		}
	};

	template <typename from>
	struct Converter <int16_t, from> {
		static int16_t converter(const std::string& value) {
			return std::stoi(value);
		}

		static int16_t converter(const char* value) {
			return std::atoi(value);
		}
	};

	template <typename from>
	struct Converter <uint16_t, from> {
		static uint16_t converter(const std::string& value) {
			return std::stoi(value);
		}

		static uint16_t converter(const char* value) {
			return std::atoi(value);
		}
	};

	template <typename from>
	struct Converter <int32_t, from> {
		static int32_t converter(const std::string& value) {
			return std::stoi(value);
		}

		static int32_t converter(const char* value) {
			return std::atoi(value);
		}
	};

	template <typename from>
	struct Converter <uint32_t, from> {
		static uint32_t converter(const std::string& value) {
			return std::stoi(value);
		}

		static uint32_t converter(const char* value) {
			return std::atoi(value);
		}
	};

	template <typename from>
	struct Converter <int64_t, from> {
		static int64_t converter(const std::string& value) {
			return std::stoll(value);
		}

		static int64_t converter(const char* value) {
			return std::atoll(value);
		}
	};
	
	template <typename from>
	struct Converter <uint64_t, from> {
		static uint64_t converter(const std::string& value) {
			return std::stoll(value);
		}

		static uint64_t converter(const char* value) {
			return std::atoll(value);
		}
	};

	template <typename from>
	struct Converter <float, from> {
		static float converter(const std::string& value) {
			return std::stof(value);
		}

		static float converter(const char* value) {
			return std::atof(value);
		}
	};

	template <typename from>
	struct Converter <double, from> {
		static double converter(const std::string& value) {
			return std::stod(value);
		}

		static double converter(const char* value) {
			return std::stod(value);
		}
	};
	
	template <typename from>
	struct Converter <std::string, from> {
		static std::string converter(const from& value) {
			return std::to_string(value);
		}
	};

	template <typename to, typename from, std::enable_if_t<!std::is_same_v<from, to>, int32_t> = 0>
	to lexical_cast(const from& value) {
		return Converter<to, from>::converter(value);
	}
	
	template <typename to, typename from, std::enable_if_t<std::is_same_v<from, to>, int32_t> = 0>
	to lexical_cast(const from& value) {
		return value;
	}

}

#endif // ! ____CRAZY_LEXICAL_H____
