/**
 * @file servlet.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_HTTP_SERVLET_H____
#define ____CRAZY_HTTP_SERVLET_H____

#include "fnmatch.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "crazy/http/http11.h"
#include "crazy/http/request.h"
#include "crazy/http/response.h"
#include "crazy/mutex.h"

namespace crazy::http {

	class Servlet {
	public:
		using Ptr = std::shared_ptr<Servlet>;
		Servlet(const std::string& name);
		virtual ~Servlet() {}
		virtual void Handle(Request::Ptr req, Response::Ptr res) = 0;
		const std::string& GetName() const { return m_name; }
	private:
		std::string m_name;
	};

	class FunctionServlet : public Servlet {
	public:
		using Ptr = std::shared_ptr<FunctionServlet>;
		using callback = std::function<void (Request::Ptr, Response::Ptr)>;
		FunctionServlet(callback cb); 
		virtual void Handle(Request::Ptr req, Response::Ptr res) override;
	private:
		callback m_cb;
	};
	
	class ServletDispatch : public Servlet {
	public:
		using Ptr = std::shared_ptr<ServletDispatch>;
		ServletDispatch();
		virtual void Handle(Request::Ptr req, Response::Ptr res) override;
		void AddServlet(const std::string& uri, Servlet::Ptr servlet);
		void AddServlet(const std::string& uri, FunctionServlet::callback cb);
		void AddGlobServlet(const std::string& uri, Servlet::Ptr servlet);
		void AddGlobServlet(const std::string& uri, FunctionServlet::callback cb);
		void DelServlet(const std::string& uri);
		void DelGlobServlet(const std::string& uri);

		Servlet::Ptr GetServlet(const std::string& uri);
		Servlet::Ptr GetGlobServlet(const std::string& uri);
		Servlet::Ptr GetMatchedServlet(const std::string& uri);

		void SetDefaultServlet(Servlet::Ptr servlet);
		Servlet::Ptr GetDefaultServlet() const;
	private:
		Mutex m_mutex;
		std::unordered_map<std::string, Servlet::Ptr> m_servlets;
		std::vector<std::pair<std::string, Servlet::Ptr>> m_globs;
		Servlet::Ptr m_default;	
	};

	class NotFoundServlet : public Servlet {
	public:
		using Ptr = std::shared_ptr<NotFoundServlet>;
		NotFoundServlet(const std::string& name);
		virtual void Handle(Request::Ptr req, Response::Ptr res) override;
	private:
		std::string m_content;
	};
}

#endif // ! ____CRAZY_HTTP_SERVLET_H____
