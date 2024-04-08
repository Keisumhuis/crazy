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

	const uint64_t GetCurrentSS();
	const uint64_t GetCurrentMS();
	const uint64_t GetCurrentUS();
	
	const uint64_t GetCoroutineId();
	const uint64_t GetThreadId();
}

#endif // ! ____CRAZY_UTIL_H____
