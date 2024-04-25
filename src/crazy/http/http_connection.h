/**
 * @file http_connection.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_HTTP_HTTP_CONNECTION_H____
#define ____CRAZY_HTTP_HTTP_CONNECTION_H____

#include <memory>

#include "crazy/streams/socket_stream.h"
#include "crazy/uri.h"
#include "request.h"
#include "response.h"

namespace crazy::http {
    
    enum class ResultError {
        OK ,
        INVALID_URL,
        INVALID_HOST,
        CONNECT_FAIL,
        SEND_CLOSE_BY_PEER,
        SEND_SOCKET_ERROR,
        TIMEOUT,
        CREATE_SOCKET_ERROR,
        POOL_GET_CONNECTION,
        POOL_INVALID_CONNECTION
    };

    struct HttpResult {
        using Ptr = std::shared_ptr<HttpResult>;
        HttpResult(ResultError _error, Response::Ptr _response, const std::string& _errstr)
            : error(_error) , response(_response), errstr(_errstr) {}
        ResultError error;
        Response::Ptr response;
        std::string errstr;
    };

    class HttpConnection : public stream::SocketStream {
    public:
        using Ptr = std::shared_ptr<HttpConnection>;
        static HttpResult::Ptr Get(const std::string& url
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers = {}
                , const std::string& body = "");
        static HttpResult::Ptr Get(const Uri::Ptr uri
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers = {}
                , const std::string& body = "");
        static HttpResult::Ptr Post(const std::string& url
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers = {}
                , const std::string& body = "");
        static HttpResult::Ptr Post(const Uri::Ptr uri
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers = {}
                , const std::string& body = "");
        static HttpResult::Ptr Request(const HttpMethod method
                , const std::string& url
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers = {}
                , const std::string& body = "");
        static HttpResult::Ptr Request(const HttpMethod method
                , const Uri::Ptr uri
                , uint64_t timeout
                , const std::map<std::string, std::string>& headers = {}
                , const std::string& body = "");
        static HttpResult::Ptr Request(Request::Ptr request
                , Uri::Ptr uri
                , uint64_t timeout);
        
        HttpConnection(Socket::Ptr sock);
        ~HttpConnection();
        Response::Ptr RecvResponse();
        int32_t SendRequest(Request::Ptr req);
    };

}

#endif // ! ____CRAZY_HTTP_HTTP_CONNECTION_H____