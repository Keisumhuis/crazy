/**
 * @file socket.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief TCP套接字类（默认 IPv4，支持 IPv6 与双栈）
 * @version 0.1
 * @date 16
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include "socket_interface.h"

namespace crazy {
	/**
	 * @brief TCP套接字类.
	 * @note 默认为「仅 IPv4」，与既有行为一致；IPv6 字面量地址（`::1`、`::`、`[::1]`）
	 * 自动使用 `AF_INET6`，双栈必须由 `listen()` 的 `dualStack` 参数显式开启。
	 */
	class Socket : public SocketInterface {
	public:
		using ptr = std::shared_ptr<Socket>;
		/**
		 * @brief 构造函数.
		 */
		Socket();
		/**
		 * @brief 构造函数.
		 */
		explicit Socket(socket_t acceptedSocket);
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
		bool listen(uint64_t port, const std::string& address = "0.0.0.0", bool dualStack = false);
		/**
		 * @brief 链接地址.
		 * @note IPv6 字面量（可带方括号）走 `AF_INET6`；非字面量主机名继续只产出 IPv4 候选.
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
		 * @brief 构造 accept 出来的客户端 socket.
		 */
		virtual Socket::ptr createAcceptedSocket(socket_t acceptedSocket);
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
		std::string sockaddr2string(const sockaddr* address);

	private:
		//! 对端地址
		std::string remoteAddress_;
		//! 本端地址
		std::string localAddress_;
	};
}
