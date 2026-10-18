/**
 * @file tls.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief TLS 传输支持（TlsContext 上下文 + TlsSocket 套接字）
 * @version 0.1
 * @date 2026-10-09
 *
 * @note 本文件在 `CRAZY_ENABLE_TLS=OFF`（默认）时依然可被包含：此时
 * `TlsContext::createServer/createClient` 一律返回 `nullptr` 并给出
 * 「TLS 支持未编译」的错误原因，**绝不静默降级为明文**。
 * @note OpenSSL 类型只做前置声明，`tls.h` 的包含者不需要 OpenSSL 头文件。
 * @copyright Copyright (c) 2026
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "crazy/net/socket.h"

// OpenSSL 类型前置声明（避免把 OpenSSL 头文件泄漏给包含者）
struct ssl_ctx_st;
struct ssl_st;

namespace crazy {
	/**
	 * @brief TLS 上下文参数.
	 */
	struct TlsOptions {
		//! 最低协议版本，只接受 "1.2" / "1.3"
		std::string minVersion = "1.2";
		//! 最高协议版本，空表示不限制（只接受空 或 "1.2" / "1.3"）
		std::string maxVersion;
		//! 服务端证书链 PEM 路径
		std::string certFile;
		//! 服务端私钥 PEM 路径
		std::string keyFile;
		//! 客户端信任的 CA PEM 路径（空表示 OpenSSL 默认信任库 + 随包 CA bundle）
		std::string caFile;
		//! 是否校验对端证书（客户端默认 `true`，服务端默认 `false`）
		bool verifyPeer = false;
	};

	/**
	 * @brief TLS 上下文.
	 * @note 持有 `SSL_CTX`；创建失败返回 `nullptr` 并把可读原因写入 `error`，
	 * 不抛异常、不静默降级。
	 */
	class TlsContext {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<TlsContext>;
		/**
		 * @brief 角色.
		 */
		enum class Role : uint8_t {
			server,  //! 服务端
			client,  //! 客户端
		};
		/**
		 * @brief 析构函数.
		 */
		~TlsContext();
		/**
		 * @brief 创建服务端上下文.
		 * @param options 参数，`certFile` 与 `keyFile` 必须非空且可读.
		 * @param error 失败原因（含具体文件路径）.
		 */
		static TlsContext::ptr createServer(const TlsOptions& options, std::string& error);
		/**
		 * @brief 创建客户端上下文.
		 * @param options 参数，`verifyPeer` 为 `true` 时校验对端证书.
		 * @param error 失败原因（含具体文件路径）.
		 * @note 信任库优先级：① `caFile` 非空 → **只用它**，不叠加随包 CA bundle；
		 * ② `caFile` 为空 → OpenSSL 默认验证路径（`SSL_CERT_FILE`/`SSL_CERT_DIR`）
		 * **加上**随框架编译进来的 CA bundle（见 `crazy/net/ca_bundle.h`）；
		 * 但若用户用 `SSL_CERT_FILE`/`SSL_CERT_DIR` 显式指定了信任库，则**不再叠加**
		 * 随包 bundle，以尊重「只信私有 CA」的显式选择.
		 */
		static TlsContext::ptr createClient(const TlsOptions& options, std::string& error);
		/**
		 * @brief 获取角色.
		 */
		Role role() const;
		/**
		 * @brief 获取最低协议版本.
		 */
		const std::string& minVersion() const;
		/**
		 * @brief 获取最高协议版本.
		 */
		const std::string& maxVersion() const;
		/**
		 * @brief 是否校验对端证书.
		 */
		bool verifyPeer() const;
		/**
		 * @brief 获取底层 `SSL_CTX`（供 `TlsSocket` 使用）.
		 */
		struct ssl_ctx_st* native() const;

	private:
		TlsContext() = default;
		/**
		 * @brief 释放上下文.
		 */
		void release();

	private:
		//! 底层 SSL_CTX
		struct ssl_ctx_st* ctx_ = nullptr;
		//! 角色
		Role role_ = Role::server;
		//! 最低协议版本
		std::string minVersion_ = "1.2";
		//! 最高协议版本
		std::string maxVersion_;
		//! 是否校验对端证书
		bool verifyPeer_ = false;
	};

	/**
	 * @brief TLS 套接字.
	 * @note 直接继承 `Socket`，覆写 `send`/`recv`/`close`/`pending`/`wantRead`/`wantWrite`，
	 * 上层（`HttpSession` 等）不需要感知 TLS。
	 */
	class TlsSocket : public Socket {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<TlsSocket>;
		/**
		 * @brief 角色（本端）.
		 */
		enum class Side : uint8_t {
			server,  //! 服务端（执行 SSL_accept）
			client,  //! 客户端（执行 SSL_connect）
		};
		/**
		 * @brief 构造函数（自行建立/连接底层 socket）.
		 */
		TlsSocket(TlsContext::ptr context, Side side);
		/**
		 * @brief 构造函数（接管已经 accept 出来的 socket）.
		 */
		TlsSocket(socket_t acceptedSocket, TlsContext::ptr context, Side side);
		/**
		 * @brief 析构函数.
		 */
		~TlsSocket() override;
		/**
		 * @brief 执行 TLS 握手（阻塞，受握手超时约束）.
		 * @param error 失败原因.
		 * @return 握手成功返回 `true`.
		 */
		bool handshake(std::string& error);
		/**
		 * @brief 握手是否已经完成.
		 */
		bool handshakeDone() const;
		/**
		 * @brief 设置握手超时（毫秒），默认 5000.
		 * @note 握手期间在底层 socket 上设置 `SO_RCVTIMEO`/`SO_SNDTIMEO`，
		 * 握手结束后恢复为无限等待。这是本 REQ 对「握手期阻塞 DoS」的最小防护。
		 */
		void setHandshakeTimeout(uint32_t milliseconds);
		/**
		 * @brief 设置需要校验的对端主机名或 IP 字面量（仅客户端）.
		 */
		void setVerifyHost(const std::string& host);
		/**
		 * @brief 握手协商出的协议版本（未握手时为空）.
		 */
		const std::string& negotiatedVersion() const;
		/**
		 * @brief 握手协商出的密码套件（未握手时为空）.
		 */
		const std::string& negotiatedCipher() const;
		/**
		 * @brief 发送数据.
		 * @note 「要么全部收下、要么返回错误」：不会把 `SSL_write` 的部分写入
		 * 暴露成与 `::send` 相同的字节偏移语义（失败时不消耗任何明文）。
		 */
		int32_t send(const void* buffer, size_t length, int32_t flags = 0) override;
		/**
		 * @brief 接收数据.
		 * @return `> 0` 解密后的明文字节数；`0` 收到 `close_notify`（正常关闭）；
		 * `< 0` 错误（含连接被截断）；因 `WANT_READ`/`WANT_WRITE` 需要稍后再来时
		 * 返回 `< 0` 且 `errno == EAGAIN`，并由 `wantRead()`/`wantWrite()` 给出方向。
		 */
		int32_t recv(void* buffer, size_t length, int32_t flags = 0) override;
		/**
		 * @brief TLS 层已解密但尚未被读走的字节数.
		 */
		int32_t pending() const override;
		/**
		 * @brief 上一次读写是否需要等待可读.
		 */
		bool wantRead() const override;
		/**
		 * @brief 上一次读写是否需要等待可写.
		 */
		bool wantWrite() const override;
		/**
		 * @brief 关闭连接（先发送 TLS close_notify）.
		 */
		void close() override;
		/**
		 * @brief 不发送 close_notify 直接关闭底层 socket.
		 * @note 仅用于对端已不可用、或需要立即释放连接的场景；
		 * 对端将观察到「连接被截断」而不是干净结束。
		 */
		void abort();

	private:
		/**
		 * @brief 设置/恢复底层 socket 超时（毫秒，0 表示无限等待）.
		 */
		void setReceiveTimeout(int32_t milliseconds);
		/**
		 * @brief 设置/恢复底层 socket 发送超时（毫秒，0 表示无限等待）.
		 */
		void setSendTimeout(int32_t milliseconds);
		/**
		 * @brief 关闭前有界排空内核里未读的数据.
		 * @note 内核接收缓冲区里还有未读数据时 `closesocket` 会发送 RST，
		 * 对端会把连接看成「被截断」（`SSL_ERROR_SYSCALL`）而不是干净结束；
		 * 这里先把已有数据读进 TLS 层，再关闭，避免这条路径。
		 */
		void drainBeforeClose();

	private:
		//! TLS 上下文
		TlsContext::ptr context_;
		//! 底层 SSL
		struct ssl_st* ssl_ = nullptr;
		//! 本端角色
		Side side_ = Side::server;
		//! 握手是否完成
		bool handshakeDone_ = false;
		//! 是否已经发送 close_notify
		bool closeNotifySent_ = false;
		//! 上一次读写是否需要等待可读
		bool wantRead_ = false;
		//! 上一次读写是否需要等待可写
		bool wantWrite_ = false;
		//! 握手超时（毫秒）
		uint32_t handshakeTimeoutMs_ = 5000;
		//! 需要校验的对端主机
		std::string verifyHost_;
		//! 协商出的协议版本
		std::string negotiatedVersion_;
		//! 协商出的密码套件
		std::string negotiatedCipher_;
	};

	/**
	 * @brief TLS 监听套接字：`accept()` 产出 `TlsSocket`.
	 */
	class TlsServerSocket final : public Socket {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<TlsServerSocket>;
		/**
		 * @brief 构造函数.
		 */
		explicit TlsServerSocket(TlsContext::ptr context);

	protected:
		/**
		 * @brief 构造 accept 出来的 TLS socket（握手由调用方执行）.
		 */
		Socket::ptr createAcceptedSocket(socket_t acceptedSocket) override;

	private:
		//! TLS 上下文
		TlsContext::ptr context_;
	};
}
