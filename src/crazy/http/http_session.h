/**
 * @file http_session.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_HTTP_HTTP_SESSION_H____
#define ____CRAZY_HTTP_HTTP_SESSION_H____

#include <memory>

#include "crazy/streams/socket_stream.h"
#include "crazy/http/request.h"
#include "crazy/http/response.h"

namespace crazy::http {
    class HttpSession : public stream::SocketStream {
    public:
        using Ptr = std::shared_ptr<HttpSession>;
        HttpSession(Socket::Ptr sock);
        Request::Ptr RecvRequest();
        int32_t SendResponse(Response::Ptr res);
    };
}

#endif // ! ____CRAZY_HTTP_HTTP_SESSION_H____