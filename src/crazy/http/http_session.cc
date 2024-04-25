#include "http_session.h"

namespace crazy::http {
    HttpSession::HttpSession(Socket::Ptr sock)
        : stream::SocketStream(sock) {}
    Request::Ptr HttpSession::RecvRequest() {
        RequestParser::Ptr parser(new RequestParser);
        uint64_t buffer_size = RequestParser::GetHttpRequestBufferSize();
        std::shared_ptr<char> buffer(new char[buffer_size], [](char* ptr){ delete[] ptr;});
        char* data = buffer.get();
        int32_t offset = 0;
        do {
            int32_t len = Read(data + offset, buffer_size - offset);
            if(len <= 0) {
                return nullptr;
            }
            len += offset;
            size_t nparse = parser->Execute(data, len);
            if(parser->HasError()) {
                return nullptr;
            }
            offset = len - nparse;
            if(offset == (int)buffer_size) {
                return nullptr;
            }
            if(parser->IsFinished()) {
                break;
            }
        } while (true);
        int64_t length = parser->GetContentLength();
        if (length > RequestParser::GetHttpRequestMaxBodySize()) {
            return nullptr;
        }
        if (length > 0) {
            std::string body;
            body.resize(length);

            int len = 0;
            if(length >= offset) {
                memcpy(&body[0], data, offset);
                len = offset;
            } else {
                memcpy(&body[0], data, length);
                len = length;
            }
            length -= offset;
            if(length > 0) {
                if(ReadFixSize(&body[len], length) <= 0) {
                    return nullptr;
                }
            }
            parser->GetData()->SetBody(body);
        }
        return parser->GetData();
    }
    int32_t HttpSession::SendResponse(Response::Ptr res) {
        std::string str = res->ToString();
        return WriteFixSize(str.c_str(), str.size());
    }
} 