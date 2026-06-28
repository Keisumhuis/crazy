#include "crazy/net/socket.h"

#include <string.h>

#include "crazy/logger.h"

namespace crazy {
	Socket::Socket() {
		initSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}
	Socket::ptr Socket::accept() {
		auto clientSocket = std::make_shared<Socket>();
		clientSocket->socket_ = ::accept(socket_, nullptr, nullptr);
		if (clientSocket->socket_ <= 0) {
			CRAZY_SYSTEM_ERROR() << "bind error errno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			return nullptr;
		}
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
		if (!active()) {
			initSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		}

		sockaddr_in connectSockaddr;
		memset(&connectSockaddr, 0, sizeof(connectSockaddr));
		connectSockaddr.sin_family = AF_INET;
		connectSockaddr.sin_port = htons(port);

		auto ip_addr = inet_addr(address.c_str());
		if (ip_addr == INADDR_NONE) {
			CRAZY_SYSTEM_ERROR() << "invalid ip address: " << address;
			return false;
		}
		connectSockaddr.sin_addr.s_addr = ip_addr;

		if (::connect(socket_, (sockaddr*)&connectSockaddr, sizeof(connectSockaddr)) != 0) {
			CRAZY_SYSTEM_ERROR() << "socket = " << socket_ << " errorno = "
				<< WSAGetLastError() << " errstr = " << strerror(errno) << " addr = " << address << ":" << port;
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
