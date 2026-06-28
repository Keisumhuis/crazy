/**
 * @file socket_interface.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 套接字基类
 * @version 0.1
 * @date 16
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <atomic>
#include <memory>
#include <string>

#if _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <afunix.h>
using socket_t = SOCKET;
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
using socket_t = int32_t;
#define INVALID_SOCKET -1
#define closesocket ::close
#define WSAGetLastError() errno
#endif

namespace crazy {
	/**
	 * @brief 套接字基类.
	 */
	class SocketInterface {
	public:
		using ptr = std::shared_ptr<SocketInterface>;
		/**
		 * @brief 析构函数.
		 */
		virtual ~SocketInterface();
		/**
		 * @brief socket 是否已经就绪.
		 */
		bool active() const;
		/**
		 * @brief 关闭链接.
		 */
		void close();
		/**
		 * @brief 发送数据.
		 */
		virtual int32_t send(const void* buffer, size_t length, int32_t flags = 0);
		/**
		 * @brief 接收数据.
		 */
		virtual int32_t recv(void* buffer, size_t length, int32_t flags = 0);
		/**
		 * @brief 获取选项配置.
		 */
		bool getOption(int32_t level, int32_t option, void* result, socklen_t* len);
		/**
		 * @brief 获取选项配置.
		 */
		template <typename type>
		bool getOption(int32_t level, int32_t option, type& value);
		/**
		 * @brief 设置选项配置.
		 */
		bool setOption(int32_t level, int32_t option, void* result, socklen_t len);
		/**
		 * @brief 设置选项配置.
		 */
		template <typename type>
		bool setOption(int32_t level, int32_t option, type& value);
		/**
		 * @brief 获取当前socket
		 */
		socket_t socket() const;
		/**
		 * @brief 初始化socket.
		 */
		void initSocket(int32_t af, int32_t type, int32_t protocol);

	protected:
		//! socket fd
		socket_t socket_ = INVALID_SOCKET;
		//! socket 是否就绪
		std::atomic_bool active_ = false;
	};
	template <typename type>
	bool SocketInterface::getOption(int32_t level, int32_t option, type& value) {
		return getOption(level, option, &value, sizeof(type));
	}
	template <typename type>
	bool SocketInterface::setOption(int32_t level, int32_t option, type& value) {
		return setOption(level, option, &value, sizeof(type));
	}
}
