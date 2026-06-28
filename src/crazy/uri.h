/**
 * @file uri.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief
 * @version 0.1
 * @date 2026-3-27
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <string>
#include <vector>

namespace crazy {
	/**
	 * @brief 统一资源标识符.
	 */
	class Uri final {
	public:
		/**
		 * @brief 构造函数.
		 */
		Uri();
		/**
		 * @brief 构造函数.
		 */
		explicit Uri(const char* uri, int32_t length);
		/**
		 * @brief 构造函数.
		 */
		explicit Uri(const std::string& uri);
		/**
		 * @brief 获取协议方案.
		 */
		std::string getScheme() const;
		/**
		 * @brief 设置协议方案.
		 */
		void setScheme(const std::string& scheme);
		/**
		 * @brief 获取主机名.
		 */
		std::string getHost() const;
		/**
		 * @brief 设置主机名.
		 */
		void setHost(const std::string& host);
		/**
		 * @brief 获取用户认证信息.
		 */
		std::string getUserInfo() const;
		/**
		 * @brief 设置用户认证信息.
		 */
		void setUserInfo(const std::string& userInfo);
		/**
		 * @brief 获取用户名.
		 */
		std::string getUserName() const;
		/**
		 * @brief 获取密码.
		 */
		std::string getPassword() const;
		/**
		 * @brief 获取路径.
		 */
		std::string getPath() const;
		/**
		 * @brief 设置路径.
		 */
		void setPath(const std::string& path);
		/**
		 * @brief 获取查询字符串.
		 */
		std::string getQuery() const;
		/**
		 * @brief 设置查询字符串.
		 */
		void setQuery(const std::string& query);
		/**
		 * @brief 获取片段标识符.
		 */
		std::string getFragment() const;
		/**
		 * @brief 设置片段标识符.
		 */
		void setFragment(const std::string& fragment);
		/**
		 * @brief 获取端口号.
		 */
		uint16_t getPort() const;
		/**
		 * @brief 设置端口号.
		 */
		void setPort(uint16_t port);
		/**
		 * @brief 添加查询参数.
		 */
		void addQueryParam(const std::string& key, const std::string& value);
		/**
		 * @brief 获取查询参数.
		 */
		std::string getQueryParam(const std::string& key) const;
		/**
		 * @brief 获取所有查询参数.
		 */
		std::vector<std::pair<std::string, std::string>> getQueryParams() const;
		/**
		 * @brief 清除所有查询参数.
		 */
		void clearQueryParams();
		/**
		 * @brief 转换为字符串.
		 */
		std::string toString() const;
		/**
		 * @brief 创建具有相同authority的新URI对象.
		 */
		Uri authority() const;
		/**
		 * @brief 获取此URI的路径、查询和片段部分.
		 */
		Uri resource() const;
		/**
		 * @brief 判断URI是否为空.
		 */
		bool isEmpty() const;
		/**
		 * @brief 判断是否为本机回环地址.
		 */
		bool isHostLoopback() const;
		/**
		 * @brief 判断是否为通配符主机.
		 */
		bool isHostWildcard() const;
		/**
		 * @brief 判断是否为可移植主机.
		 */
		bool isHostPortable() const;
		/**
		 * @brief 判断端口是否为默认端口.
		 */
		bool isPortDefault() const;
		/**
		 * @brief 判断是否为authority URI.
		 */
		bool isAuthority() const;
		/**
		 * @brief 判断路径是否为空.
		 */
		bool isPathEmpty() const;
		/**
		 * @brief 判断是否与另一个URI具有相同的authority.
		 */
		bool hasSameAuthority(const Uri& other) const;
		/**
		 * @brief 解析相对URI.
		 */
		std::string resolveUri(const std::string& relativeUri) const;
		/**
		 * @brief 解析相对URI.
		 */
		Uri resolve(const Uri& relativeUri) const;
		/**
		 * @brief 两个uri是否相等.
		 */
		bool operator==(const Uri& other) const;
		/**
		 * @brief 两个url是否不相对.
		 */
		bool operator!=(const Uri& other) const;
		/**
		 * @brief 当前uri是否小于other.
		 */
		bool operator<(const Uri& other) const;
		/**
		 * @brief 清空所有数据.
		 */
		void clear();

	protected:
		/**
		 * @brief 从字符串中解析uri.
		 */
		void parseFrom(const std::string& other);
		/**
		 * @brief 解析authority部分.
		 */
		void parseAuthority(const char* start, const char* end);
		/**
		 * @brief 解析查询参数.
		 */
		void parseQueryParams();
		/**
		 * @brief 构建查询字符串.
		 */
		void buildQueryString();
		/**
		 * @brief 判断是否为合法的scheme字符.
		 */
		static bool isSchemeChar(char c);
		/**
		 * @brief 判断是否为合法的authority字符.
		 */
		static bool isAuthorityChar(char c);
		/**
		 * @brief 判断是否为合法的user_info字符.
		 */
		static bool isUserInfoChar(char c);
		/**
		 * @brief 判断是否为合法的path字符.
		 */
		static bool isPathChar(char c);
		/**
		 * @brief 判断是否为合法的query字符.
		 */
		static bool isQueryChar(char c);
		/**
		 * @brief 判断是否为合法的fragment字符.
		 */
		static bool isFragmentChar(char c);
		/**
		 * @brief 简化路径，处理 . 和 .. 符号.
		 */
		static std::string simplifyPath(const std::string& path);

	private:
		//! 协议方案
		std::string scheme_;
		//! 主机名
		std::string host_;
		//! 用户认证信息
		std::string userInfo_;
		//! 路径
		std::string path_;
		//! 查询字符串
		std::string query_;
		//! 片段标识符
		std::string fragment_;
		//! 端口号
		uint16_t port_ = 0;
		//! 原始URI字符串
		std::string originalUri_;
		//! 查询参数列表
		std::vector<std::pair<std::string, std::string>> queryParams_;
		//! 查询参数是否需要重新构建
		bool queryParamsDirty_ = false;
	};
}
