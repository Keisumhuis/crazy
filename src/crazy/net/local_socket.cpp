#include "crazy/net/local_socket.h"

#include "crazy/logger.h"

namespace crazy {
	LocalSocket::LocalSocket() {
		initSocket(AF_UNIX, SOCK_STREAM, 0);
	}
	LocalSocket::ptr LocalSocket::accept() {
		auto clientSocket = std::make_shared<LocalSocket>();
		clientSocket->socket_ = ::accept(socket_, nullptr, nullptr);
		if (clientSocket->socket_ <= 0) {
			CRAZY_SYSTEM_ERROR() << "accept error errno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			return nullptr;
		}
		return clientSocket;
	}
	bool LocalSocket::listen(const std::string& address) {
		if (!active()) {
			initSocket(AF_UNIX, SOCK_STREAM, 0);
		}
		
		sockaddr_un listenSockaddr;
		memset(&listenSockaddr, 0, sizeof(listenSockaddr));
		listenSockaddr.sun_family = AF_UNIX;
		strncpy(listenSockaddr.sun_path, address.c_str(), address.size() < 107 ? address.size() : 107);

		unlink(address.data());
		if (::bind(socket_, (sockaddr*)&listenSockaddr, sizeof(listenSockaddr)) != 0) {
			CRAZY_SYSTEM_ERROR() << "bind error errno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			return false;
		}

		if (::listen(socket_, SOMAXCONN) != 0) {
			CRAZY_SYSTEM_ERROR() << "listen error errno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			return false;
		}

		return true;
	}
	bool LocalSocket::connect(const std::string& address) {
		if (!active()) {
			initSocket(AF_UNIX, SOCK_STREAM, 0);
		}

		sockaddr_un connectSockaddr;
		memset(&connectSockaddr, 0, sizeof(connectSockaddr));
		connectSockaddr.sun_family = AF_UNIX;
		strncpy(connectSockaddr.sun_path, address.c_str(), address.size() < 107 ? address.size() : 107);

		if (::connect(socket_, (sockaddr*)&connectSockaddr, sizeof(connectSockaddr)) != 0) {
			CRAZY_SYSTEM_ERROR() << "socket = " << socket_ << " errorno = "
				<< WSAGetLastError() << " errstr = " << strerror(errno);
			return false;
		}
		return true;
	}
}
