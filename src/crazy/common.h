/**
 * @file common.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 内部路由字段
 * @version 0.1
 * @date 21
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <stdint.h>

namespace crazy {
	/**
	 * 内部路由字段.
	 */
	enum InternalCommand : uint64_t {
		//! 心跳包
		heartbeat = 0,
		//! 命令行请求
		command_line_request = 1,
		//! 命令行应答
		command_line_response = 2,
		//! 与服务器建立上链接
		command_connected_service = 3,
		//! 与服务器断开链接
		command_disconnected_service = 4,
		//! 客户端链接上服务
		command_client_connected = 5,
		//! 客户端断开链接服务
		command_client_disconnected = 6,
		//! 自定义内部消息
		command_custom_internal = 7,
		//! 自定义路由字段
		custom_command_begin = 100,
	};
}
