/**
 * @file uri.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_URI_H____
#define ____CRAZY_URI_H____

#include <stdint.h>

#include <memory>
#include <string>

#include "address.h"
#include "logger.h"

namespace crazy {
	/*
	     foo://user@sylar.com:8042/over/there?name=ferret#nose
	       \_/   \______________/\_________/ \_________/ \__/
	        |           |            |            |        |
	     scheme     authority       path        query   fragment
	*/
	class Uri {
	public:
		using Ptr = std::shared_ptr<Uri>;
		static Uri::Ptr Create(const std::string& uristr);
		const std::string& GetScheme() const;
		const std::string& GetUserInfo() const;
		const std::string& GetHost() const;
		const std::string& GetPath() const;
		const std::string& GetQuery() const;
		const std::string& GetFragment() const;
		const int32_t GetPort() const;
		void SetScheme(const std::string& val);
		void SetUserInfo(const std::string& val);
		void SetHost(const std::string& val);
		void SetPath(const std::string& val);
		void SetQuery(const std::string& val);
		void SetFragment(const std::string& val);
		void SetPort(const int32_t val);
		Address::Ptr CreateAddress() const;
	private:
		std::string m_scheme;
		std::string m_userinfo;
		std::string m_host;
		std::string m_path;
		std::string m_query;
		std::string m_fragment;
		int32_t m_port = 0;
	};

}

#endif // ! ____CRAZY_URI_H____

