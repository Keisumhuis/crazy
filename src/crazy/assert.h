/**
 * @file assert.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_ASSERT_H____
#define ____CRAZY_ASSERT_H____

#include <assert.h>
#include <execinfo.h>

#include <iostream>
#include <string>
#include <vector>

#include "logger.h"

#define CRAZY_ASSERT(x) 					\
	if (!(x)) {						\
		auto backTrace = crazy::Execinfo::BackTrace(); 	\
		for (auto & itBackTrace : backTrace) {		\
			std::cout << itBackTrace << std::endl;	\
		}						\
		assert((x));					\
	}	

#define CRAZY_ASSERT_WITH_LOG(x, w) 				\
	if (!(x)) {						\
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << w;		\
		auto backTrace = crazy::Execinfo::BackTrace(); 	\
		for (auto & itBackTrace : backTrace) {		\
			std::cout << itBackTrace << std::endl;	\
		}						\
		assert((x));					\
	}

namespace crazy {

	class Execinfo final {
	public:
		static std::vector<std::string> BackTrace(const size_t& size = 64, const size_t skip = 2) {
			void **backTrace = (void**)malloc(sizeof(void*) * size);
			auto ret = backtrace(backTrace, size);

			char **symbols = backtrace_symbols(backTrace, ret);
			if (!symbols) {
				free(backTrace);
				return {};
			}

			std::vector<std::string> rtBackTrace;
			for (size_t i = skip; i < ret; ++i) {
				rtBackTrace.push_back(symbols[i]);
			}
			free(symbols);
			free(backTrace);
			return rtBackTrace;
		}
		
	};

}

#endif // ! ____CRAZY_ASSERT_H____
