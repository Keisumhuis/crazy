/**
 * @file config.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 配置文件管理类
 * @version 0.1
 * @date 01
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "crazy/nocopyable.h"

namespace crazy {
	//! 配置文件全局默认key
	const std::string CONFIG_GLOBAL_SECTION = "global";
	/**
	 * @brief 配置文件管理类.
	 */
	class Config final : public Noncopyable {
	public:
		using ConfigValueMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
		/**
		 * @brief 加载指定目录下的所有配置文件.
		 * @param path 配置文件路径
		 * @param suffix 配置文件后缀
		 */
		static void LoadConfigPath(const std::string& path, const std::string& suffix = ".ini");
		/**
		 * @brief 加载指定目录下的所有配置文件.
		 * @param path 配置文件路径
		 * @param suffix 配置文件后缀
		 */
		static void LoadConfigPath(const std::vector<std::string>& path, const std::string& suffix = ".ini");
		/**
		 * @brief 加载指配置文件.
		 * @param filePath 配置文件路径
		 */
		static void LoadConfigFile(const std::string& filePath);
		/**
		 * @brief 获取配置文件中boolean参数值.
		 * @param section 部分
		 * @param key 键
		 */
		static bool GetBoolean(const std::string& section, const std::string& key, const bool& defaultValue = bool{});
		/**
		 * @brief 获取配置文件中intager参数值.
		 * @param section 部分
		 * @param key 键
		 */
		static int64_t GetIntager(const std::string& section, const std::string& key, const int64_t& defaultValue = {});
		/**
		 * @brief 获取配置文件中double参数值.
		 * @param section 部分
		 * @param key 键
		 */
		static double GetDouble(const std::string& section, const std::string& key, const double& defaultValue = {});
		/**
		 * @brief 获取配置文件中string参数值.
		 * @param section 部分
		 * @param key 键
		 */
		static std::string GetString(const std::string& section, const std::string& key, const std::string& defaultValue = "");
		/**
		 * @brief 删除配置项.
		 * @param section 部分
		 * @param key 键
		 */
		static void EreaseValue(const std::string& section, const std::string& key);
		/**
		 * @brief 获取是否有配置项.
		 * @param section 部分
		 */
		static bool HasSession(const std::string& section);

	private:
		/**
		 * @brief 解析配置文件.
		 * @param filePath 配置文件路径
		 */
		static void ParseConfigFile(const std::string& filePath);
		/**
		 * @brief 获取全部配置的值.
		 * @return ConfigValueMap& 全部配置的值
		 */
		static ConfigValueMap& GetConfigValueMap();
	};
}
