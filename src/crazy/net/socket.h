/**
 * @file socket.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief IPV4套接字类
 * @version 0.1
 * @date 16
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include "socket_interface.h"

namespace crazy {
	/**
	 * @brief IPV4套接字类.
	 */
	class Socket : public SocketInterface {
	public:
		using ptr = std::shared_ptr<Socket>;
		/**
		 * @brief 构造函数.
		 */
		Socket();
		/**
		 * @brief 析构函数.
		 */
		virtual ~Socket() = default;
		/**
		 * @brief 接受一个链接.
		 */
		Socket::ptr accept();
		/**
		 * @brief 监听地址.
		 */
		bool listen(uint64_t port, const std::string& address = "0.0.0.0");
		/**
		 * @brief 链接地址.
		 */
		bool connect(const std::string& address, uint64_t port);
		/**
		 * @brief 获取对端地址.
		 */
		const std::string& remoteAddress() const;
		/**
		 * @brief 获取本端地址.
		 */
		const std::string& localAddress() const;;

	protected:
		/**
		 * @brief 初始化socket.
		 */
		void initSocket(int32_t af, int32_t type, int32_t protocol);
		/**
		 * @brief 设置本端地址.
		 */
		void updateLocalAddress();
		/**
		 * @brief 设置对端地址.
		 */
		void updateRemoteAddress();
		/**
		 * @brief 地址转换.
		 */
		std::string sockaddr2string(const sockaddr_in& address);

	private:
		//! 对端地址
		std::string remoteAddress_;
		//! 本端地址
		std::string localAddress_;
	};
}
