#include "socket_stream.h"

namespace crazy::stream {
    
    SocketStream::SocketStream(Socket::Ptr sock) 
        :m_sock (sock) {
    }
    int32_t SocketStream::Read(void* buffer, size_t length) {
        if (!m_sock->IsConnect()) {
            return -1;
        }
        return m_sock->Recv(buffer, length);
    }
	int32_t SocketStream::Write(const void* buffer, size_t length) {
        if (!m_sock->IsConnect()) {
            return -1;
        }
        return m_sock->Send(buffer, length);
    }
    void SocketStream::Close() {
        if (m_sock) {
            m_sock->Close();
        }
    }
    Socket::Ptr SocketStream::GetSocket() const {
        return m_sock;
    }
    Address::Ptr SocketStream::GetRemoteAddress() {
        if (!m_sock) {
            return nullptr;
        }
        return m_sock->GetRemoteAddress();
    }
    Address::Ptr SocketStream::GetLocalAddress() {
        if (!m_sock) {
            return nullptr;
        }
        return m_sock->GetLocalAddress();
    }
    std::string SocketStream::GetRemoteAddressString() {
        auto addr = GetRemoteAddress();
        if (!addr) {
            return "";
        }
        return addr->ToString();
    }
    std::string SocketStream::GetLoaclAddressString() {
        auto addr = GetLocalAddress();
        if (!addr) {
            return "";
        }
        return addr->ToString();
    }
}