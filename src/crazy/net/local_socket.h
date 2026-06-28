/**
 * @file local_socket.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 本地套接字
 * @version 0.1
 * @date 23
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include "crazy/net/socket_interface.h"

namespace crazy {
	/**
	 * @brief 本地套接字.
	 */
	class LocalSocket : public SocketInterface {
	public:
		using ptr = std::shared_ptr<LocalSocket>;
		/**
		 * @brief 构造函数.
		 */
		explicit LocalSocket();
		/**
		 * @brief 析构函数.
		 */
		virtual ~LocalSocket() = default;
		/**
		 * @brief 接受一个链接.
		 */
		LocalSocket::ptr accept();
		/**
		 * @brief 监听地址.
		 */
		bool listen(const std::string& address);
		/**
		 * @brief 链接地址.
		 */
		bool connect(const std::string& address);
	};
}
