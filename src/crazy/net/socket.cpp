#include "crazy/net/socket.h"

#include <string.h>

#include "crazy/logger.h"

namespace crazy {
	namespace {
		/**
		 * @brief 去掉 IPv6 字面量的方括号.
		 * @return 方括号不配对时返回 false.
		 */
		bool stripBrackets(const std::string& address, std::string& literal) {
			if (address.empty()) {
				return false;
			}
			if (address.front() == '[') {
				if (address.back() != ']' || address.size() < 3) {
					return false;
				}
				literal = address.substr(1, address.size() - 2);
				return true;
			}
			if (address.find('[') != std::string::npos || address.find(']') != std::string::npos) {
				return false;
			}
			literal = address;
			return true;
		}
		/**
		 * @brief 按字面量内容判定地址族.
		 * @note 非字面量主机名一律返回 `AF_INET`，保持「主机名只产出 IPv4 候选」的既有策略.
		 */
		int32_t literalAddressFamily(const std::string& literal) {
			in6_addr address6;
			if (literal.find(':') != std::string::npos && inet_pton(AF_INET6, literal.c_str(), &address6) == 1) {
				return AF_INET6;
			}
			return AF_INET;
		}
		/**
		 * @brief 是否形如 IPv4 点分十进制字面量（四段、全数字）.
		 * @note 用于把「999.1.1.1」这类非法字面量直接判错，而不是丢给 `getaddrinfo` 当主机名做 DNS 查询.
		 */
		bool looksLikeIpv4Literal(const std::string& host) {
			int32_t parts = 1;
			for (const auto ch : host) {
				if (ch == '.') {
					++parts;
					continue;
				}
				if (ch < '0' || ch > '9') {
					return false;
				}
			}
			return parts == 4;
		}
		/**
		 * @brief 解析绑定地址.
		 * @note 只接受字面量地址，与既有 `inet_addr` 的行为范围一致（不接受主机名）.
		 */
		bool parseBindSockaddr(const std::string& address, sockaddr_storage& storage, socklen_t& length, int32_t& family) {
			std::string literal;
			if (!stripBrackets(address, literal)) {
				return false;
			}
			memset(&storage, 0, sizeof(storage));
			if (AF_INET6 == literalAddressFamily(literal)) {
				auto* address6 = reinterpret_cast<sockaddr_in6*>(&storage);
				if (inet_pton(AF_INET6, literal.c_str(), &address6->sin6_addr) != 1) {
					return false;
				}
				address6->sin6_family = AF_INET6;
				family = AF_INET6;
				length = sizeof(sockaddr_in6);
				return true;
			}
			auto* address4 = reinterpret_cast<sockaddr_in*>(&storage);
			if (inet_pton(AF_INET, literal.c_str(), &address4->sin_addr) != 1) {
				return false;
			}
			address4->sin_family = AF_INET;
			family = AF_INET;
			length = sizeof(sockaddr_in);
			return true;
		}
	}

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
			const int32_t error = WSAGetLastError();
			CRAZY_SYSTEM_ERROR() << "accept error errno = " << error << " errstr = " << socketErrorText(error);
			return nullptr;
		}
		auto clientSocket = createAcceptedSocket(acceptedSocket);
		if (!clientSocket) {
			closesocket(acceptedSocket);
			return nullptr;
		}
		clientSocket->updateLocalAddress();
		clientSocket->updateRemoteAddress();
		return clientSocket;
	}
	Socket::ptr Socket::createAcceptedSocket(socket_t acceptedSocket) {
		return Socket::ptr(new Socket(acceptedSocket));
	}
	bool Socket::listen(uint64_t port, const std::string& address, bool dualStack) {
		sockaddr_storage listenSockaddr;
		socklen_t addressLength = 0;
		int32_t family = AF_INET;
		if (!parseBindSockaddr(address, listenSockaddr, addressLength, family)) {
			CRAZY_SYSTEM_ERROR() << "invalid ip address: " << address;
			return false;
		}

		if (AF_INET6 == family) {
			reinterpret_cast<sockaddr_in6*>(&listenSockaddr)->sin6_port = htons(static_cast<uint16_t>(port));
		}
		else {
			reinterpret_cast<sockaddr_in*>(&listenSockaddr)->sin_port = htons(static_cast<uint16_t>(port));
		}

		close();
		initSocket(family, SOCK_STREAM, IPPROTO_TCP);
		if (!active()) {
			CRAZY_SYSTEM_ERROR() << "create socket fail, address = " << address;
			return false;
		}

		if (AF_INET6 == family) {
			// 「仅 IPv6」(1) 与「双栈」(0) 都必须显式设置，且必须在 bind() 之前：
			// Windows 默认 V6ONLY = 1，Linux 默认由 /proc/sys/net/ipv6/bindv6only 决定；
			// 设置失败按严格模式返回 false，不静默退化为平台默认行为。
			int32_t v6Only = dualStack ? 0 : 1;
			if (!setOption(IPPROTO_IPV6, IPV6_V6ONLY, v6Only)) {
				CRAZY_SYSTEM_ERROR() << "set IPV6_V6ONLY = " << v6Only << " fail, address = " << address;
				close();
				return false;
			}
		}

		if (::bind(socket_, (sockaddr*)&listenSockaddr, addressLength) != 0) {
			const int32_t error = WSAGetLastError();
			CRAZY_SYSTEM_ERROR() << "bind error errno = " << error << " errstr = " << socketErrorText(error);
			close();
			return false;
		}

		if (::listen(socket_, SOMAXCONN) != 0) {
			const int32_t error = WSAGetLastError();
			CRAZY_SYSTEM_ERROR() << "listen error errno = " << error << " errstr = " << socketErrorText(error);
			close();
			return false;
		}

		updateLocalAddress();
		return true;
	}
	bool Socket::connect(const std::string& address, uint64_t port) {
		// `getaddrinfo` 也要求套接字运行时已经初始化（Windows 上即 `WSAStartup`）
		if (!ensureWinSock()) {
			return false;
		}
		std::string host;
		if (!stripBrackets(address, host)) {
			host = address;
		}

		addrinfo hints = {};
		// 字面量按内容判定地址族；非字面量主机名保持「只产出 IPv4 候选」的既有策略.
		hints.ai_family = literalAddressFamily(host);
		hints.ai_socktype = SOCK_STREAM;

		// 形如 IPv4 点分十进制但非法的输入直接判错，避免被当作主机名去做 DNS 查询.
		if (AF_INET == hints.ai_family && looksLikeIpv4Literal(host)) {
			in_addr address4;
			if (inet_pton(AF_INET, host.c_str(), &address4) != 1) {
				CRAZY_SYSTEM_ERROR() << "invalid ip address: " << address;
				return false;
			}
		}

		addrinfo* result = nullptr;
		const std::string portString = std::to_string(port);
		if (getaddrinfo(host.c_str(), portString.c_str(), &hints, &result) != 0) {
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
			const int32_t error = WSAGetLastError();
			CRAZY_SYSTEM_ERROR() << "socket connect fail, addr = " << address << ":" << port
				<< ", errorno = " << error << " errstr = " << socketErrorText(error);
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
	void Socket::updateLocalAddress() {
		sockaddr_storage address;
		socklen_t len = sizeof(address);
		if (getsockname(socket_, (sockaddr*)&address, &len)) {
			const int32_t error = WSAGetLastError();
			CRAZY_SYSTEM_ERROR() << "getsockname(" << socket_ << ") fail, errno = " << error << " errstr = " << socketErrorText(error);
			return;
		}
		localAddress_ = sockaddr2string((const sockaddr*)&address);
	}
	void Socket::updateRemoteAddress() {
		sockaddr_storage address;
		socklen_t len = sizeof(address);
		if (getpeername(socket_, (sockaddr*)&address, &len)) {
			const int32_t error = WSAGetLastError();
			CRAZY_SYSTEM_ERROR() << "getpeername(" << socket_ << ") fail, errno = " << error << " errstr = " << socketErrorText(error);
			return;
		}
		remoteAddress_ = sockaddr2string((const sockaddr*)&address);
	}
	std::string Socket::sockaddr2string(const sockaddr* address) {
		if (address == nullptr) {
			return std::string();
		}

		char buffer[INET6_ADDRSTRLEN] = {};
		std::stringstream ss;
		if (AF_INET6 == address->sa_family) {
			const auto* address6 = reinterpret_cast<const sockaddr_in6*>(address);
			// 双栈下 IPv4 对端是 IPv4-mapped 地址（RFC 4291 的 ::ffff:0:0/96），
			// 归一化为 IPv4 形式，不把 ::ffff: 前缀泄漏给业务与日志.
			if (IN6_IS_ADDR_V4MAPPED(&address6->sin6_addr)) {
				in_addr mapped = {};
				memcpy(&mapped, reinterpret_cast<const uint8_t*>(&address6->sin6_addr) + 12, sizeof(mapped));
				if (inet_ntop(AF_INET, &mapped, buffer, sizeof(buffer)) == nullptr) {
					return std::string();
				}
				ss << "tcp://" << buffer << ":" << ntohs(address6->sin6_port);
				return ss.str();
			}
			if (inet_ntop(AF_INET6, &address6->sin6_addr, buffer, sizeof(buffer)) == nullptr) {
				return std::string();
			}
			ss << "tcp://[" << buffer << "]:" << ntohs(address6->sin6_port);
			return ss.str();
		}
		if (AF_INET == address->sa_family) {
			const auto* address4 = reinterpret_cast<const sockaddr_in*>(address);
			if (inet_ntop(AF_INET, &address4->sin_addr, buffer, sizeof(buffer)) == nullptr) {
				return std::string();
			}
			ss << "tcp://" << buffer << ":" << ntohs(address4->sin_port);
			return ss.str();
		}
		return std::string();
	}
}
