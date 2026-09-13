#include "crazy/net/socket.h"

#include <string.h>

#include "crazy/logger.h"

namespace crazy {
	Socket::Socket() {
		initSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}
	Socket::Socket(socket_t acceptedSocket) {
		socket_ = acceptedSocket;
		active_.store(acceptedSocket != INVALID_SOCKET);
		if (!active()) {
			return;
		}

		int32_t value = 1;
#if WIN32
		setOption(SOL_SOCKET, SO_REUSEADDR, value);
#else
		setOption(SOL_SOCKET, SO_REUSEPORT, value);
#endif
		setOption(IPPROTO_TCP, TCP_NODELAY, value);

		int32_t bufferSize = 32768;
		setOption(SOL_SOCKET, SO_SNDBUF, bufferSize);
		setOption(SOL_SOCKET, SO_RCVBUF, bufferSize);
	}
	Socket::ptr Socket::accept() {
		const socket_t acceptedSocket = ::accept(socket_, nullptr, nullptr);
		if (acceptedSocket == INVALID_SOCKET) {
			CRAZY_SYSTEM_ERROR() << "bind error errno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			return nullptr;
		}
		auto clientSocket = Socket::ptr(new Socket(acceptedSocket));
		clientSocket->updateLocalAddress();
		clientSocket->updateRemoteAddress();
		return clientSocket;
	}
	bool Socket::listen(uint64_t port, const std::string& address) {
		if (!active()) {
			initSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		}
	
		sockaddr_in listenSockaddr;
		memset(&listenSockaddr, 0, sizeof(listenSockaddr));
		listenSockaddr.sin_family = AF_INET;
		listenSockaddr.sin_port = htons(port);

		auto ip_addr = inet_addr(address.c_str());
		if (ip_addr == INADDR_NONE) {
			CRAZY_SYSTEM_ERROR() << "invalid ip address: " << address;
			return false;
		}
		listenSockaddr.sin_addr.s_addr = ip_addr;

		if (::bind(socket_, (sockaddr*)&listenSockaddr, sizeof(listenSockaddr)) != 0) {
			CRAZY_SYSTEM_ERROR() << "bind error errno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			return false;
		}

		if (::listen(socket_, SOMAXCONN) != 0) {
			CRAZY_SYSTEM_ERROR() << "listen error errno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			return false;
		}

		updateLocalAddress();
		return true;
	}
	bool Socket::connect(const std::string& address, uint64_t port) {
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo* result = nullptr;
		const std::string portString = std::to_string(port);
		if (getaddrinfo(address.c_str(), portString.c_str(), &hints, &result) != 0) {
			CRAZY_SYSTEM_ERROR() << "resolve address fail, addr = " << address << ":" << port;
			return false;
		}

		bool connected = false;
		for (auto* it = result; it != nullptr; it = it->ai_next) {
			close();
			initSocket(it->ai_family, it->ai_socktype, it->ai_protocol);
			if (!active()) {
				continue;
			}
			if (::connect(socket_, it->ai_addr, static_cast<int32_t>(it->ai_addrlen)) == 0) {
				connected = true;
				break;
			}
		}
		freeaddrinfo(result);

		if (!connected) {
			CRAZY_SYSTEM_ERROR() << "socket connect fail, addr = " << address << ":" << port
				<< ", errorno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			close();
			return false;
		}

		updateLocalAddress();
		updateRemoteAddress();
		return true;
	}
	const std::string& Socket::remoteAddress() const {
		return remoteAddress_;
	}
	const std::string& Socket::localAddress() const {
		return localAddress_;
	}
	void Socket::initSocket(int32_t af, int32_t type, int32_t protocol) {
		SocketInterface::initSocket(af, type, protocol);
		int32_t value = 1;
#if WIN32
		setOption(SOL_SOCKET, SO_REUSEADDR, value);
#else
		setOption(SOL_SOCKET, SO_REUSEPORT, value);
#endif
		setOption(IPPROTO_TCP, TCP_NODELAY, value);

		int32_t bufferSize = 32768;
		setOption(SOL_SOCKET, SO_SNDBUF, bufferSize);
		setOption(SOL_SOCKET, SO_RCVBUF, bufferSize);
	}
	void Socket::updateLocalAddress() {
		sockaddr_in address;
		socklen_t len = sizeof(address);
		if (getsockname(socket_, (sockaddr*)&address, &len)) {
			CRAZY_SYSTEM_ERROR() << "getsockname(" << socket_ << ") fail, errno" << WSAGetLastError() << " errstr = " << strerror(errno);
			return;
		}
		localAddress_ = sockaddr2string(address);
	}
	void Socket::updateRemoteAddress() {
		sockaddr_in address;
		socklen_t len = sizeof(address);
		if (getpeername(socket_, (sockaddr*)&address, &len)) {
			CRAZY_SYSTEM_ERROR() << "getpeername(" << socket_ << ") fail, errno" << WSAGetLastError() << " errstr = " << strerror(errno);
			return;
		}
		remoteAddress_ = sockaddr2string(address);
	}
	std::string Socket::sockaddr2string(const sockaddr_in& address) {
		uint32_t addr = ntohl(address.sin_addr.s_addr);
		std::stringstream ss;
		ss << "tcp://" << ((addr >> 24) & 0xff) << "." << ((addr >> 16) & 0xff) << "." << ((addr >> 8) & 0xff) << "." << (addr & 0xff);
		ss << ":" << ntohs(address.sin_port);
		return ss.str();
	}
}
