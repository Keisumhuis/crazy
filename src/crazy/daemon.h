/**
 * @file daemon.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 守护进程管理.
 * @version 0.1
 * @date 2026-08-20
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace crazy {
	/**
	 * @brief 守护进程角色.
	 */
	enum class DaemonRole {
		//! 普通进程.
		none,
		//! 1号守护进程, 只负责检查和拉起worker.
		supervisor,
		//! 2号业务进程, 负责运行真实服务.
		worker,
	};

	/**
	 * @brief 守护进程命令行参数.
	 */
	struct DaemonArguments {
		//! 是否使用-d启用守护进程.
		bool enabled = false;
		//! 当前进程角色.
		DaemonRole role = DaemonRole::none;
		//! 业务命令行参数, 用于Application解析.
		std::vector<std::string> applicationArgs;
		//! 子进程命令行参数, 已移除守护进程内部参数.
		std::vector<std::string> childArgs;
	};

	/**
	 * @brief 守护进程管理.
	 */
	class Daemon final {
	public:
		/**
		 * @brief 解析守护进程参数.
		 */
		static DaemonArguments ParseArguments(int32_t argc, char** argv);
		/**
		 * @brief 启动1号守护进程.
		 */
		static bool StartSupervisor(const std::vector<std::string>& serviceArgs);
		/**
		 * @brief 运行1号守护进程循环.
		 */
		static int RunSupervisor(const std::vector<std::string>& serviceArgs);
	};
}
