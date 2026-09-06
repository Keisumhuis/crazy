/**
 * @file http_server.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 服务端.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "crazy/net/http/http_router.h"
#include "crazy/net/http/http_session.h"
#include "crazy/net/socket.h"

namespace crazy {
	/**
	 * @brief HTTP 服务端.
	 */
	class HttpServer {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpServer>;
		/**
		 * @brief 构造函数.
		 */
		HttpServer();
		/**
		 * @brief 析构函数.
		 */
		~HttpServer();
		/**
		 * @brief 开始监听.
		 */
		bool listen(uint16_t port, const std::string& address = "0.0.0.0");
		/**
		 * @brief 获取监听 socket.
		 */
		const Socket::ptr& listener() const;
		/**
		 * @brief 接受一个 HTTP 会话.
		 */
		HttpSession::ptr acceptSession();
		/**
		 * @brief 获取 HTTP 路由.
		 */
		const HttpRouter::ptr& router() const;
		/**
		 * @brief 注册自由函数或 lambda 路由.
		 */
		template <HttpMethod... Methods, typename Function>
		bool registerHttpHandler(const std::string& path, Function&& handler) {
			auto wrapped = std::function<void(HttpRequest&, HttpResponse&)>(std::forward<Function>(handler));
			bool result = true;
			((result = router_->add(Methods, path, wrapped) && result), ...);
			return result;
		}
		/**
		 * @brief 注册成员函数路由并绑定对象引用.
		 */
		template <HttpMethod... Methods, typename Class, typename Member>
		bool registerHttpHandler(const std::string& path, Member Class::* handler, Class& object) {
			bool result = true;
			((result = router_->add(Methods, path, handler, object) && result), ...);
			return result;
		}
		/**
		 * @brief 注册成员函数路由并绑定对象指针.
		 */
		template <HttpMethod... Methods, typename Class, typename Member>
		bool registerHttpHandler(const std::string& path, Member Class::* handler, Class* object) {
			bool result = true;
			((result = router_->add(Methods, path, handler, object) && result), ...);
			return result;
		}
		/**
		 * @brief 注册成员函数路由并绑定 shared_ptr.
		 */
		template <HttpMethod... Methods, typename Class, typename Member>
		bool registerHttpHandler(const std::string& path, Member Class::* handler, const std::shared_ptr<Class>& object) {
			bool result = true;
			((result = router_->add(Methods, path, handler, object) && result), ...);
			return result;
		}

	private:
		//! HTTP 路由
		HttpRouter::ptr router_;
		//! 监听 socket
		Socket::ptr acceptor_;
	};
}
