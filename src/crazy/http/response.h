/**
 * @file response.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_HTTP_RESPONSE_H____
#define ____CRAZY_HTTP_RESPONSE_H____

#include <map>
#include <memory>
#include <string>

#include "crazy/config.h"
#include "crazy/logger.h"
#include "crazy/lexicl_cast.h"
#include "crazy/util.h"
#include "http11.h"
#include "httpclient_parser.h"

namespace crazy::http {

	class Response final {
	public:
		using Ptr = std::shared_ptr<Response>;
		using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;
		HttpStatus GetStatus() const { return m_status; }
		const std::string& GetVersion() const { return m_version; }
		const std::string& GetBody() const { return m_body; }
		const std::string& GetReason() const { return m_reason; }
		const MapType& GetHeaders() const { return m_headers; }

		void SetStatus(const HttpStatus& val) { m_status = val; }
		void SetVersion(const std::string& val) { m_version = val; }
		void SetBody(const std::string& val) { m_body = val; }
		void SetReason(const std::string& val) { m_reason = val; }
		void SetHeaders(const MapType& val) { m_headers = val; }
		void SetKeepALive(const bool val) { m_isKeepALive = val; }
		void SetWebsocket(const bool val) { m_isWebsocket = val; }

		bool IsKeepALive() const { return m_isKeepALive; }
		bool IsWebsocket() const { return m_isWebsocket; }

		std::string GetHeader(const std::string& key, const std::string& def = "");
		void SetHeader(const std::string& key, const std::string& val);
		void DelHeader(const std::string& key);
		template <typename type>
    		bool CheckGetHeaderAs(const std::string& key, type& val, const type& def = type{}) {
        		return CheckGetAs(m_headers, key, val, def);
    		}
		template <typename type>
    		type GetHeaderAs(const std::string& key, const type& def = type{}) {
        		return GetAs(m_headers, key, def);
    		}
		std::string GetCookie(const std::string& key, const std::string& def = "");
		void SetCookie(const std::string& key, const std::string& val);
		void DelCookie(const std::string& key);
		template <typename type>
    	bool CheckGetCookieAs(const std::string& key, type& val, const type& def = type{}) {
        	return CheckGetAs(m_cookies, key, val, def);
    	}
		template <typename type>
    	type getCookieAs(const std::string& key, const type& def = type{}) {
        	return getAs(m_cookies, key, def);
    	}
		std::ostream& Dump(std::ostream& os) const;
		std::string ToString() const;
	private:
		HttpStatus m_status = HttpStatus::OK;
		std::string m_version = "HTTP/1.1";
		bool m_isKeepALive;
		bool m_isWebsocket;
		std::string m_body;
		std::string m_reason;
		MapType m_headers;
		MapType m_cookies;
	};

	class ResponseParser final {
	public:
		using Ptr = std::shared_ptr<ResponseParser>;
		ResponseParser();
		size_t Execute(char* data, size_t len, bool chunck = false);
		int32_t IsFinished();
		int32_t HasError();
		Response::Ptr GetData() const { return m_data; };
		void SetError(int32_t error) { m_error = error; }
		uint64_t GetContentLength();
		const httpclient_parser& GetParser() const { return m_parser; }
		static uint64_t GetHttpResponseBufferSize();
		static uint64_t GetHttpResponseMaxBodySize();
	private:
		httpclient_parser m_parser;
		Response::Ptr m_data;
		int32_t m_error;
	};

}

#endif // ! ____CRAZY_HTTP_RESPONSE_H____

