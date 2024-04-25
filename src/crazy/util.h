/**
 * @file util.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_UTIL_H____
#define ____CRAZY_UTIL_H____

#include <stdarg.h>
#include <string.h>

#include <chrono>
#include <string>

#include "lexicl_cast.h"

namespace crazy {
	
	template<class MapType, class type>
	bool CheckGetAs(const MapType& m, const std::string& key, type& val, const type& def = type{}) {
    		auto it = m.find(key);
    		if(it == m.end()) {
        		val = def;
        		return false;
    		}
    		try {
        		val = lexical_cast<type>(it->second);
        		return true;
    		} catch (...) {
        		val = def;
    		}
    		return false;
	}

	template<class MapType, class type>
	type GetAs(const MapType& m, const std::string& key, const type& def = type{}) {
    		auto it = m.find(key);
    		if(it == m.end()) {
        		return def;
    		}
    		try {
        		return lexical_cast<type>(it->second);
    		} catch (...) {
  		}
    		return def;
	}

	struct CaseInsensitiveLess {
		bool operator()(const std::string& lhs, const std::string& rhs) const {
			return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
		}
	};

	template<class V, class Map, class K>
	V GetParamValue(const Map& m, const K& k, const V& def = V()) {
    	auto it = m.find(k);
    	if(it == m.end()) {
        	return def;
    	}
    	try {
        	return lexical_cast<V>(it->second);
    	} catch (...) {
    	}
    	return def;
	}

	const uint64_t GetCurrentSS();
	const uint64_t GetCurrentMS();
	const uint64_t GetCurrentUS();
	
	const uint64_t GetCoroutineId();
	const uint64_t GetThreadId();
	std::string ToUpper(const std::string& name);
	std::string ToLower(const std::string& name);
	std::string Time2Str(time_t ts = time(0), const std::string& format = "%Y-%m-%d %H:%M:%S");
	time_t Str2Time(const char* str, const char* format = "%Y-%m-%d %H:%M:%S");
	
	class StringUtil {
	public:
    	static std::string Format(const char* fmt, ...);
    	static std::string Formatv(const char* fmt, va_list ap);

    	static std::string UrlEncode(const std::string& str, bool space_as_plus = true);
    	static std::string UrlDecode(const std::string& str, bool space_as_plus = true);

    	static std::string Trim(const std::string& str, const std::string& delimit = " \t\r\n");
    	static std::string TrimLeft(const std::string& str, const std::string& delimit = " \t\r\n");
    	static std::string TrimRight(const std::string& str, const std::string& delimit = " \t\r\n");


    	static std::string WStringToString(const std::wstring& ws);
    	static std::wstring StringToWString(const std::string& s);
	};
}

#endif // ! ____CRAZY_UTIL_H____
