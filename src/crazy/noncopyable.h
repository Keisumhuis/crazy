/**
 * @file noncopyable.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_NONCOPYABLE_H____
#define ____CRAZY_NONCOPYABLE_H____

namespace crazy {

	class Noncopyable {
	public:
		Noncopyable() = default;
		~Noncopyable() = default;
		Noncopyable(const Noncopyable&) = delete;
		Noncopyable& operator=(const Noncopyable&) = delete;
	};
}

#endif // ! ____CRAZY_NONCOPYABLE_H____

