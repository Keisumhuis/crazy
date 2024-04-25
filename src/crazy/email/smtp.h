/**
 * @file smtp.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_EMAIL_SMTP_H____
#define ____CRAZY_EMAIL_SMTP_H____

#include <memory>
#include <sstream>

#include "crazy/streams/socket_stream.h"
#include "crazy/socket.h"

namespace crazy::email {

    enum class ResultError {
        OK = 0,
        IO_ERROR = -1
    };

    struct SmtpResult {
        using Ptr = std::shared_ptr<SmtpResult>;
        SmtpResult(ResultError r, const std::string& m)
            : result(r), msg(m) {}
        ResultError result;
        std::string msg;
    };

    class SmtpClient : public stream::SocketStream {
    public:
        using Ptr = std::shared_ptr<SmtpClient>;
        static SmtpClient::Ptr Create(const std::string& host, uint32_t port, bool ssl = false);
    private:
        SmtpClient(Socket::Ptr sock);
    private:
        std::string m_host;
        std::stringstream m_ss;
        bool m_authed = false;
    };

}

#endif // ! ____CRAZY_EMAIL_SMTP_H____