#include "crazy.h"

int main() {
    const char test_request_data[] = "POST / HTTP/1.1\r\n"
                                "Host: www.baidu.com\r\n"
                                "Content-Length: 10\r\n\r\n"
                                "1234567890";
    std::string str = test_request_data;
    crazy::http::RequestParser::Ptr request_parser(new crazy::http::RequestParser);
    request_parser->Execute(str.data(), str.size());
    auto req = request_parser->GetData();
    CRAZY_INFO(CRAZY_ROOT_LOGGER()) << req->ToString();

    const char test_response_data[] = "HTTP/1.1 200 OK\r\n"
        "Date: Tue, 04 Jun 2019 15:43:56 GMT\r\n"
        "Server: Apache\r\n"
        "Last-Modified: Tue, 12 Jan 2010 13:48:00 GMT\r\n"
        "ETag: \"51-47cf7e6ee8400\"\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: 81\r\n"
        "Cache-Control: max-age=86400\r\n"
        "Expires: Wed, 05 Jun 2019 15:43:56 GMT\r\n"
        "Connection: Close\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<html>\r\n"
        "<meta http-equiv=\"refresh\" content=\"0;url=http://www.baidu.com/\">\r\n"
        "</html>\r\n";
    str = test_response_data;
    crazy::http::ResponseParser::Ptr response_parser(new crazy::http::ResponseParser);
    response_parser->Execute(str.data(), str.size());
    auto res = response_parser->GetData();
    CRAZY_INFO(CRAZY_ROOT_LOGGER()) << res->ToString();
}