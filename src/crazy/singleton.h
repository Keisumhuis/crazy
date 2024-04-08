/**
 * @file singleton.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_SINGLETON_H____
#define ____CRAZY_SINGLETON_H____

#include <memory>

#include "noncopyable.h"

namespace crazy {

	template <typename type>
	class Singleton : public Noncopyable {
	public:
		static type& GetInstance() {
			static type t;
			return t;
		}
	};

	template <typename type>
	class SingletonPtr : public Noncopyable {
	public:
		static std::shared_ptr<type> GetInstance(){
			static std::shared_ptr<type> ptr (new type);
			return ptr;
		}
	};

}

#endif // ! ____CRAZY_SINGLETON_H____
