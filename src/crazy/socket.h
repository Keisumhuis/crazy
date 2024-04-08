/**
 * @file socket.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_SOCKET_H____
#define ____CRAZY_SOCKET_H____

#include <netinet/tcp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <memory>

#include "address.h"
#include "assert.h"
#include "logger.h"
#include "noncopyable.h"

namespace crazy {

	enum class SocketType : int32_t {
		TCP = SOCK_STREAM,
		UDP = SOCK_DGRAM
	};

	enum class SocketFamily : int32_t {
		IPv4 = AF_INET,
		IPv6 = AF_INET6,
		UNIX = AF_UNIX
	};

	class Socket : public std::enable_shared_from_this<Socket> {
	public:
		using Ptr = std::shared_ptr<Socket>;
		static Socket::Ptr CreateTCP(Address::Ptr address);
		static Socket::Ptr CreateUDP(Address::Ptr address);
		static Socket::Ptr CreateTCPSocket();
		static Socket::Ptr CreateUDPSocket();
		static Socket::Ptr CreateTCPSocket6();
		static Socket::Ptr CreateUDPSocket6();
		static Socket::Ptr CreateUinxTCPSocket();
		static Socket::Ptr CreateUinxUDPSocket();

		Socket(int32_t family, int32_t type, int32_t protocol = 0);
		virtual ~Socket();
		bool GetOption(int32_t level, int32_t option, void* result, socklen_t* len);
		template <typename type>
		bool GetOption(int32_t level, int32_t option, type& value) {
			return GetOption(level, option, &value, sizeof(type));
		}
		bool SetOption(int32_t level, int32_t option, void* result, socklen_t len);
		template <typename type>
		bool SetOption(int32_t level, int32_t option, type& value) {
			return SetOption(level, option, &value, sizeof(type));
		}

		virtual Socket::Ptr Accept();
		virtual bool Bind(Address::Ptr addr);
		virtual bool Connect(const Address::Ptr addr, uint64_t timeout_ms = 0);
		virtual bool ReConnect(uint64_t timeout_ms = 0);
		virtual bool Listen(int32_t backlog = SOMAXCONN);
		virtual bool Close();
		virtual int32_t Send(const void* buffer, size_t length, int32_t flags = 0);
		virtual int32_t Send(const iovec* buffers, size_t length, int32_t flags = 0);
		virtual int32_t SendTo(const void* buffer, size_t length, const Address::Ptr addr,int32_t flags = 0);
		virtual int32_t SendTo(const iovec* buffers, size_t length, const Address::Ptr addr, int32_t flags = 0);
		virtual int32_t Recv(void* buffer, size_t length, int32_t flags = 0);
		virtual int32_t Recv(iovec* buffer, size_t length, int32_t flags = 0);
		virtual int32_t RecvFrom(void* buffer, size_t length, Address::Ptr addr, int32_t flags = 0);
		virtual int32_t RecvFrom(iovec* buffers, size_t length, Address::Ptr addr, int32_t flags = 0);
		Address::Ptr GetRemoteAddress();
		Address::Ptr GetLocalAddress();
		int32_t GetFamily() const;
		int32_t GetType() const;
		int32_t GetProtocol() const;
		bool IsConnect() const;
		bool IsValid() const;
		int32_t GetError();
		int32_t GetSocket() const;
		bool CancelRead();
		bool CancelWrite();
		bool CancelAccept();
		bool CancelAll();
	protected:
		void InitSocket();
		void NewSocket();
		virtual bool Init(int32_t socket);
	protected:
		int32_t m_socket;
		int32_t m_family;
		int32_t m_type;
		int32_t m_protocol;
		bool m_isConnected;
		Address::Ptr m_localAddress;
		Address::Ptr m_remoteAddress;
	};

	class SSLSocket : public Socket {
	public:
		using Ptr = std::shared_ptr<SSLSocket>;

		static SSLSocket::Ptr CreateTCP(Address::Ptr addr);
		static SSLSocket::Ptr CreateTCPSocket();
		static SSLSocket::Ptr CreateTCPSocket6();
		
		SSLSocket(int32_t family, int32_t type, int32_t protocol = 0);	
		virtual Socket::Ptr Accept() override;
		virtual bool Bind(Address::Ptr addr) override;
		virtual bool Connect(const Address::Ptr addr, uint64_t timeout_ms = 0) override;
		virtual bool Listen(int32_t backlog = SOMAXCONN) override;
		virtual bool Close() override;
		virtual int32_t Send(const void* buffer, size_t length, int32_t flags = 0) override;
		virtual int32_t Send(const iovec* buffers, size_t length, int32_t flags = 0) override;
		virtual int32_t SendTo(const void* buffer, size_t length, const Address::Ptr addr,int32_t flags = 0) override;
		virtual int32_t SendTo(const iovec* buffers, size_t length, const Address::Ptr addr, int32_t flags = 0) override;
		virtual int32_t Recv(void* buffer, size_t length, int32_t flags = 0) override;
		virtual int32_t Recv(iovec* buffer, size_t length, int32_t flags = 0) override;
		virtual int32_t RecvFrom(void* buffer, size_t length, Address::Ptr addr, int32_t flags = 0) override;
		virtual int32_t RecvFrom(iovec* buffers, size_t length, Address::Ptr addr, int32_t flags = 0) override;
		bool LoadCertificates(const std::string& cert_file, const std::string& key_file);
	protected:
		virtual bool Init(int32_t socket) override;
	private:
		std::shared_ptr<SSL_CTX> m_ctx;
		std::shared_ptr<SSL> m_ssl;
	};
}

#endif // ! ____CRAZY_SOCKET_H____

