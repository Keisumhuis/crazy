#include "request.h"

namespace crazy::http {

static ConfigValue<uint64_t>::Ptr g_http_request_buffer_size =
    	Config::Lookup("http.request.buffer_size"
                ,(uint64_t)(4 * 1024), "http request buffer size");

static ConfigValue<uint64_t>::Ptr g_http_request_max_body_size =
    	Config::Lookup("http.request.max_body_size"
                ,(uint64_t)(64 * 1024 * 1024), "http request max body size");

void Request::DelHeader(const std::string& val) {
	m_headers.erase(val);
}
void Request::DelParam(const std::string& val) {
	m_params.erase(val);
}
void Request::DelCookie(const std::string& val) {
	m_cookies.erase(val);
}
bool Request::HasHeader(const std::string& key, std::string& val) {
	auto it = m_headers.find(key);
	if (m_headers.end() != it) {
		val = it->second;
		return true;
	}
	return false;
}
bool Request::HasParam(const std::string& key, std::string& val) {
	auto it = m_params.find(key);
	if (m_headers.end() != it) {
		val = it->second;
		return true;
	}
	return false;
}
bool Request::HasCookie(const std::string& key, std::string& val) {
	auto it = m_cookies.find(key);
	if (m_headers.end() != it) {
		val = it->second;
		return true;
	}
	return false;
}
void Request::SetHeader(const std::string& key, const std::string& val) {
	m_headers[key] = val;
}
void Request::SetParam(const std::string& key, const std::string& val) {
	m_params[key] = val;
}
void Request::SetCookie(const std::string& key, const std::string& val) {
	m_cookies[key] = val;
}
std::ostream& Request::dump(std::ostream& os) const {
    return os;
}
void on_request_method(void *data, const char *at, size_t length) {
	RequestParser* parser = static_cast<RequestParser*>(data);
	auto method = StringToHttpMethod(at);
	parser->GetData()->SetMethod(method);
}
void on_request_uri(void *data, const char *at, size_t length) {
}
void on_request_fragment(void *data, const char *at, size_t length) {
	RequestParser* parser = static_cast<RequestParser*>(data);
	parser->GetData()->SetFragment(std::string{at, length});	
}
void on_request_path(void *data, const char *at, size_t length) {
	RequestParser* parser = static_cast<RequestParser*>(data);
	parser->GetData()->SetPath(std::string{at, length});
}
void on_request_query(void *data, const char *at, size_t length) {
	RequestParser* parser = static_cast<RequestParser*>(data);
	parser->GetData()->SetQuery(std::string{at, length});
}
void on_request_version(void *data, const char *at, size_t length) {
	RequestParser* parser = static_cast<RequestParser*>(data);
	parser->GetData()->SetVersion(std::string{at, length});
}
void on_request_header_done(void *data, const char *at, size_t length) {
}
void on_request_http_field(void *data, const char *field, size_t flen
                           ,const char *value, size_t vlen) {
	RequestParser* parser = static_cast<RequestParser*>(data);
	if (flen == 0) {
		CRAZY_WARN(CRAZY_ROOT_LOGGER()) << "invaalid http request field length == 0";
		return;
	}
	parser->GetData()->SetHeader(std::string{field, flen}, std::string{value, vlen});
}
RequestParser::RequestParser() 
	: m_error(0) {
	m_data.reset(new Request);
	m_parser.request_method = on_request_method;
    	m_parser.request_uri = on_request_uri;
    	m_parser.fragment = on_request_fragment;
    	m_parser.request_path = on_request_path;
    	m_parser.query_string = on_request_query;
    	m_parser.http_version = on_request_version;
    	m_parser.header_done = on_request_header_done;
    	m_parser.http_field = on_request_http_field;
    	m_parser.data = this;
}
size_t RequestParser::Execute(char* data, size_t len) {
	auto offset = http_parser_execute(&m_parser, data, len, 0);
	memmove(data, data + offset, (len - offset));
	return offset;
}
int32_t RequestParser::IsFinished() {
	return http_parser_finish(&m_parser);
}
int32_t RequestParser::HasError() {
	return http_parser_has_error(&m_parser);
}
uint64_t RequestParser::GetContentLength() {
	m_data->GetHeaderAs<uint64_t>("content-length", 0);
}
uint64_t RequestParser::GetHttpRequestBufferSize() {
	return g_http_request_buffer_size->GetValue();
}
uint64_t RequestParser::GetHttpRequestMaxBodySize() {
	return g_http_request_max_body_size->GetValue();
}

}
