/**
 * @file tls.cpp
 * @author keisum (keisumhuis@gmail.com)
 * @brief TLS 传输实现（OpenSSL）
 * @version 0.1
 * @date 2026-10-09
 *
 * @copyright Copyright (c) 2026
 */
#include "crazy/net/tls.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "crazy/logger.h"
#include "crazy/net/ca_bundle.h"

#if !_WIN32
#include <sys/ioctl.h>
#endif

#ifdef CRAZY_TLS_ENABLED
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#endif

namespace crazy {
	namespace {
#ifdef CRAZY_TLS_ENABLED
		/**
		 * @brief 取出 OpenSSL 错误队列中的全部可读错误.
		 */
		std::string opensslErrorString() {
			std::string message;
			unsigned long code = 0;
			while ((code = ERR_get_error()) != 0) {
				char buffer[256] = {};
				ERR_error_string_n(code, buffer, sizeof(buffer));
				if (!message.empty()) {
					message += "; ";
				}
				message += buffer;
			}
			if (message.empty()) {
				message = "no error detail";
			}
			return message;
		}
		/**
		 * @brief 协议版本字符串转常量.
		 */
		bool parseVersion(const std::string& version, int32_t& value) {
			if (version == "1.2") {
				value = TLS1_2_VERSION;
				return true;
			}
			if (version == "1.3") {
				value = TLS1_3_VERSION;
				return true;
			}
			return false;
		}
		/**
		 * @brief 是否 IP 字面量.
		 */
		bool isIpLiteral(const std::string& host, int32_t& family) {
			in_addr address4 = {};
			in6_addr address6 = {};
			if (inet_pton(AF_INET, host.c_str(), &address4) == 1) {
				family = AF_INET;
				return true;
			}
			if (inet_pton(AF_INET6, host.c_str(), &address6) == 1) {
				family = AF_INET6;
				return true;
			}
			return false;
		}
		/**
		 * @brief 把 SSL_get_error 的结果翻译成可读原因.
		 */
		std::string describeSslError(int32_t sslError) {
			const char* name = "unknown";
			switch (sslError) {
			case SSL_ERROR_NONE:
				name = "SSL_ERROR_NONE";
				break;
			case SSL_ERROR_SSL:
				name = "SSL_ERROR_SSL";
				break;
			case SSL_ERROR_WANT_READ:
				name = "SSL_ERROR_WANT_READ";
				break;
			case SSL_ERROR_WANT_WRITE:
				name = "SSL_ERROR_WANT_WRITE";
				break;
			case SSL_ERROR_SYSCALL:
				name = "SSL_ERROR_SYSCALL";
				break;
			case SSL_ERROR_ZERO_RETURN:
				name = "SSL_ERROR_ZERO_RETURN";
				break;
			default:
				break;
			}
			return std::string(name) + ": " + opensslErrorString();
		}
		/**
		 * @brief 把随框架编译进来的 CA bundle 加入信任库.
		 * @param store 目标信任库（由 `SSL_CTX` 持有，调用方不释放）.
		 * @return 实际加入的证书数量（`0` 表示没有可用 bundle 或全部解析失败）.
		 * @note 重复证书（例如默认验证路径里已有同一个根）不算失败；
		 * 解析结束时 `PEM_read_bio_X509` 会把「无更多证书」记进错误队列，这里统一清掉。
		 */
		std::size_t addBundledCaToStore(X509_STORE* store) {
			const std::string& pem = bundledCaPem();
			if (pem.empty() || store == nullptr) {
				return 0;
			}
			BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int32_t>(pem.size()));
			if (bio == nullptr) {
				return 0;
			}
			std::size_t added = 0;
			X509* certificate = nullptr;
			while ((certificate = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr)) != nullptr) {
				if (X509_STORE_add_cert(store, certificate) == 1) {
					++added;
				}
				else {
					ERR_clear_error();
				}
				X509_free(certificate);
			}
			ERR_clear_error();
			BIO_free(bio);
			return added;
		}
		/**
		 * @brief 是否由 `SSL_CERT_FILE` / `SSL_CERT_DIR` 显式指定了 OpenSSL 信任库.
		 */
		bool hasExplicitDefaultTrustStore() {
			return std::getenv("SSL_CERT_FILE") != nullptr || std::getenv("SSL_CERT_DIR") != nullptr;
		}
#endif
	}

	TlsContext::~TlsContext() {
		release();
	}
	void TlsContext::release() {
#ifdef CRAZY_TLS_ENABLED
		if (ctx_ != nullptr) {
			SSL_CTX_free(ctx_);
			ctx_ = nullptr;
		}
#endif
	}
	TlsContext::ptr TlsContext::createServer(const TlsOptions& options, std::string& error) {
#ifndef CRAZY_TLS_ENABLED
		(void)options;
		error = "TLS support is not compiled in (CRAZY_ENABLE_TLS=OFF), rebuild with -DCRAZY_ENABLE_TLS=ON";
		return nullptr;
#else
		if (options.certFile.empty()) {
			error = "tls_cert_file is empty";
			return nullptr;
		}
		if (options.keyFile.empty()) {
			error = "tls_key_file is empty";
			return nullptr;
		}
		int32_t minVersion = 0;
		if (!parseVersion(options.minVersion, minVersion)) {
			error = "invalid tls_min_version: " + options.minVersion + " (only \"1.2\" and \"1.3\" are accepted)";
			return nullptr;
		}
		int32_t maxVersion = 0;
		if (!options.maxVersion.empty() && !parseVersion(options.maxVersion, maxVersion)) {
			error = "invalid tls_max_version: " + options.maxVersion + " (only \"1.2\" and \"1.3\" are accepted)";
			return nullptr;
		}

		TlsContext::ptr context(new TlsContext());
		context->role_ = Role::server;
		context->minVersion_ = options.minVersion;
		context->maxVersion_ = options.maxVersion;
		context->verifyPeer_ = options.verifyPeer;
		context->ctx_ = SSL_CTX_new(TLS_server_method());
		if (context->ctx_ == nullptr) {
			error = "SSL_CTX_new(TLS_server_method) failed: " + opensslErrorString();
			return nullptr;
		}
		if (SSL_CTX_set_min_proto_version(context->ctx_, minVersion) != 1) {
			error = "set tls_min_version = " + options.minVersion + " failed: " + opensslErrorString();
			return nullptr;
		}
		if (!options.maxVersion.empty() && SSL_CTX_set_max_proto_version(context->ctx_, maxVersion) != 1) {
			error = "set tls_max_version = " + options.maxVersion + " failed: " + opensslErrorString();
			return nullptr;
		}
		if (SSL_CTX_use_certificate_chain_file(context->ctx_, options.certFile.c_str()) != 1) {
			error = "load tls_cert_file failed, file = " + options.certFile + ", " + opensslErrorString();
			return nullptr;
		}
		if (SSL_CTX_use_PrivateKey_file(context->ctx_, options.keyFile.c_str(), SSL_FILETYPE_PEM) != 1) {
			error = "load tls_key_file failed, file = " + options.keyFile + ", " + opensslErrorString();
			return nullptr;
		}
		if (SSL_CTX_check_private_key(context->ctx_) != 1) {
			error = "tls_key_file does not match tls_cert_file, cert = " + options.certFile
				+ ", key = " + options.keyFile + ", " + opensslErrorString();
			return nullptr;
		}
		if (options.verifyPeer) {
			SSL_CTX_set_verify(context->ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
		}
		return context;
#endif
	}
	TlsContext::ptr TlsContext::createClient(const TlsOptions& options, std::string& error) {
#ifndef CRAZY_TLS_ENABLED
		(void)options;
		error = "TLS support is not compiled in (CRAZY_ENABLE_TLS=OFF), rebuild with -DCRAZY_ENABLE_TLS=ON";
		return nullptr;
#else
		int32_t minVersion = 0;
		if (!parseVersion(options.minVersion, minVersion)) {
			error = "invalid tls_min_version: " + options.minVersion + " (only \"1.2\" and \"1.3\" are accepted)";
			return nullptr;
		}
		int32_t maxVersion = 0;
		if (!options.maxVersion.empty() && !parseVersion(options.maxVersion, maxVersion)) {
			error = "invalid tls_max_version: " + options.maxVersion + " (only \"1.2\" and \"1.3\" are accepted)";
			return nullptr;
		}

		TlsContext::ptr context(new TlsContext());
		context->role_ = Role::client;
		context->minVersion_ = options.minVersion;
		context->maxVersion_ = options.maxVersion;
		context->verifyPeer_ = options.verifyPeer;
		context->ctx_ = SSL_CTX_new(TLS_client_method());
		if (context->ctx_ == nullptr) {
			error = "SSL_CTX_new(TLS_client_method) failed: " + opensslErrorString();
			return nullptr;
		}
		if (SSL_CTX_set_min_proto_version(context->ctx_, minVersion) != 1) {
			error = "set tls_min_version = " + options.minVersion + " failed: " + opensslErrorString();
			return nullptr;
		}
		if (!options.maxVersion.empty() && SSL_CTX_set_max_proto_version(context->ctx_, maxVersion) != 1) {
			error = "set tls_max_version = " + options.maxVersion + " failed: " + opensslErrorString();
			return nullptr;
		}
		if (options.verifyPeer) {
			if (!options.caFile.empty()) {
				// ① 显式配置了 tls_ca_file：**只用它**。
				//    调用方可能只想信任私有 CA / 测试夹具 CA，因此绝不叠加随包 bundle。
				if (SSL_CTX_load_verify_locations(context->ctx_, options.caFile.c_str(), nullptr) != 1) {
					error = "load tls_ca_file failed, file = " + options.caFile + ", " + opensslErrorString();
					return nullptr;
				}
			}
			else {
				// ② 未显式配置：OpenSSL 默认验证路径（SSL_CERT_FILE / SSL_CERT_DIR）+ 随包 bundle。
				X509_STORE* store = SSL_CTX_get_cert_store(context->ctx_);
				if (store == nullptr) {
					error = "SSL_CTX_get_cert_store failed: " + opensslErrorString();
					return nullptr;
				}
				if (X509_STORE_set_default_paths(store) != 1) {
					// Windows 上 OpenSSL 不读系统证书存储，这条失败是常态，不是致命错误：
					// 随包 bundle 仍然是可用的信任来源，所以这里只记录并继续。
					ERR_clear_error();
					CRAZY_SYSTEM_DEBUG() << "tls client: no usable OpenSSL default verify paths"
						<< " (SSL_CERT_FILE/SSL_CERT_DIR or the compiled-in default file)";
				}
				if (hasExplicitDefaultTrustStore()) {
					// 用户用 SSL_CERT_FILE / SSL_CERT_DIR 显式指定了信任库：
					// 尊重该选择，不叠加随包 bundle（否则「只信私有 CA」会被 bundle 兜住）。
					CRAZY_SYSTEM_INFO() << "tls client uses the trust store explicitly selected through"
						<< " SSL_CERT_FILE/SSL_CERT_DIR; the bundled CA bundle is intentionally not added";
				}
				else if (bundledCaCertificateCount() > 0) {
					const std::size_t added = addBundledCaToStore(store);
					if (added > 0) {
						CRAZY_SYSTEM_INFO() << "tls client loaded the bundled CA bundle, certificates = " << added
							<< ", compiled from = " << bundledCaSource()
							<< " (compiled into the library, no file is read at run time)";
					}
					else {
						CRAZY_SYSTEM_WARN() << "tls client could not load the bundled CA bundle, compiled from = "
							<< bundledCaSource() << "; only the OpenSSL default verify paths remain";
					}
				}
				else {
					CRAZY_SYSTEM_WARN() << "this build has no bundled CA bundle compiled in; the client falls back to"
						<< " the OpenSSL default verify paths only (SSL_CERT_FILE/SSL_CERT_DIR),"
						<< " so public HTTPS sites may fail with 'unable to get local issuer certificate'";
				}
			}
			SSL_CTX_set_verify(context->ctx_, SSL_VERIFY_PEER, nullptr);
		}
		else {
			SSL_CTX_set_verify(context->ctx_, SSL_VERIFY_NONE, nullptr);
		}
		return context;
#endif
	}
	TlsContext::Role TlsContext::role() const {
		return role_;
	}
	const std::string& TlsContext::minVersion() const {
		return minVersion_;
	}
	const std::string& TlsContext::maxVersion() const {
		return maxVersion_;
	}
	bool TlsContext::verifyPeer() const {
		return verifyPeer_;
	}
	struct ssl_ctx_st* TlsContext::native() const {
		return ctx_;
	}

	TlsSocket::TlsSocket(TlsContext::ptr context, Side side)
		: Socket(), context_(std::move(context)), side_(side) {
	}
	TlsSocket::TlsSocket(socket_t acceptedSocket, TlsContext::ptr context, Side side)
		: Socket(acceptedSocket), context_(std::move(context)), side_(side) {
	}
	TlsSocket::~TlsSocket() {
		close();
	}
	bool TlsSocket::handshake(std::string& error) {
		if (handshakeDone_) {
			return true;
		}
		if (!context_) {
			error = "tls context is null";
			return false;
		}
		if (!active()) {
			error = "socket is not active";
			return false;
		}
#ifndef CRAZY_TLS_ENABLED
		error = "TLS support is not compiled in (CRAZY_ENABLE_TLS=OFF)";
		return false;
#else
		if (ssl_ == nullptr) {
			ssl_ = SSL_new(context_->native());
			if (ssl_ == nullptr) {
				error = "SSL_new failed: " + opensslErrorString();
				return false;
			}
			if (SSL_set_fd(ssl_, static_cast<int32_t>(socket_)) != 1) {
				error = "SSL_set_fd failed: " + opensslErrorString();
				return false;
			}
			if (Side::server == side_) {
				SSL_set_accept_state(ssl_);
			}
			else {
				SSL_set_connect_state(ssl_);
				if (!verifyHost_.empty()) {
					int32_t family = 0;
					X509_VERIFY_PARAM* parameter = SSL_get0_param(ssl_);
					if (isIpLiteral(verifyHost_, family)) {
						X509_VERIFY_PARAM_set1_ip_asc(parameter, verifyHost_.c_str());
					}
					else {
						X509_VERIFY_PARAM_set1_host(parameter, verifyHost_.c_str(), 0);
						SSL_set_tlsext_host_name(ssl_, verifyHost_.c_str());
					}
				}
			}
		}

		// 握手期阻塞 DoS 的最小防护：只在握手期间设置收发超时，握手完成或失败后立即恢复
		setReceiveTimeout(static_cast<int32_t>(handshakeTimeoutMs_));
		setSendTimeout(static_cast<int32_t>(handshakeTimeoutMs_));
		const int32_t result = (Side::server == side_) ? SSL_accept(ssl_) : SSL_connect(ssl_);
		const int32_t sslError = (result == 1) ? SSL_ERROR_NONE : SSL_get_error(ssl_, result);
		setReceiveTimeout(0);
		setSendTimeout(0);

		if (result != 1) {
			error = std::string(Side::server == side_ ? "SSL_accept failed, timeout = " : "SSL_connect failed, timeout = ")
				+ std::to_string(handshakeTimeoutMs_) + "ms, " + describeSslError(sslError);
			const long verifyResult = SSL_get_verify_result(ssl_);
			if (X509_V_OK != verifyResult) {
				// 证书校验失败时给出可读的具体原因（例如「certificate has expired」）
				error += std::string("; certificate verify failed: ") + X509_verify_cert_error_string(verifyResult);
			}
			return false;
		}

		handshakeDone_ = true;
		const char* version = SSL_get_version(ssl_);
		negotiatedVersion_ = version != nullptr ? version : "";
		const char* cipher = SSL_get_cipher_name(ssl_);
		negotiatedCipher_ = cipher != nullptr ? cipher : "";
		return true;
#endif
	}
	bool TlsSocket::handshakeDone() const {
		return handshakeDone_;
	}
	void TlsSocket::setHandshakeTimeout(uint32_t milliseconds) {
		handshakeTimeoutMs_ = milliseconds;
	}
	void TlsSocket::setVerifyHost(const std::string& host) {
		verifyHost_ = host;
	}
	const std::string& TlsSocket::negotiatedVersion() const {
		return negotiatedVersion_;
	}
	const std::string& TlsSocket::negotiatedCipher() const {
		return negotiatedCipher_;
	}
	int32_t TlsSocket::send(const void* buffer, size_t length, int32_t flags) {
		(void)flags;
		wantRead_ = false;
		wantWrite_ = false;
		if (!active() || !handshakeDone_) {
			errno = EIO;
			return -1;
		}
		if (length == 0) {
			return 0;
		}
#ifndef CRAZY_TLS_ENABLED
		errno = EIO;
		return -1;
#else
		const int32_t result = SSL_write(ssl_, buffer, static_cast<int32_t>(length));
		if (result > 0) {
			// 默认模式下 SSL_write 只在整段被接受后返回，部分写入不得暴露成字节偏移语义
			if (static_cast<size_t>(result) != length) {
				wantWrite_ = true;
				errno = EAGAIN;
				return -1;
			}
			return result;
		}
		const int32_t sslError = SSL_get_error(ssl_, result);
		if (SSL_ERROR_WANT_WRITE == sslError) {
			wantWrite_ = true;
			errno = EAGAIN;
			return -1;
		}
		if (SSL_ERROR_WANT_READ == sslError) {
			wantRead_ = true;
			errno = EAGAIN;
			return -1;
		}
		{
			const std::string detail = describeSslError(sslError);
			CRAZY_SYSTEM_ERROR() << "tls write fail, " << detail;
		}
		errno = EIO;
		return -1;
#endif
	}
	int32_t TlsSocket::recv(void* buffer, size_t length, int32_t flags) {
		(void)flags;
		wantRead_ = false;
		wantWrite_ = false;
		if (!active() || !handshakeDone_) {
			errno = EIO;
			return -1;
		}
		if (length == 0) {
			return 0;
		}
#ifndef CRAZY_TLS_ENABLED
		errno = EIO;
		return -1;
#else
		const int32_t result = SSL_read(ssl_, buffer, static_cast<int32_t>(length));
		if (result > 0) {
			return result;
		}
		const int32_t sslError = SSL_get_error(ssl_, result);
		if (SSL_ERROR_ZERO_RETURN == sslError) {
			// 对端发送了 close_notify：正常关闭
			return 0;
		}
		if (SSL_ERROR_WANT_READ == sslError) {
			wantRead_ = true;
			errno = EAGAIN;
			return -1;
		}
		if (SSL_ERROR_WANT_WRITE == sslError) {
			wantWrite_ = true;
			errno = EAGAIN;
			return -1;
		}
		// 其余（SSL_ERROR_SSL / SSL_ERROR_SYSCALL / 非预期 EOF）都按「连接被截断」处理
		{
			const std::string detail = describeSslError(sslError);
			CRAZY_SYSTEM_ERROR() << "tls read fail, " << detail;
		}
		errno = EIO;
		return -1;
#endif
	}
	int32_t TlsSocket::pending() const {
#ifdef CRAZY_TLS_ENABLED
		if (ssl_ == nullptr) {
			return 0;
		}
		return static_cast<int32_t>(SSL_pending(ssl_));
#else
		return 0;
#endif
	}
	bool TlsSocket::wantRead() const {
		return wantRead_;
	}
	bool TlsSocket::wantWrite() const {
		return wantWrite_;
	}
	void TlsSocket::close() {
#ifdef CRAZY_TLS_ENABLED
		if (ssl_ != nullptr) {
			if (handshakeDone_ && !closeNotifySent_) {
				closeNotifySent_ = true;
				// close_notify 也要有界，避免对端不读取时把调用线程挂死
				setSendTimeout(static_cast<int32_t>(handshakeTimeoutMs_));
				SSL_shutdown(ssl_);
				setSendTimeout(0);
			}
			drainBeforeClose();
			SSL_free(ssl_);
			ssl_ = nullptr;
		}
#endif
		SocketInterface::close();
	}
	void TlsSocket::drainBeforeClose() {
#ifdef CRAZY_TLS_ENABLED
		// 内核接收缓冲区里还有未读数据时 closesocket 会发 RST，对端会看到「连接被截断」。
		// 先确认确实有未读数据（避免在正常关闭路径上白等一次超时），再有界地读进 TLS 层。
		u_long available = 0;
#if _WIN32
		if (ioctlsocket(socket_, FIONREAD, &available) != 0 || available == 0) {
			return;
		}
#else
		if (ioctl(socket_, FIONREAD, &available) != 0 || available == 0) {
			return;
		}
#endif
		char buffer[4096];
		setReceiveTimeout(100);
		for (int32_t i = 0; i < 16; ++i) {
			if (SSL_read(ssl_, buffer, sizeof(buffer)) <= 0) {
				break;
			}
		}
		setReceiveTimeout(0);
#endif
	}
	void TlsSocket::abort() {
#ifdef CRAZY_TLS_ENABLED
		if (ssl_ != nullptr) {
			SSL_free(ssl_);
			ssl_ = nullptr;
		}
#endif
		closeNotifySent_ = true;
		SocketInterface::close();
	}
	void TlsSocket::setReceiveTimeout(int32_t milliseconds) {
#if _WIN32
		DWORD timeout = static_cast<DWORD>(milliseconds);
		setOption(SOL_SOCKET, SO_RCVTIMEO, timeout);
#else
		timeval timeout = {};
		timeout.tv_sec = milliseconds / 1000;
		timeout.tv_usec = (milliseconds % 1000) * 1000;
		setOption(SOL_SOCKET, SO_RCVTIMEO, timeout);
#endif
	}
	void TlsSocket::setSendTimeout(int32_t milliseconds) {
#if _WIN32
		DWORD timeout = static_cast<DWORD>(milliseconds);
		setOption(SOL_SOCKET, SO_SNDTIMEO, timeout);
#else
		timeval timeout = {};
		timeout.tv_sec = milliseconds / 1000;
		timeout.tv_usec = (milliseconds % 1000) * 1000;
		setOption(SOL_SOCKET, SO_SNDTIMEO, timeout);
#endif
	}

	TlsServerSocket::TlsServerSocket(TlsContext::ptr context)
		: Socket(), context_(std::move(context)) {
	}
	Socket::ptr TlsServerSocket::createAcceptedSocket(socket_t acceptedSocket) {
		return std::make_shared<TlsSocket>(acceptedSocket, context_, TlsSocket::Side::server);
	}
}
