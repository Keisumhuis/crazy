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
std::string Request::GetHeader(const std::string& key, const std::string& def) {
	auto it = m_headers.find(key);
	return m_headers.end() == it ? def : it->second;
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
std::ostream& Request::Dump(std::ostream& os) const {
    os << HttpMethodToString(m_method) << " "
		<< m_path 
		<< " "
		<< (m_query.empty() ? "" : "?")
		<< m_query
		<< (m_fragment.empty() ? "" : "#")
		<< m_fragment
		<< m_version
		<< "\r\n";
	if (!m_isWebsocket) {
		os << "connection: " << (m_isKeepALive ? "keep-alive" : "close") << "\r\n";
	}
	for (auto& it : m_headers) {
		if ((!m_isWebsocket && strcasecmp(it.first.c_str(), "connection") == 0) || 
			(strcasecmp(it.first.c_str(), "content-length") == 0)) {
			continue;
		}
		os << it.first << ": " << it.second << "\r\n";
	}
	if (!m_body.empty()) {
		os << "content-length: " << m_body.size() << "\r\n\r\n"
			<< m_body;
	} else {
		os << "\r\n";
	} 
	return os;
}
std::string Request::ToString() const {
	std::stringstream ss;
	Dump(ss);
	return ss.str();
}
Response::Ptr Request::GetResponse() {
	Response::Ptr response(new Response);
	response->SetVersion(m_version);
	response->SetKeepALive(m_isKeepALive);
	response->SetWebsocket(m_isWebsocket);
	return response;
}
void on_request_method(void *data, const char *at, size_t length) {
	RequestParser* parser = static_cast<RequestParser*>(data);
	auto method = StringToHttpMethod(std::string{at, length});
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
	RequestParser* parser = static_cast<RequestParser*>(data);
	parser->GetData()->SetBody(std::string{at, length});
}
void on_request_http_field(void *data, const char *field, size_t flen
                           ,const char *value, size_t vlen) {
	RequestParser* parser = static_cast<RequestParser*>(data);
	if (flen == 0) {
		CRAZY_WARN(CRAZY_ROOT_LOGGER()) << "invaalid http request field length == 0";
		return;
	}
	std::string key {field, flen};
	std::string val {value, vlen};
	if (!strcasecmp(key.c_str(), "connection")) {
		parser->GetData()->SetKeepALive(strcasecmp(val.c_str(), "keep-alive") ? false : true);
	}
	parser->GetData()->SetHeader(std::string{field, flen}, std::string{value, vlen});
}
RequestParser::RequestParser() 
	: m_error(0) {
	m_data.reset(new Request);
	http_parser_init(&m_parser);
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
	memmove((char*)data, data + offset, (len - offset));
	return offset;
}
int32_t RequestParser::IsFinished() {
	return http_parser_finish(&m_parser);
}
int32_t RequestParser::HasError() {
	return http_parser_has_error(&m_parser);
}
uint64_t RequestParser::GetContentLength() {
	return m_data->GetHeaderAs<uint64_t>("content-length", 0);
}
uint64_t RequestParser::GetHttpRequestBufferSize() {
	return g_http_request_buffer_size->GetValue();
}
uint64_t RequestParser::GetHttpRequestMaxBodySize() {
	return g_http_request_max_body_size->GetValue();
}

}
