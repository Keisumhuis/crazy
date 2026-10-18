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
#include <netdb.h>
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
	 * @brief 确保套接字运行时已经初始化（进程级、幂等、线程安全）.
	 * @return 初始化成功返回 `true`；非 Windows 平台恒为 `true`.
	 */
	bool ensureWinSock();
	/**
	 * @brief 把套接字错误码转成可读文本（Windows 走系统消息表，POSIX 走 `strerror`）.
	 * @param error `WSAGetLastError()`（POSIX 上即 `errno`）返回的错误码.
	 */
	std::string socketErrorText(int32_t error);
	/**
	 * @brief 错误码是否表示「暂时没有数据 / 暂时写不出去」（非阻塞模式下的正常返回）.
	 * @param error `WSAGetLastError()`（POSIX 上即 `errno`）返回的错误码.
	 */
	bool socketWouldBlock(int32_t error);
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
		 * @note 虚函数：TLS 套接字需要先发送 close_notify 再关闭底层 socket.
		 */
		virtual void close();
		/**
		 * @brief 发送数据.
		 */
		virtual int32_t send(const void* buffer, size_t length, int32_t flags = 0);
		/**
		 * @brief 接收数据.
		 */
		virtual int32_t recv(void* buffer, size_t length, int32_t flags = 0);
		/**
		 * @brief 传输层已解密但尚未被读走的字节数.
		 * @note 明文 TCP 恒为 0；TLS 下对应 `SSL_pending`，用于排空 TLS 层缓冲.
		 */
		virtual int32_t pending() const;
		/**
		 * @brief 上一次 send/recv 是否因需要等待可读而未能推进（TLS 的 WANT_READ）.
		 */
		virtual bool wantRead() const;
		/**
		 * @brief 上一次 send/recv 是否因需要等待可写而未能推进（TLS 的 WANT_WRITE）.
		 */
		virtual bool wantWrite() const;
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
		 * @brief 设置非阻塞模式.
		 * @param enabled true 设为非阻塞，false 设回阻塞.
		 * @return 设置成功返回 true.
		 * @note 事件驱动用法：`connectAsync()` 之后由 `onReadable()` / `onWritable()` 收发.
		 */
		bool setNonBlocking(bool enabled = true);
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
