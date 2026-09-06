/**
 * @file http_router.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 路由注册和分发.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "crazy/net/http/http11_common.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"

namespace crazy {
	/**
	 * @brief HTTP 路由.
	 */
	class HttpRouter {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpRouter>;
		//! 路由处理器类型
		using handler_type = std::function<void(HttpRequest&, HttpResponse&)>;
		/**
		 * @brief 构造函数.
		 */
		HttpRouter() = default;
		/**
		 * @brief 注册自由函数或绑定函数.
		 */
		template <typename Function>
		bool add(HttpMethod method, const std::string& path, Function&& handler) {
			return addHandler(method, path, handler_type(std::forward<Function>(handler)));
		}
		/**
		 * @brief 注册成员函数并绑定对象引用.
		 */
		template <typename Class, typename Member>
		bool add(HttpMethod method, const std::string& path,
			Member Class::* handler, Class& object) {
			return addHandler(method, path, [object = &object, handler](HttpRequest& request, HttpResponse& response) {
				std::invoke(handler, *object, request, response);
				});
		}
		/**
		 * @brief 注册成员函数并绑定对象指针.
		 */
		template <typename Class, typename Member>
		bool add(HttpMethod method, const std::string& path,
			Member Class::* handler, Class* object) {
			return addHandler(method, path, [object, handler](HttpRequest& request, HttpResponse& response) {
				if (object != nullptr) {
					std::invoke(handler, *object, request, response);
				}
				});
		}
		/**
		 * @brief 注册成员函数并绑定 shared_ptr 对象.
		 */
		template <typename Class, typename Member>
		bool add(HttpMethod method, const std::string& path,
			Member Class::* handler, const std::shared_ptr<Class>& object) {
			return addHandler(method, path, [object, handler](HttpRequest& request, HttpResponse& response) {
				if (object) {
					std::invoke(handler, *object, request, response);
				}
				});
		}
		/**
		 * @brief 按模板方法注册自由函数或绑定函数.
		 */
		template <HttpMethod method, typename Function>
		bool setHttpHandler(const std::string& path, Function&& handler) {
			return add(method, path, std::forward<Function>(handler));
		}
		/**
		 * @brief 按模板方法注册成员函数并绑定对象引用.
		 */
		template <HttpMethod method, typename Class, typename Member>
		bool setHttpHandler(const std::string& path, Member Class::* handler, Class& object) {
			return add(method, path, handler, object);
		}
		/**
		 * @brief 按模板方法注册成员函数并绑定对象指针.
		 */
		template <HttpMethod method, typename Class, typename Member>
		bool setHttpHandler(const std::string& path, Member Class::* handler, Class* object) {
			return add(method, path, handler, object);
		}
		/**
		 * @brief 按模板方法注册成员函数并绑定 shared_ptr 对象.
		 */
		template <HttpMethod method, typename Class, typename Member>
		bool setHttpHandler(const std::string& path, Member Class::* handler, const std::shared_ptr<Class>& object) {
			return add(method, path, handler, object);
		}
		/**
		 * @brief 分发 HTTP 请求.
		 * @return 是否找到处理器.
		 */
		bool handle(HttpRequest& request, HttpResponse& response) const;
		/**
		 * @brief 分发 HTTP 请求.
		 * @return 是否找到处理器.
		 */
		bool route(HttpRequest& request, HttpResponse& response) const {
			return handle(request, response);
		}
		/**
		 * @brief 判断是否已注册某个路由.
		 */
		bool contains(HttpMethod method, const std::string& path) const;
		/**
		 * @brief 清空所有路由.
		 */
		void clear() {
			handlers_.clear();
		}
		/**
		 * @brief 获取已注册的路由数量.
		 */
		std::size_t size() const {
			return handlers_.size();
		}

	private:
		/**
		 * @brief 内部注册处理器.
		 */
		bool addHandler(HttpMethod method, const std::string& path, handler_type handler);
		/**
		 * @brief 生成 METHOD path 路由键.
		 */
		static std::string makeRouteKey(HttpMethod method, const std::string& path);
		//! 路由处理器表
		std::unordered_map<std::string, handler_type> handlers_;
	};
	inline bool HttpRouter::handle(HttpRequest& request, HttpResponse& response) const {
		const std::string key = makeRouteKey(request.method(), request.uri().getPath());
		const auto it = handlers_.find(key);
		if (it == handlers_.end()) {
			return false;
		}
		it->second(request, response);
		return true;
	}
	inline bool HttpRouter::contains(HttpMethod method, const std::string& path) const {
		return handlers_.find(makeRouteKey(method, path)) != handlers_.end();
	}
	inline bool HttpRouter::addHandler(HttpMethod method,
		const std::string& path, handler_type handler) {
		const auto [it, inserted] = handlers_.emplace(
			makeRouteKey(method, path), std::move(handler));
		return inserted;
	}
	inline std::string HttpRouter::makeRouteKey(HttpMethod method, const std::string& path) {
		std::string key = httpMethodToString(method);
		key += ' ';
		if (path.empty()) {
			key += '/';
		}
		else if (path[0] == '/') {
			key += path;
		}
		else {
			key += '/';
			key += path;
		}
		return key;
	}
}
