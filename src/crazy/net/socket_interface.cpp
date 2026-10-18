#include "crazy/net/socket_interface.h"

#include <mutex>

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#endif

#include "crazy/logger.h"

namespace crazy {
	bool ensureWinSock() {
#if _WIN32
		static std::once_flag once;
		static bool ready = false;
		std::call_once(once, []() {
			WSADATA wsaData = {};
			const int32_t error = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (0 != error) {
				CRAZY_SYSTEM_ERROR() << "WSAStartup(2, 2) fail, error = " << error
					<< ", no socket can be used in this process";
				return;
			}
			ready = true;
		});
		return ready;
#else
		return true;
#endif
	}
	std::string socketErrorText(int32_t error) {
#if _WIN32
		if (error <= 0) {
			return std::string();
		}
		LPSTR buffer = nullptr;
		const DWORD length = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, static_cast<DWORD>(error), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
		std::string text;
		if (0 != length && nullptr != buffer) {
			text.assign(buffer, length);
			while (!text.empty() && ('\r' == text.back() || '\n' == text.back() || ' ' == text.back())) {
				text.pop_back();
			}
		}
		if (nullptr != buffer) {
			LocalFree(buffer);
		}
		if (text.empty()) {
			text = "unknown socket error " + std::to_string(error);
		}
		return text;
#else
		return std::string(strerror(error));
#endif
	}
	bool socketWouldBlock(int32_t error) {
#if _WIN32
		return WSAEWOULDBLOCK == error || WSAEINPROGRESS == error || WSAEALREADY == error;
#else
		return EAGAIN == error || EWOULDBLOCK == error || EINPROGRESS == error || EALREADY == error;
#endif
	}
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
	int32_t SocketInterface::pending() const {
		return 0;
	}
	bool SocketInterface::wantRead() const {
		return false;
	}
	bool SocketInterface::wantWrite() const {
		return false;
	}
	bool SocketInterface::getOption(int32_t level, int32_t option, void* result, socklen_t* len) {
		if (getsockopt(socket_, level, option, (char*)result, len)) {
			const int32_t error = WSAGetLastError();
			CRAZY_SYSTEM_ERROR() << "getOption sock = " << socket_ << " level = " << level << " option = " << option
				<< " errorno = " << error << " errstr = " << socketErrorText(error);
			return false;
		}
		return true;
	}
	bool SocketInterface::setOption(int32_t level, int32_t option, void* result, socklen_t len) {
		if (setsockopt(socket_, level, option, (char*)result, len)) {
			const int32_t error = WSAGetLastError();
			CRAZY_SYSTEM_ERROR() << "setOption sock = " << socket_ << " level = " << level << " option = " << option
				<< " errorno = " << error << " errstr = " << socketErrorText(error);
			return false;
		}
		return true;
	}
	socket_t SocketInterface::socket() const {
		return socket_;
	}
	bool SocketInterface::setNonBlocking(bool enabled) {
		if (INVALID_SOCKET == socket_) {
			return false;
		}
#if _WIN32
		u_long value = enabled ? 1 : 0;
		if (ioctlsocket(socket_, FIONBIO, &value) != 0) {
			const int32_t error = WSAGetLastError();
			CRAZY_SYSTEM_ERROR() << "set non blocking fail, sock = " << socket_ << " enabled = " << enabled
				<< " errno = " << error << " errstr = " << socketErrorText(error);
			return false;
		}
#else
		const int32_t flags = fcntl(socket_, F_GETFL, 0);
		if (flags < 0) {
			const int32_t error = errno;
			CRAZY_SYSTEM_ERROR() << "get socket flags fail, sock = " << socket_
				<< " errno = " << error << " errstr = " << socketErrorText(error);
			return false;
		}
		const int32_t target = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
		if (fcntl(socket_, F_SETFL, target) < 0) {
			const int32_t error = errno;
			CRAZY_SYSTEM_ERROR() << "set non blocking fail, sock = " << socket_ << " enabled = " << enabled
				<< " errno = " << error << " errstr = " << socketErrorText(error);
			return false;
		}
#endif
		return true;
	}
	void SocketInterface::initSocket(int32_t af, int32_t type, int32_t protocol) {
		if (!ensureWinSock()) {
			CRAZY_SYSTEM_ERROR() << "socket(" << af << ", " << type << ", " << protocol
				<< ") is not attempted, the socket subsystem is not initialized";
			return;
		}
		socket_ = ::socket(af, type, protocol);
		if (INVALID_SOCKET == socket_) {
			const int32_t error = WSAGetLastError();
			CRAZY_SYSTEM_ERROR() << "socket(" << af << ", " << type << ", " << protocol << ") fail, errno = " << error
				<< ", error message = " << socketErrorText(error);
			return;
		}
		active_.store(true);
	}
}
