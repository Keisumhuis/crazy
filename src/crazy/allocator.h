/**
 * @file ascii_logo.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_ALLOCATOR_H____
#define ____CRAZY_ALLOCATOR_H____

#include <stdlib.h>

#include "assert.h"
#include "noncopyable.h"

namespace crazy {

	class Allocator final : Noncopyable {
	public:
		static void* Alloc(size_t size) {
			auto ptr = malloc (size);
			if (!ptr) {
				CRAZY_ASSERT_WITH_LOG(false
						, "malloc fail");
			}
			return ptr;
		}
		static void Dealloc(void* ptr, size_t size = 0) {
			return free(ptr);
		}	
	};

}

#endif // ! ____CRAZY_ALLOCATOR_H____

