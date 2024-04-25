#include "http_connection.h"

namespace crazy::http {

HttpResult::Ptr HttpConnection::Get(const std::string& url
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers
                , const std::string& body) {
    auto uri = Uri::Create(url);
    if (!uri) {
        return HttpResult::Ptr(new HttpResult(ResultError::INVALID_URL
            , nullptr
            , "invalid url = " + url));
    }
    return Request(HttpMethod::GET, uri, timeout, headers, body);
}
HttpResult::Ptr HttpConnection::Get(const Uri::Ptr uri
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers
                , const std::string& body) {
    return Request(HttpMethod::GET, uri, timeout, headers, body);
}
HttpResult::Ptr HttpConnection::Post(const std::string& url
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers
                , const std::string& body) {
    auto uri = Uri::Create(url);
    if (!uri) {
        return HttpResult::Ptr(new HttpResult(ResultError::INVALID_URL
            , nullptr
            , "invalid url = " + url));
    }
    return Request(HttpMethod::POST, uri, timeout, headers, body);
}
HttpResult::Ptr HttpConnection::Post(const Uri::Ptr uri
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers
                , const std::string& body) {
    return Request(HttpMethod::GET, uri, timeout, headers, body);
}
HttpResult::Ptr HttpConnection::Request(const HttpMethod method
                , const std::string& url
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers
                , const std::string& body) {
    auto uri = Uri::Create(url);
    if (!uri) {
        return HttpResult::Ptr(new HttpResult(ResultError::INVALID_URL
            , nullptr
            , "invalid url = " + url));
    }
    return Request(method, uri, timeout, headers, body);
}
HttpResult::Ptr HttpConnection::Request(const HttpMethod method
                , const Uri::Ptr uri
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers
                , const std::string& body) {
    Request::Ptr request = std::make_shared<crazy::http::Request>();
    request->SetPath(uri->GetPath());
    request->SetQuery(uri->GetQuery());
    request->SetFragment(uri->GetFragment());
    request->SetMethod(method);
    bool has_host = false;
    for (auto& [key, val] : headers) {
        if (strcasecmp(key.c_str(), "connection") == 0) {
            if (strcasecmp(val.c_str(), "keep-alive") == 0) {
                request->SetKeepALive(true);
            }
            continue;
        }

        if (!has_host && strcasecmp(key.c_str(), "host") == 0) {
            has_host = !val.empty();
        }

        request->SetHeader(key, val);
    }

    if (!has_host) {
        request->SetHeader("host", uri->GetHost());
    }
    request->SetBody(body);
    return Request(request, uri, timeout);
}
HttpResult::Ptr HttpConnection::Request(Request::Ptr request
                , Uri::Ptr uri
                , uint64_t timeout) {
    bool is_ssl = uri->GetScheme() == "https";
    auto addr = uri->CreateAddress();
    if (!addr) {
        return HttpResult::Ptr(new HttpResult(ResultError::INVALID_HOST
            , nullptr, "invalid host = " + uri->GetHost()));
    }
    Socket::Ptr sock = is_ssl ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
    if (!sock) {
        return HttpResult::Ptr(new HttpResult(ResultError::CREATE_SOCKET_ERROR
            , nullptr, "create socket fail" + addr->ToString()
                + " errno = " + std::to_string(errno)
                + " errstr = " + std::string{strerror(errno)}));
    }
    if (!sock->Connect(addr)) {
        return HttpResult::Ptr(new HttpResult(ResultError::CONNECT_FAIL
            , nullptr, "connect fail = " + addr->ToString()));
    }
    HttpConnection::Ptr conn(new HttpConnection(sock));
    auto rt = conn->SendRequest(request);
    if (rt == 0) {
        return HttpResult::Ptr(new HttpResult(ResultError::SEND_CLOSE_BY_PEER
            , nullptr, "send request closed by peer = " + addr->ToString()));
    }
    if (rt < 0) {
        return HttpResult::Ptr(new HttpResult(ResultError::SEND_SOCKET_ERROR
            , nullptr, "send request socket error errno=" + std::to_string(errno)
                    + " errstr=" + std::string(strerror(errno))));
    }
    auto rsp = conn->RecvResponse();
    if (!rsp) {
        return HttpResult::Ptr(new HttpResult(ResultError::TIMEOUT
            , nullptr, "recv response timeout: " + addr->ToString()
                    + " timeout_ms:" + std::to_string(timeout)));
    }
    return HttpResult::Ptr(new HttpResult(ResultError::OK, rsp, "ok"));
}
HttpConnection::HttpConnection(Socket::Ptr sock)
    : stream::SocketStream(sock) {}
HttpConnection::~HttpConnection() {}
Response::Ptr HttpConnection::RecvResponse() {
    ResponseParser::Ptr parser(new ResponseParser);
    uint64_t buff_size = ResponseParser::GetHttpResponseBufferSize();
    std::shared_ptr<char> buffer(new char[buff_size + 1], [](char* ptr){delete[] ptr;});
    char* data = buffer.get();
    int offset = 0;
    do {
        int len = Read(data + offset, buff_size - offset);
        if(len <= 0) {
            Close();
            return nullptr;
        }
        len += offset;
        size_t nparse = parser->Execute(data, len, false);
        if(parser->HasError()) {
            Close();
            return nullptr;
        }
        offset = len - nparse;
        if(offset == (int)buff_size) {
            Close();
            return nullptr;
        }
        if(parser->IsFinished()) {
            break;
        }
    } while(true);
    auto& client_parser = parser->GetParser();
    std::string body;
    if(client_parser.chunked) {
        int len = offset;
        do {
            bool begin = true;
            do {
                if(!begin || len == 0) {
                    int rt = Read(data + len, buff_size - len);
                    if(rt < 0) {
                        Close();
                        return nullptr;
                    }
                    len += rt;
                }
                data[len] = '\0';
                size_t nparse = parser->Execute(data, len, true);
                if(parser->HasError()) {
                    Close();
                    return nullptr;
                }
                len -= nparse;
                if(len == (int)buff_size) {
                    Close();
                    return nullptr;
                }
                begin = false;
            } while(!parser->IsFinished());
            if(client_parser.content_len + 2 <= len) {
                body.append(data, client_parser.content_len);
                memmove(data, data + client_parser.content_len + 2
                        , len - client_parser.content_len - 2);
                len -= client_parser.content_len + 2;
            } else {
                body.append(data, len);
                int left = client_parser.content_len - len + 2;
                while(left > 0) {
                    int rt = Read(data, left > (int)buff_size ? (int)buff_size : left);
                    if(rt < 0) {
                        Close();
                        return nullptr;
                    }
                    body.append(data, rt);
                    left -= rt;
                }
                body.resize(body.size() - 2);
                len = 0;
            }
        } while(!client_parser.chunks_done);
    } else {
        int64_t length = parser->GetContentLength();
        if(length > 0) {
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
                if(ReadFixSize(&body[len], length) < 0) {
                    Close();
                    return nullptr;
                }
            }
        }
    }
    if(!body.empty()) {
        auto content_encoding = parser->GetData()->GetHeader("content-encoding");
        parser->GetData()->SetBody(body);
    }
    return parser->GetData();
}
int32_t HttpConnection::SendRequest(Request::Ptr req) {
    std::string data = req->ToString();
    return WriteFixSize(data.c_str(), data.size());
}
}