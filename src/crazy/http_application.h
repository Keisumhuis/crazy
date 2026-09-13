/**
 * @file http_application.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 应用管理类
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "crazy/application.h"
#include "crazy/net/http/http_server.h"

namespace crazy {
	/**
	 * @brief HTTP 应用管理类.
	 */
	class HttpApplication : public Application {
	public:
		using Application::Application;
		/**
		 * @brief 设置 HTTP 监听端口.
		 */
		void listen(uint16_t port, const std::string& address = "0.0.0.0");
		/**
		 * @brief 设置 HTTP 监听地址和端口.
		 */
		void listen(const std::string& address, uint16_t port);
		/**
		 * @brief 注册 WebSocket 连接建立回调.
		 */
		void registerWebSocketConnectCallback(HttpSession::WebSocketConnectCallback callback);
		/**
		 * @brief 注册 WebSocket 消息回调.
		 */
		void registerWebSocketMessageCallback(HttpSession::WebSocketMessageCallback callback);
		/**
		 * @brief 注册 WebSocket 关闭回调.
		 */
		void registerWebSocketCloseCallback(HttpSession::WebSocketCloseCallback callback);
		/**
		 * @brief 注册自由函数或 lambda 路由.
		 */
		template <HttpMethod... Methods, typename Function>
		bool registerHttpHandler(const std::string& path, Function&& handler) {
			return ensureServer()->template registerHttpHandler<Methods...>(path, std::forward<Function>(handler));
		}
		/**
		 * @brief 注册成员函数路由并绑定对象引用.
		 */
		template <HttpMethod... Methods, typename Class, typename Member>
		bool registerHttpHandler(const std::string& path, Member Class::* handler, Class& object) {
			return ensureServer()->template registerHttpHandler<Methods...>(path, handler, object);
		}
		/**
		 * @brief 注册成员函数路由并绑定对象指针.
		 */
		template <HttpMethod... Methods, typename Class, typename Member>
		bool registerHttpHandler(const std::string& path, Member Class::* handler, Class* object) {
			return ensureServer()->template registerHttpHandler<Methods...>(path, handler, object);
		}
		/**
		 * @brief 注册成员函数路由并绑定 shared_ptr.
		 */
		template <HttpMethod... Methods, typename Class, typename Member>
		bool registerHttpHandler(const std::string& path, Member Class::* handler, const std::shared_ptr<Class>& object) {
			return ensureServer()->template registerHttpHandler<Methods...>(path, handler, object);
		}
		/**
		 * @brief 注册静态文件目录.
		 */
		bool registerStaticDirectory(const std::string& path, const std::string& directory, bool enableDirectoryListing = false) {
			return ensureServer()->registerStaticDirectory(path, directory, enableDirectoryListing);
		}

	protected:
		/**
		 * @brief exec 进入主循环前启动 HTTP 服务.
		 */
		void startServer() override;
		/**
		 * @brief 创建或获取 HTTP 服务.
		 */
		HttpServer::ptr ensureServer();
		/**
		 * @brief 接受 HTTP 连接.
		 */
		void onHttpAccept();
		/**
		 * @brief 将会话分配到 Application 线程池中的 actor.
		 */
		void assignHttpSession(HttpSession::ptr session);

	private:
		//! WebSocket 消息回调
		HttpSession::WebSocketMessageCallback wsMessageCallback_;
		//! WebSocket 连接回调
		HttpSession::WebSocketConnectCallback wsConnectCallback_;
		//! WebSocket 关闭回调
		HttpSession::WebSocketCloseCallback wsCloseCallback_;
		//! HTTP 服务
		HttpServer::ptr httpServer_ = nullptr;
		//! 监听端口
		uint16_t port_ = 8080;
		//! 监听地址
		std::string address_ = "0.0.0.0";
		//! 是否主动设置监听
		bool listenSet_ = false;
	};
}  // namespace crazy
