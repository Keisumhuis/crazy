/**
 * @file http_header.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 头部容器.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <map>
#include <optional>
#include <cstddef>
#include <string>

#include "crazy/utils.h"

namespace crazy {
	/**
	 * @brief HTTP 头部容器，大小写不敏感.
	 */
	class HttpHeader final {
	public:
		//! 底层容器类型（multimap 支持重复头字段，如多个 Set-Cookie）
		using container_type = std::multimap<std::string, std::string>;
		//! 底层容器的键值对类型
		using value_type = container_type::value_type;
		//! 迭代器类型
		using iterator = container_type::iterator;
		//! 常量迭代器类型	
		using const_iterator = container_type::const_iterator;
		/**
		 * @brief 插入一个头字段（允许重复 key）.
		 */
		bool insert(const std::string& key, const std::string& value);
		/**
		 * @brief 查询头字段值.
		 */
		std::optional<std::string> get(const std::string& key) const;
		/**
		 * @brief 判断头字段是否存在..
		 */
		bool contains(const std::string& key) const;
		/**
		 * @brief 删除头字段（删除该 key 的所有值）.
		 */
		bool erase(const std::string& key);
		/**
		 * @brief 清空所有头字段.
		 */
		void clear();
		/**
		 * @brief 头字段数量.
		 */
		std::size_t size() const;
		/**
		 * @brief 是否为空.
		 */
		bool empty() const;
		/**
		 * @brief 迭代器开始.
		 */
		iterator begin();
		/**
		 * @brief 迭代器结束.
		 */
		iterator end();
		/**
		 * @brief 迭代器开始（常量）.
		 */
		const_iterator begin() const;
		/**
		 * @brief 迭代器结束（常量）.
		 */
		const_iterator end() const;
		/**
		 * @brief 迭代器开始（常量）.
		 */
		const_iterator cbegin() const;
		/**
		 * @brief 迭代器结束（常量）.
		 */
		const_iterator cend() const;
		/**
		 * @brief 直接访问底层容器.
		 */
		container_type& data();
		/**
		 * @brief 直接访问底层容器（常量）.
		 */
		const container_type& data() const;
		/**
		 * @brief 查找头字段.
		 */
		iterator find(const std::string& key);
		/**
		 * @brief 查找头字段（常量）.
		 */
		const_iterator find(const std::string& key) const;

	protected:
		/**
		 * @brief 规范化头字段键（大小写不敏感）.
		 */
		static std::string normalizeKey(const std::string& key);

	private:
		//! 底层头字段容器
		container_type headers_;
	};
}
