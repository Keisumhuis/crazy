/**
 * @file request.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_HTTP_REQUEST_H____
#define ____CRAZY_HTTP_REQUEST_H____

#include <map>
#include <memory>
#include <string>
#include <sstream>

#include "crazy/config.h"
#include "crazy/lexicl_cast.h"
#include "crazy/logger.h"
#include "crazy/util.h"
#include "http11.h"
#include "http11_parser.h"
#include "response.h"

namespace crazy::http {


	class Request final {
	public:
		using Ptr = std::shared_ptr<Request>;
		using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;
		
		HttpMethod GetMethod() const { return m_method; }
		const std::string& GetVersion() const { return m_version; }
		const std::string& GetPath() const { return m_path; }
		const std::string& GetQuery() const { return m_query; }
		const std::string& GetBody() const { return m_body; }
		const MapType& GetHeaders() const { return m_headers; }
		const MapType& GetParams() const { return m_params; }
		const MapType& GetCookies() const { return m_cookies; }
		bool IsKeepALive() const { return m_isKeepALive; }
		bool IsWebsocket() const { return m_isWebsocket; }

		void SetMethod(const HttpMethod& val) { m_method = val; } 
		void SetVersion(const std::string& val) { m_version = val; }
		void SetPath(const std::string& val) { m_path = val; }
		void SetQuery(const std::string& val) { m_query = val; }
		void SetFragment(const std::string& val) { m_fragment = val;}
		void SetBody(const std::string& val) { m_body = val; }
		void SetKeepALive(const bool& val) { m_isKeepALive = val; }
		void SetWebsocket(const bool& val) { m_isWebsocket = val; }
		void SetHeaders(const MapType& val) { m_headers = val; }
		void SetParams(const MapType& val) { m_params = val; }
		void SetCookies(const MapType& val) { m_cookies = val; }
		void SetHeader(const std::string& key, const std::string& val);
		void SetParam(const std::string& key, const std::string& val);
		void SetCookie(const std::string& key, const std::string& val);
		
		void DelHeader(const std::string& val);
		void DelParam(const std::string& val);
		void DelCookie(const std::string& val);
		bool HasHeader(const std::string& key, std::string& val);
		bool HasParam(const std::string& key, std::string& val);
		bool HasCookie(const std::string& key, std::string& val);		
		std::string GetHeader(const std::string& key, const std::string& def = "");

		template <typename type>
		bool CheckGetHeaderAs(const std::string& key, type& val, const type& def = type{}) {
			return CheckGetAs(m_headers, key, val, def);
		}
		template <typename type>
		type GetHeaderAs(const std::string& key, const type& def = type{}) {
			return GetAs(m_headers, key, def);
		}
		template <typename type>
		bool CheckGetParamAs(const std::string& key, type& val, const type& def = type{}) {
		}
		template <typename type>
		type GetParamAs(const std::string& key, const type& def = type{}) {
		}
		template <typename type>
		bool CheckGetCookiesAs(const std::string& key, type& val, const type& def = type{}) {
		}
		template <typename type>
		type GetCookiesAs(const std::string& key, const type& def = type{}) {
		}
		std::ostream& Dump(std::ostream& os) const;
		std::string ToString() const;
		Response::Ptr GetResponse();
	private:
		HttpMethod m_method;
		std::string m_version = "HTTP/1.1";
		bool m_isKeepALive;
		bool m_isWebsocket;
		
		std::string m_path;
		std::string m_query;
		std::string m_fragment;
		std::string m_body;
		MapType m_headers;
		MapType m_params;
		MapType m_cookies;
	};
	
	class RequestParser final {
	public:
		using Ptr = std::shared_ptr<RequestParser>;
		RequestParser();
		size_t Execute(char* data, size_t len);
		int32_t IsFinished();
		int32_t HasError();
		Request::Ptr GetData() const { return m_data; }
		void SetError(int32_t error) { m_error = error; }
		uint64_t GetContentLength();
		const http_parser& GetParser() const { return m_parser; }
		static uint64_t GetHttpRequestBufferSize();
		static uint64_t GetHttpRequestMaxBodySize();	
	private:
		http_parser m_parser;
		Request::Ptr m_data;
		int32_t m_error;
	};
}

#endif // ! ____CRAZY_HTTP_REQUEST_H____
