/**
 * @file singleton.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief 单例模板
 * @version 0.1
 * @date 2025-08-31
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once
#pragma managed(push, off)
namespace crazy {
	/**
	 * @brief 单例模板.
	 */
	template <typename type>
	class Singleton {
	public:
		static type& Instance() {
			static type t;
			return t;
		}

	protected:
		Singleton() = default;
		virtual ~Singleton() = default;
	};
}
#pragma managed(pop)
