#include "response.h"

namespace crazy::http {

static ConfigValue<uint64_t>::Ptr g_http_response_buffer_size =
    	Config::Lookup("http.response.buffer_size"
                ,(uint64_t)(4 * 1024), "http response buffer size");

static ConfigValue<uint64_t>::Ptr g_http_response_max_body_size =
    	Config::Lookup("http.response.max_body_size"
                ,(uint64_t)(64 * 1024 * 1024), "http response max body size");

void Response::SetHeader(const std::string& key, const std::string& val) {
	m_headers[key] = val;
}
void Response::DelHeader(const std::string& key) {
	m_headers.erase(key);
}
void Response::SetCookie(const std::string& key, const std::string& val) {
	m_cookies[key] = val;
}
void Response::DelCookie(const std::string& key) {
	m_cookies.erase(key);
}
std::ostream& Response::dump(std::ostream& os) const {
    return os;
}
void on_response_reason(void *data, const char *at, size_t length) {
    ResponseParser* parser = static_cast<ResponseParser*>(data);
    parser->GetData()->SetReason(std::string{at, length});
}

void on_response_status(void *data, const char *at, size_t length) {
    ResponseParser* parser = static_cast<ResponseParser*>(data);
    HttpStatus status = (HttpStatus)(atoi(at));
    parser->GetData()->SetStatus(status);
}

void on_response_chunk(void *data, const char *at, size_t length) {
}

void on_response_version(void *data, const char *at, size_t length) {
    ResponseParser* parser = static_cast<ResponseParser*>(data);
    parser->GetData()->SetVersion(std::string{at, length});
}

void on_response_header_done(void *data, const char *at, size_t length) {
}

void on_response_last_chunk(void *data, const char *at, size_t length) {
}

void on_response_http_field(void *data, const char *field, size_t flen
                           ,const char *value, size_t vlen) {
    ResponseParser* parser = static_cast<ResponseParser*>(data);
    if(flen == 0) {
        CRAZY_WARN(CRAZY_ROOT_LOGGER()) << "invalid http response field length == 0";
        return;
    }
    parser->GetData()->SetHeader(std::string{field, flen}
                                ,std::string{value, vlen});
}
ResponseParser::ResponseParser() 
	:m_error(0) {
    	m_data.reset(new Response);
    	httpclient_parser_init(&m_parser);
    	m_parser.reason_phrase = on_response_reason;
    	m_parser.status_code = on_response_status;
    	m_parser.chunk_size = on_response_chunk;
    	m_parser.http_version = on_response_version;
    	m_parser.header_done = on_response_header_done;
    	m_parser.last_chunk = on_response_last_chunk;
    	m_parser.http_field = on_response_http_field;
    	m_parser.data = this;
}
size_t ResponseParser::Execute(char* data, size_t len, bool chunck) {
	if (chunck) {
		httpclient_parser_init(&m_parser);
	}
	size_t offset = httpclient_parser_execute(&m_parser, data, len, 0);
	memmove(data, data + offset, (len - offset));
	return offset;
}
int32_t ResponseParser::IsFinished() {
	return httpclient_parser_finish(&m_parser);
}
int32_t ResponseParser::HasError() {
	httpclient_parser_has_error(&m_parser);
}
uint64_t ResponseParser::GetContentLength() {
	return m_data->GetHeaderAs<uint64_t>("content-length", 0);
}
uint64_t ResponseParser::GetHttpResponseBufferSize() {
	return g_http_response_buffer_size->GetValue();
}
uint64_t ResponseParser::GetHttpResponseMaxBodySize() {
	return g_http_response_max_body_size->GetValue();
}
}
