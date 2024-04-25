/**
 * @file socket_stream.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_STREAMS_SOCKET_STREAM_H____
#define ____CRAZY_STREAMS_SOCKET_STREAM_H____

#include "memory"

#include "crazy/socket.h"
#include "crazy/stream.h"

namespace crazy::stream {

    class SocketStream : public Stream {
    public:
        using Ptr = std::shared_ptr<SocketStream>;
        SocketStream(Socket::Ptr sock);
        virtual ~SocketStream() {
            m_sock->Close();
        }
        virtual int32_t Read(void* buffer, size_t length) override;
		virtual int32_t Write(const void* buffer, size_t length) override;
        virtual void Close();
        Socket::Ptr GetSocket() const;
        Address::Ptr GetRemoteAddress();
        Address::Ptr GetLocalAddress();
        std::string GetRemoteAddressString();
        std::string GetLoaclAddressString();
    private:
        Socket::Ptr m_sock;
    };

}

#endif // ! ____CRAZY_STREAMS_SOCKET_STREAM_H____