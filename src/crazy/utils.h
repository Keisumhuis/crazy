/**
 * @file utils.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief 基础工具
 * @version 0.1
 * @date 2025-08-31
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace crazy {
	/**
	 * @brief 获取当前时间戳（秒）.
	 * @return const uint64_t 当前时间戳
	 */
    const uint64_t GetCurrentSS();
	/**
	 * @brief 获取当前时间戳（毫秒）.
	 * @return const uint64_t 当前时间戳
	 */
	const uint64_t GetCurrentMS();
	/**
	 * @brief 获取当前时间戳（微妙）.
	 * @return const uint64_t 当前时间戳
	 */
	const uint64_t GetCurrentUS();
	/**
	 * @brief 获取当前时间戳（纳秒）.
	 * @return const uint64_t 当前时间戳
	 */
    const uint64_t GetCurrentNS();
	/**
	 * @brief 创建uuid
	 * @return uuid
	 */
	const std::string CreateUUID();
	/**
	 * @brief 字符串处理工具类.
	 */
	class StringUtil {
	public:
		/**
		 * @brief 原地去除字符串首尾的指定字符.
		 */
		static std::string Trim(const std::string& str, const std::string& delimit = " \t\r\n");
		/**
		 * @brief 去除字符串左侧的指定字符.
		 */
		static std::string TrimLeft(const std::string& str, const std::string& delimit = " \t\r\n");
		/**
		 * @brief 去除字符串右侧的指定字符.
		 */
		static std::string TrimRight(const std::string& str, const std::string& delimit = " \t\r\n");
		/**
		 * @brief 按分隔符分割字符串（字符串分隔符）
		 * @param str 原始字符串
		 * @param delimiter 分隔字符串
		 * @return 分割后的字符串数组
		 */
		static std::vector<std::string> Split(const std::string& str, const std::string& delimiter = "--");
		/**
		 * @brief 转换成大写字符串.
		 */
		static std::string ToUpper(const std::string& str);
		/**
		 * @brief 转换成小写字符串.
		 */
		static std::string ToLower(const std::string& str);
	};

	/**
	 * @brief 路径工具类.
	 */
	class PathUtil {
	public:
		/**
		 * @brief 获取当前可执行文件的完整路径
		 */
		static std::string GetExecutablePath();
		/**
		 * @brief 获取当前可执行文件名字.
		 */
		static std::string GetExecutableName();
		/**
		 * @brief 获取当前可执行文件所在的目录
		 */
		static std::string GetExecutableDirectory();
		/**
		 * @brief 获取当前工作目录
		 */
		static std::string GetCurrentWorkingDirectory();
		/**
		 * @brief 设置当前工作目录
		 */
		static bool SetCurrentWorkingDirectory(const std::string& path);
		/**
		 * @brief 获取临时目录
		 */
		static std::string GetTempDirectory();
		/**
		 * @brief 检查路径是否存在
		 */
		static bool PathExists(const std::string& path);
		/**
		 * @brief 检查是否是绝对路径
		 */
		static bool IsAbsolutePath(const std::string& path);
		/**
		 * @brief 连接路径
		 */
		static std::string JoinPath(const std::string& path1, const std::string& path2);
		/**
		 * @brief 获取路径的文件名部分
		 */
		static std::string GetFileName(const std::string& path);
		/**
		 * @brief 获取路径的目录部分
		 */
		static std::string GetDirectoryName(const std::string& path);
		/**
		 * @brief 获取文件扩展名
		 */
		static std::string GetFileExtension(const std::string& path);
		/**
		 * @brief 移除文件扩展名
		 */
		static std::string RemoveFileExtension(const std::string& path);
		/**
		 * @brief 创建单个目录
		 */
		static bool CreateDir(const std::string& path, bool create_parents = false);
		/**
		 * @brief 递归创建目录（包含所有父目录）
		 */
		static bool CreateDirectories(const std::string& path);
		/**
		 * @brief 删除空目录
		 */
		static bool RemoveDir(const std::string& path);
		/**
		 * @brief 递归删除目录及其所有内容
		 */
		static bool RemoveDirectoryRecursive(const std::string& path);
		/**
		 * @brief 检查路径是否是目录
		 */
		static bool IsDirectory(const std::string& path);
		/**
		 * @brief 检查路径是否是文件
		 */
		static bool IsFile(const std::string& path);
		/**
		 * @brief 列出目录内容
		 */
		static std::vector<std::string> ListDirectory(const std::string& path, bool recursive = false);
		/**
		 * @brief 创建临时目录
		 */
		static std::string CreateTempDirectory(const std::string& prefix = "tmp");
		/**
		 * @brief 获取目录可用空间（字节）
		 */
		static int64_t GetAvailableSpace(const std::string& path);
		/**
		 * @brief 复制目录
		 */
		static bool CopyDirectory(const std::string& source, const std::string& destination);
	};
}
