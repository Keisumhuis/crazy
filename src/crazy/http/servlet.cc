#include "servlet.h"

namespace crazy::http {
Servlet::Servlet(const std::string& name)
		: m_name (name) {
}

FunctionServlet::FunctionServlet(callback cb) 
	: Servlet ("FunctionServlet") 
	, m_cb (cb) {
} 
void FunctionServlet::Handle(Request::Ptr req, Response::Ptr res) {
	m_cb (req, res);
}
ServletDispatch::ServletDispatch() 
	: Servlet ("ServletDispatch") {
	m_default.reset(new NotFoundServlet("crazy/0.0.1"));	
}
void ServletDispatch::Handle(Request::Ptr req, Response::Ptr res) {
	auto slt = GetMatchedServlet(req->GetPath());
    	if(slt) {
        	slt->Handle(req, res);
    	}
}
void ServletDispatch::AddServlet(const std::string& uri, Servlet::Ptr servlet) {
	Mutex::Guard guard(m_mutex);
	m_servlets[uri] = servlet;
}
void ServletDispatch::AddServlet(const std::string& uri, FunctionServlet::callback cb) {
	Mutex::Guard guard(m_mutex);
	m_servlets[uri] = FunctionServlet::Ptr(new FunctionServlet(cb));
}
void ServletDispatch::AddGlobServlet(const std::string& uri, Servlet::Ptr servlet) {
	Mutex::Guard guard(m_mutex);
	for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
		if (it->first == uri) {
			m_globs.erase(it);
			break;
		}
	}
	m_globs.push_back(std::make_pair(uri, servlet));
}
void ServletDispatch::AddGlobServlet(const std::string& uri, FunctionServlet::callback cb) {
	return AddGlobServlet(uri, FunctionServlet::Ptr(new FunctionServlet(cb)));
}
void ServletDispatch::DelServlet(const std::string& uri) {
	Mutex::Guard guard(m_mutex);
	m_servlets.erase(uri);
}
void ServletDispatch::DelGlobServlet(const std::string& uri) {
	Mutex::Guard guard(m_mutex);
	for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
		if (it->first == uri) {
			m_globs.erase(it);
			break;
		}
	}
}
Servlet::Ptr ServletDispatch::GetServlet(const std::string& uri) {
	Mutex::Guard guard(m_mutex);
	auto it = m_servlets.find(uri);
	return it == m_servlets.end() ? nullptr : it->second;
}
Servlet::Ptr ServletDispatch::GetGlobServlet(const std::string& uri) {
	Mutex::Guard guard(m_mutex);
	for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
		if (it->first == uri) {
			return it->second;
		}
	}
	return nullptr;
}
Servlet::Ptr ServletDispatch::GetMatchedServlet(const std::string& uri) {
	Mutex::Guard guard(m_mutex);
	auto itServlet = m_servlets.find(uri);
	if (m_servlets.end() != itServlet) {
		return itServlet->second;
	}
	for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
		if (fnmatch(it->first.c_str(), uri.c_str(), 0)) {
			return it->second;
		}
	}
	return m_default;
}
void ServletDispatch::SetDefaultServlet(Servlet::Ptr servlet) {
	m_default = servlet;
}
Servlet::Ptr ServletDispatch::GetDefaultServlet() const {
	return m_default;
}
NotFoundServlet::NotFoundServlet(const std::string& name) 
	: Servlet (name) {
	m_content = "<html><head><title>404 not found</title></head>"
		"<body><center><h1>404 Not Found</h1></center>"
		"<hr><center>" + name + "</center><body></html>";
}
void NotFoundServlet::Handle(Request::Ptr req, Response::Ptr res) {
	res->SetStatus(HttpStatus::NOT_FOUND);
	res->SetBody(m_content);
	res->SetHeader("server", "crazy/0.0.1");
	res->SetHeader("content-type", "text/html");
}
}
