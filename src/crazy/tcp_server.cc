#include "tcp_server.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

namespace crazy {

TcpServer::TcpServer() 
	: m_ssl (false)
	, m_stopping (true)
       	, m_name ("crazy/0.0.1"){
	
}
TcpServer::~TcpServer() {
	CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "destory";
	for (auto& it : m_socks) {
		it->Close();
	}
	m_socks.clear();
}
bool TcpServer::Bind(Address::Ptr addr, bool ssl) {
	std::vector<Address::Ptr> addrs;
	std::vector<Address::Ptr> fails;
	addrs.push_back(addr);
	Bind(addrs, fails, ssl);
	return fails.empty() ? false : true;
}
void TcpServer::Bind(const std::vector<Address::Ptr>& addrs
		, std::vector<Address::Ptr>& fails
		, bool ssl) {
	m_ssl = ssl;
	for (auto& addr : addrs) {
		Socket::Ptr sock = ssl ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
		if (!sock->Bind(addr)) {
			CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "listen fail errno = " << errno
				<< " errstr = " << strerror(errno) << " addr = [" 
				<< addr->ToString() << "]";	
			fails.push_back(addr);
			continue;
		}
		if (!sock->Listen()) {
			CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "listen fail errno = " << errno
				<< " errstr = " << strerror(errno) << " addr = [" 
				<< addr->ToString() << "]";	
			fails.push_back(addr);
			continue;
		}
		m_socks.insert(sock);
	}
	
	if (!fails.empty()) {
		m_socks.clear();
	}
}
bool TcpServer::LoadCertificates(const std::string& cert_file, const std::string& key_file) {
	for (auto& it : m_socks) {
		auto ssl_socket = std::dynamic_pointer_cast<SSLSocket>(it);
		if (ssl_socket) {
			if (!ssl_socket->LoadCertificates(cert_file, key_file)) {
				return false;
			}
		}
	}
	return true;
}
bool TcpServer::Start() {
	m_stopping = false;
	for (auto& it : m_socks) {
		SELECTOR().RegisterEvent(it, SelectEvent::Read, std::bind(&TcpServer::OnAccept, shared_from_this(), it));
	}
	return true;
}
bool TcpServer::Stop() {
	m_stopping = true;
	for (auto& it : m_socks) {
		SELECTOR().CancelAllEvent(it);
	}
	return true;
}
bool TcpServer::IsStoppoing() const {
	return m_stopping;
}
std::set<Socket::Ptr> TcpServer::GetSockets() const {
	return m_socks;
}
std::string TcpServer::GetName() const {
	return m_name;
}
void TcpServer::SetName(const std::string& name) {
	m_name = name;
}
void TcpServer::OnClient(Socket::Ptr sock) {
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "on client: " << sock->GetRemoteAddress()->ToString();
}
void TcpServer::OnAccept(Socket::Ptr sock) {
	auto client = sock->Accept();
	if (client) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "new client connected, sock = " << client->GetSocket()
			<< " local address : " << client->GetLocalAddress()->ToString()
			<< " remote address : " << client->GetRemoteAddress()->ToString();
			SELECTOR().RegisterEvent(client, SelectEvent::Read, std::bind(&TcpServer::OnClient, shared_from_this(), client));
	} else {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "accept error, errno = " << errno
			<< " errstr = " << strerror(errno);
	}
}
}
