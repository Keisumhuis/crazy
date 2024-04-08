#include "socket.h"
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

namespace crazy {

Socket::Ptr Socket::CreateTCP(Address::Ptr address) {
	Socket::Ptr sock(new Socket(address->GetFamily(), static_cast<int32_t>(SocketType::TCP), 0));
	return sock;
}
Socket::Ptr Socket::CreateUDP(Address::Ptr address) {
	Socket::Ptr sock(new Socket(address->GetFamily(), static_cast<int32_t>(SocketType::UDP), 0));
	return sock;
}
Socket::Ptr Socket::CreateTCPSocket() {
	Socket::Ptr sock(new Socket(static_cast<int32_t>(SocketFamily::IPv4)
				, static_cast<int32_t>(SocketType::TCP), 0));
	return sock;
}
Socket::Ptr Socket::CreateUDPSocket() {
	Socket::Ptr sock(new Socket(static_cast<int32_t>(SocketFamily::IPv4)
				, static_cast<int32_t>(SocketType::UDP), 0));
	return sock;
}
Socket::Ptr Socket::CreateTCPSocket6() {
	Socket::Ptr sock(new Socket(static_cast<int32_t>(SocketFamily::IPv6)
				, static_cast<int32_t>(SocketType::TCP), 0));
	return sock;
}
Socket::Ptr Socket::CreateUDPSocket6() {
	Socket::Ptr sock(new Socket(static_cast<int32_t>(SocketFamily::IPv6)
				, static_cast<int32_t>(SocketType::UDP), 0));
	return sock;
}
Socket::Ptr Socket::CreateUinxTCPSocket() {
	Socket::Ptr sock(new Socket(static_cast<int32_t>(SocketFamily::UNIX)
				, static_cast<int32_t>(SocketType::TCP), 0));
	return sock;
}
Socket::Ptr Socket::CreateUinxUDPSocket() {
	Socket::Ptr sock(new Socket(static_cast<int32_t>(SocketFamily::UNIX)
				, static_cast<int32_t>(SocketType::UDP), 0));
	return sock;
}
Socket::Socket(int32_t family, int32_t type, int32_t protocol) 
	: m_family(family)
	, m_type(type)
	, m_protocol(protocol)
	, m_socket(-1)
	, m_isConnected(false){
}
Socket::~Socket() {
	Close();
}
bool Socket::GetOption(int32_t level, int32_t option, void* result, socklen_t* len) {
	auto rt = getsockopt(m_socket, level, option, result ,len);
	if (rt) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "GetOption sock = " << m_socket
			<< " level = " << level << " option = " << option
			<< " errorno = " << errno << " errstr = " << strerror(errno);
		return false;	
	}
	return true;
}
bool Socket::SetOption(int32_t level, int32_t option, void* result, socklen_t len) {
	if (setsockopt(m_socket, level, option, result, len)) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "SetOption sock = " << m_socket
			<< " level = " << level << " option = " << option
			<< " errorno = " << errno << " errstr = " << strerror(errno); 
		return false;	
	}
	return true;
}
Socket::Ptr Socket::Accept() {
	Socket::Ptr sock(new Socket(m_family, m_type, m_protocol));
	auto rt = ::accept(m_socket, nullptr, nullptr);
	if (-1 == rt) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << " accept(" << m_socket << ") errno = " 
			<< errno << " errstr = " << strerror(errno);
		return nullptr;
	}
	if (sock->Init(rt)) {
		return sock;
	}
	return nullptr;
}
bool Socket::Bind(Address::Ptr addr) {
	if (!IsValid()) {
		NewSocket();
	}

	if (addr->GetFamily() != m_family) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "bind socket.family(" << m_family 
			<< " ), addr.family(" << addr->GetFamily() << " ) not equal";
		return false;
	}

	UnixAddress::Ptr uaddr = std::dynamic_pointer_cast<UnixAddress>(addr);
	if (uaddr) {
		Socket::Ptr sock = Socket::CreateUDPSocket();
		if (sock->Connect(addr)) {
			return false;
		}
	}

	if (::bind(m_socket, addr->GetAddr(), addr->GetAddrLen())) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "bind error errno = " << errno 
			<< " errstr = " << strerror(errno);
		return false;
	}
	GetLocalAddress();
	return true;
}
bool Socket::Connect(const Address::Ptr addr, uint64_t timeout_ms) {
	m_remoteAddress = addr;
	if (!IsValid()) {
		NewSocket();
	}

	if (addr->GetFamily() != m_family) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "Connect socket.family(" << m_family 
			<< "), addr.family = " << addr->GetFamily() << " no equal";
		return false;
	}
	if (::connect(m_socket, addr->GetAddr(), addr->GetAddrLen())) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "socket = " << m_socket << "errorno = " 
			<< errno << " errstr = " << strerror(errno);
		return false;
	}
	m_isConnected = true;
	GetRemoteAddress();
	GetLocalAddress();
	return true;
}
bool Socket::ReConnect(uint64_t timeout_ms) {
	if (!m_remoteAddress) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "ReConnect m_remoteAddress is null";
		return false;
	}
	m_localAddress.reset();
	return Connect(m_remoteAddress);
}
bool Socket::Listen(int32_t backlog) {
	if (!IsValid()) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "listen error socket = 1";
		return false;
	}
	if (::listen(m_socket, backlog)) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "listen error errno = " << errno
			<< " errstr = " << strerror(errno);
		return false;	
	}
	return true;
}
bool Socket::Close() {
	if (m_socket == -1 && !m_isConnected) {
		return true;
	}
	m_isConnected = false;
	if (m_socket != -1) {
		::close(m_socket);
		m_socket = -1;
	}
	return false;
}
int32_t Socket::Send(const void* buffer, size_t length, int32_t flags) {
	if (!IsConnect()) {
		return -1;
	}
	return ::send(m_socket, buffer, length, flags);
}
int32_t Socket::Send(const iovec* buffers, size_t length, int32_t flags) {
	if (!IsConnect()) {
		return -1;
	}
	msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = (iovec*)buffers;
	msg.msg_iovlen = length;
	return ::sendmsg(m_socket, &msg, flags);
}
int32_t Socket::SendTo(const void* buffer, size_t length, const Address::Ptr addr, int32_t flags) {
	if (!IsConnect()) {
		return -1;
	}
	return ::sendto(m_socket, buffer, length, flags, addr->GetAddr(), addr->GetAddrLen());
}
int32_t Socket::SendTo(const iovec* buffers, size_t length, const Address::Ptr addr, int32_t flags) {
	if (!IsConnect()) {
		return -1;
	}
	msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = (iovec*)buffers;
	msg.msg_iovlen = length;
	msg.msg_name = addr->GetAddr();
	msg.msg_namelen = addr->GetAddrLen();
	return sendmsg(m_socket, &msg, flags);
}
int32_t Socket::Recv(void* buffer, size_t length, int32_t flags) {
	if (!IsConnect()) {
		return -1;
	}
	return ::recv(m_socket, buffer, length, flags);
}
int32_t Socket::Recv(iovec* buffer, size_t length, int32_t flags) {
	if (!IsConnect()) {
		return -1;
	}
	msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = buffer;
	msg.msg_iovlen = length;
	return ::recvmsg(m_socket, &msg, flags);
}
int32_t Socket::RecvFrom(void* buffer, size_t length, Address::Ptr addr, int32_t flags) {
	if (!IsConnect()) {
		return -1;
	}
	socklen_t len = addr->GetAddrLen();
	return ::recvfrom(m_socket, buffer, length, flags, addr->GetAddr(), &len);
}
int32_t Socket::RecvFrom(iovec* buffers, size_t length, Address::Ptr addr, int32_t flags) {
	if (!IsConnect()) {
		return -1;
	}
	msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = buffers;
	msg.msg_iovlen = length;
	msg.msg_name = addr->GetAddr();
	msg.msg_namelen = addr->GetAddrLen();
	return ::recvmsg(m_socket, &msg, flags);
}
Address::Ptr Socket::GetRemoteAddress() {
	if (m_remoteAddress) {
		return m_remoteAddress;
	}

	Address::Ptr result;
	switch(static_cast<SocketFamily>(m_family)) {
	case SocketFamily::IPv4:
		result.reset(new IPv4Address());
		break;
	case SocketFamily::IPv6:
		result.reset(new IPv6Address());
		break;
	case SocketFamily::UNIX:
		result.reset(new UnixAddress());
		break;
	default:
		throw std::logic_error("unknow socket family, family = " + m_family);
	}

	socklen_t addrlen = result->GetAddrLen();
	if (getpeername(m_socket, const_cast<sockaddr*>(result->GetAddr()), &addrlen)) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "getpeername(" << m_socket << ") fail, errno"
			<< errno << " errstr = " << strerror(errno);
		return nullptr;
	}
	m_remoteAddress = result;
	return m_remoteAddress;
}
Address::Ptr Socket::GetLocalAddress() {
	if (m_localAddress) {
		return m_localAddress;
	}

	Address::Ptr result;
	switch(static_cast<SocketFamily>(m_family)) {
	case SocketFamily::IPv4:
		result.reset(new IPv4Address());
		break;
	case SocketFamily::IPv6:
		result.reset(new IPv6Address());
		break;
	case SocketFamily::UNIX:
		result.reset(new UnixAddress());
		break;
	default:
		throw std::logic_error("unknow socket family, family = " + m_family);
	}

	socklen_t addrlen = result->GetAddrLen();
	if (getsockname(m_socket, const_cast<sockaddr*>(result->GetAddr()), &addrlen)) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "getsockname(" << m_socket << ") fail, errno"
			<< errno << " errstr = " << strerror(errno);
		return nullptr;
	}
	m_localAddress = result;
	return m_localAddress;
}
int32_t Socket::GetFamily() const {
	return m_family;
}
int32_t Socket::GetType() const {
	return m_type;
}
int32_t Socket::GetProtocol() const {
	return m_protocol;
}
bool Socket::IsConnect() const {
	return m_isConnected;
}
bool Socket::IsValid() const {
	return m_socket != -1;
}
int32_t Socket::GetError() {
	int32_t error = 0;
	socklen_t len = sizeof(error);
	if (GetOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
		error = errno;
	}
	return error;
}
int32_t Socket::GetSocket() const {
	return m_socket;
}
bool Socket::CancelRead() {
}
bool Socket::CancelWrite() {
}
bool Socket::CancelAccept() {
}
bool Socket::CancelAll() {
}
void Socket::InitSocket() {
	int32_t val = 1;
	SetOption(SOL_SOCKET, SO_REUSEADDR, val);
	if (m_type == SOCK_STREAM) {
		SetOption(IPPROTO_TCP, TCP_NODELAY, val);	
	}
}
void Socket::NewSocket() {
	m_socket = ::socket(m_family, m_type, m_protocol);
	if (-1 != m_socket) {
		InitSocket();
	} else {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "socket(" << m_family << ", "
			<< m_type << ", " << m_protocol << "), fail errno = " << errno
		 	<< " errstr = " << strerror(errno);
	}	
}
bool Socket::Init(int32_t socket) {
	m_socket = socket;
	m_isConnected = true;
	InitSocket();
	GetRemoteAddress();
	GetLocalAddress();
	return true;
}

SSLSocket::Ptr SSLSocket::CreateTCP(Address::Ptr addr) {
	SSLSocket::Ptr sock(new SSLSocket(addr->GetFamily(), static_cast<int32_t>(SocketType::TCP), 0));
	return sock;
}
SSLSocket::Ptr SSLSocket::CreateTCPSocket() {
	SSLSocket::Ptr sock(new SSLSocket(static_cast<int32_t>(SocketFamily::IPv4), static_cast<int32_t>(SocketType::TCP), 0));
	return sock;
}
SSLSocket::Ptr SSLSocket::CreateTCPSocket6() {
	SSLSocket::Ptr sock(new SSLSocket(static_cast<int32_t>(SocketFamily::IPv6), static_cast<int32_t>(SocketType::TCP), 0));
	return sock;
}
SSLSocket::SSLSocket(int32_t family, int32_t type, int32_t protocol) 
	: Socket(family, type, protocol){
}	
Socket::Ptr SSLSocket::Accept() {
	SSLSocket::Ptr sock(new SSLSocket(m_family, m_type, m_protocol));
	int32_t newsock = ::accept(m_socket, nullptr, nullptr);
	if (-1 == newsock) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "accept(" << m_socket << "), errno = "
			<< errno << " errstr = " << strerror(errno);
		return nullptr;
	}
	sock->m_ctx = m_ctx;
	if (sock->Init(newsock)) {
		return sock;
	}
	return nullptr;
}
bool SSLSocket::Bind(Address::Ptr addr) {
	return Socket::Bind(addr);
}
bool SSLSocket::Connect(const Address::Ptr addr, uint64_t timeout_ms) {
	auto rt =  Socket::Connect(addr, timeout_ms);
	if (!rt) {
		return rt;
	}
	m_ctx.reset(SSL_CTX_new(SSLv23_client_method()), SSL_CTX_free);
	m_ssl.reset(SSL_new(m_ctx.get()), SSL_free);
	SSL_set_fd(m_ssl.get(), m_socket);
	rt = (SSL_connect(m_ssl.get()) == 1);
}
bool SSLSocket::Listen(int32_t backlog) {
	return Socket::Listen(backlog);
}
bool SSLSocket::Close() {
	return Socket::Close();
}
int32_t SSLSocket::Send(const void* buffer, size_t length, int32_t flags) {
	if (m_ssl) {
		return SSL_write(m_ssl.get(), buffer, length);
	}
	return -1;
}
int32_t SSLSocket::Send(const iovec* buffers, size_t length, int32_t flags) {
	if (!m_ssl) {
        	return -1;
    	}
    	int32_t total = 0;
    	for (size_t i = 0; i < length; ++i) {
        	int tmp = SSL_write(m_ssl.get(), buffers[i].iov_base, buffers[i].iov_len);
        	if (tmp <= 0) {
            		return tmp;
        	}
        	total += tmp;
        	if (tmp != (int)buffers[i].iov_len) {
            		break;
        	}
    	}
    	return total;
}
int32_t SSLSocket::SendTo(const void* buffer, size_t length, const Address::Ptr addr,int32_t flags) {
	CRAZY_ASSERT(false);
	return -1;
}
int32_t SSLSocket::SendTo(const iovec* buffers, size_t length, const Address::Ptr addr, int32_t flags) {
	CRAZY_ASSERT(false);
	return -1;
}
int32_t SSLSocket::Recv(void* buffer, size_t length, int32_t flags) {
	if (m_ssl) {
        	return SSL_read(m_ssl.get(), buffer, length);
    	}
   	return -1;
}
int32_t SSLSocket::Recv(iovec* buffer, size_t length, int32_t flags) {
	if (!m_ssl) {
        	return -1;
    	}
    	int total = 0;
    	for (size_t i = 0; i < length; ++i) {
        	int32_t tmp = SSL_read(m_ssl.get(), buffer[i].iov_base, buffer[i].iov_len);
        	if (tmp <= 0) {
            		return tmp;
        	}
        	total += tmp;
        	if (tmp != (int)buffer[i].iov_len) {
            		break;
        	}
    	}
    	return total;
}
int32_t SSLSocket::RecvFrom(void* buffer, size_t length, Address::Ptr addr, int32_t flags) {
	CRAZY_ASSERT(false);
	return -1;
}
int32_t SSLSocket::RecvFrom(iovec* buffers, size_t length, Address::Ptr addr, int32_t flags) {
	CRAZY_ASSERT(false);
	return -1;
}
bool SSLSocket::LoadCertificates(const std::string& cert_file, const std::string& key_file) {
	m_ctx.reset(SSL_CTX_new(SSLv23_server_method()), SSL_CTX_free);
    	if (SSL_CTX_use_certificate_chain_file(m_ctx.get(), cert_file.c_str()) != 1) {
        	CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "SSL_CTX_use_certificate_chain_file("
            		<< cert_file << ") error";
        	return false;
    	}
    	if (SSL_CTX_use_PrivateKey_file(m_ctx.get(), key_file.c_str(), SSL_FILETYPE_PEM) != 1) {
       		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "SSL_CTX_use_PrivateKey_file("
            		<< key_file << ") error";
        	return false;
    	}
    	if (SSL_CTX_check_private_key(m_ctx.get()) != 1) {
        	CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "SSL_CTX_check_private_key cert_file="
            		<< cert_file << " key_file=" << key_file;
        	return false;
    	}
    	return true;
}
bool SSLSocket::Init(int32_t socket) {
	auto v = Socket::Init(socket);
	if (!v) {
		return v;
	}
	m_ssl.reset(SSL_new(m_ctx.get()),  SSL_free);
        SSL_set_fd(m_ssl.get(), m_socket);
        v = (SSL_accept(m_ssl.get()) == 1);
	return v;
}
}
