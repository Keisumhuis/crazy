#include "crazy/net/local_socket.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "crazy/logger.h"

namespace crazy {
	namespace {
		bool BuildLocalSockaddr(const std::string& address, sockaddr_un& socketAddress) {
			memset(&socketAddress, 0, sizeof(socketAddress));
			socketAddress.sun_family = AF_UNIX;
			if (address.size() >= sizeof(socketAddress.sun_path)) {
				CRAZY_SYSTEM_ERROR() << "local socket address is too long, address = " << address;
				return false;
			}
			strncpy(socketAddress.sun_path, address.c_str(), sizeof(socketAddress.sun_path) - 1);
			return true;
		}

		bool IsAddressInUse(int32_t error) {
#ifdef _WIN32
			return error == WSAEADDRINUSE;
#else
			return error == EADDRINUSE;
#endif
		}

		bool IsLocalSocketAccepting(const std::string& address) {
			socket_t probeSocket = ::socket(AF_UNIX, SOCK_STREAM, 0);
			if (probeSocket == INVALID_SOCKET) {
				return false;
			}

			sockaddr_un socketAddress;
			if (!BuildLocalSockaddr(address, socketAddress)) {
				closesocket(probeSocket);
				return false;
			}

			const bool connected = ::connect(probeSocket, reinterpret_cast<sockaddr*>(&socketAddress), sizeof(socketAddress)) == 0;
			closesocket(probeSocket);
			return connected;
		}

		bool RemoveLocalSocketPath(const std::string& address) {
			errno = 0;
			if (std::remove(address.c_str()) == 0) {
				return true;
			}
			return errno == ENOENT;
		}
	}

	LocalSocket::LocalSocket() {
		initSocket(AF_UNIX, SOCK_STREAM, 0);
	}
	LocalSocket::ptr LocalSocket::accept() {
		auto clientSocket = std::make_shared<LocalSocket>();
		clientSocket->close();
		clientSocket->socket_ = ::accept(socket_, nullptr, nullptr);
		if (clientSocket->socket_ == INVALID_SOCKET) {
			CRAZY_SYSTEM_ERROR() << "accept error errno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			return nullptr;
		}
		clientSocket->active_.store(true);
		return clientSocket;
	}
	bool LocalSocket::listen(const std::string& address) {
		if (!active()) {
			initSocket(AF_UNIX, SOCK_STREAM, 0);
		}
		if (!active()) {
			return false;
		}
		
		sockaddr_un listenSockaddr;
		if (!BuildLocalSockaddr(address, listenSockaddr)) {
			return false;
		}

		if (::bind(socket_, (sockaddr*)&listenSockaddr, sizeof(listenSockaddr)) != 0) {
			const int32_t bindError = WSAGetLastError();
			if (!IsAddressInUse(bindError)) {
				CRAZY_SYSTEM_ERROR() << "bind error errno = " << bindError << " errstr = " << strerror(errno);
				close();
				return false;
			}

			if (IsLocalSocketAccepting(address)) {
				CRAZY_SYSTEM_ERROR() << "local socket address is already in use, address = " << address;
				close();
				return false;
			}

			if (!RemoveLocalSocketPath(address)) {
				CRAZY_SYSTEM_ERROR() << "remove stale local socket fail, address = " << address << " errno = " << errno
					<< " errstr = " << strerror(errno);
				close();
				return false;
			}

			close();
			initSocket(AF_UNIX, SOCK_STREAM, 0);
			if (!active()) {
				return false;
			}
			if (::bind(socket_, (sockaddr*)&listenSockaddr, sizeof(listenSockaddr)) != 0) {
				CRAZY_SYSTEM_ERROR() << "bind error errno = " << WSAGetLastError() << " errstr = " << strerror(errno);
				close();
				return false;
			}
		}

		if (::listen(socket_, SOMAXCONN) != 0) {
			CRAZY_SYSTEM_ERROR() << "listen error errno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			close();
			return false;
		}

		return true;
	}
	bool LocalSocket::connect(const std::string& address) {
		if (!active()) {
			initSocket(AF_UNIX, SOCK_STREAM, 0);
		}
		if (!active()) {
			return false;
		}

		sockaddr_un connectSockaddr;
		if (!BuildLocalSockaddr(address, connectSockaddr)) {
			return false;
		}

		if (::connect(socket_, (sockaddr*)&connectSockaddr, sizeof(connectSockaddr)) != 0) {
			CRAZY_SYSTEM_ERROR() << "socket = " << socket_ << " errorno = "
				<< WSAGetLastError() << " errstr = " << strerror(errno);
			return false;
		}
		return true;
	}
}
