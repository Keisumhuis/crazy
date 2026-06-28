#include "crazy/net/socket_interface.h"

#include "crazy/logger.h"

namespace crazy {
	SocketInterface::~SocketInterface() {
		close();
	}
	bool SocketInterface::active() const {
		return active_.load();
	}
	void SocketInterface::close() {
		if (active()) {
			closesocket(socket_);
			active_.store(false);
		}
	}
	int32_t SocketInterface::send(const void* buffer, size_t length, int32_t flags) {
		if (!active()) {
			return -1;
		}
		return ::send(socket_, (const char*)buffer, length, flags);
	}
	int32_t SocketInterface::recv(void* buffer, size_t length, int32_t flags) {
		if (!active()) {
			return -1;
		}
		return ::recv(socket_, (char*)buffer, length, flags);
	}
	bool SocketInterface::getOption(int32_t level, int32_t option, void* result, socklen_t* len) {
		if (getsockopt(socket_, level, option, (char*)result, len)) {
			CRAZY_SYSTEM_ERROR() << "getOption sock = " << socket_ << " level = " << level << " option = " << option
				<< " errorno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			return false;
		}
		return true;
	}
	bool SocketInterface::setOption(int32_t level, int32_t option, void* result, socklen_t len) {
		if (setsockopt(socket_, level, option, (char*)result, len)) {
			CRAZY_SYSTEM_ERROR() << "setOption sock = " << socket_ << " level = " << level << " option = " << option
				<< " errorno = " << WSAGetLastError() << " errstr = " << strerror(errno);
			return false;
		}
		return true;
	}
	socket_t SocketInterface::socket() const {
		return socket_;
	}
	void SocketInterface::initSocket(int32_t af, int32_t type, int32_t protocol) {
		socket_ = ::socket(af, type, protocol);
		if (INVALID_SOCKET == socket_) {
			CRAZY_SYSTEM_ERROR() << "socket(" << af << ", " << type << ", " << protocol << ") fail, errno = " << WSAGetLastError() << ", error message = " << strerror(errno);
			return;
		}
		active_.store(true);
	}
}
