/**
 * @file application.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 应用管理类
 * @version 0.1
 * @date 15
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <stdint.h>

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "crazy/actor_interface.h"
#include "crazy/clickhouse/clickhouse_connection_pool.h"
#include "crazy/command_line.h"
#include "crazy/file_lock.h"
#include "crazy/thread_pool.h"
#include "crazy/mysql/mysql_connection_pool.h"
#include "crazy/net/local_socket.h"

//! ClickHouse 连接池
#define CLICKHOUSE_CONNECTION_POOL crazy::Application::application()->getClickHouseConnectionPool()
//! MySQL 连接池
#define MYSQL_CONNECTION_POOL crazy::Application::application()->getMySQLConnectionPool()

namespace crazy {
	/**
	 * @brief 应用管理类.
	 */
	class Application : public ActorInterface {
		friend class ActorInterface;
	public:
		/**
		 * @brief 构造函数.
		 */
		explicit Application(int32_t argc, char** argv);
		/**
		 * @brief 析构函数.
		 */
		~Application();
		/**
		 * @brief 获取应用启动类指针.
		 */
		static Application* application();
		/**
		 * @brief 获取ClickHouse连接池.
		 */
		ClickHouseConnectionPool::ptr getClickHouseConnectionPool();
		/**
		 * @brief 获取MySQL连接池.
		 */
		MySQLConnectionPool::ptr getMySQLConnectionPool();
		/**
		 * @brief 启动应用.
		 */
		void exec();
		/**
		 * @brief 停止服务.
		 * @param exitCode 进程退出码.
		 */
		void stopService(int32_t exitCode = 0);
		/**
		 * @brief 重启服务.
		 */
		void restartService();
		/**
		 * @brief 获取帮助手册.
		 */
		std::map<std::string, std::string> helps() override;
		/**
		 * @brief 注册actor.
		 */
		void registerActor(ActorInterface::ptr actor);
		/**
		 * @brief 分发命令行消息.
		 */
		bool dispatchCommandLine(const std::string& input, uint64_t sessionId, const std::string& requestId, std::string* error = nullptr);
		/**
		 * @brief 获取命令行帮助手册.
		 */
		std::string commandLineHelp();
		/**
		 * @brief 获取actor名称列表.
		 */
		std::vector<std::string> actorNames();
		/**
		 * @brief 注册actor.
		 */
		template <typename actor_type>
		void registerActor(const std::string& name);
		/**
		 * @brief 注册actor.
		 */
		template <typename actor_type, typename ...args_type>
		void registerActor(const std::string& name, args_type...args);
		/**
		 * @brief 添加路由规则.
		 */
		void addRouteTable(const std::string& from, const std::string& to, uint64_t cmd);
		/**
		 * @brief 新增异步任务.
		 */
		void enqueueRunnable(std::function<void()> runnable);
		/**
		 * @brief 新增异步任务.
		 */
		void enqueueRunnable(std::function<void()> runnable, int32_t threadIndex);
		/**
		 * @brief 获取一个后台线程.
		 */
		ActorInterface::ptr getActorImplement();

	protected:
		/**
		 * @brief 处理命令行消息.
		 * @param request 请求消息
		 * @param response 应答消息
		 */
		virtual void handleCommandLineMessgaBase(MessageBase::ptr request, MessageBase::ptr response);
		/**
		 * @brief 处理普通消息.
		 * @param message 消息
		 */
		virtual void handleMessgaBase(MessageBase::ptr message);
		/**
		 * @brief 初始化系统.
		 */
		void initSystem();
		/**
		 * @brief exec 进入主循环前启动业务服务.
		 */
		virtual void startServer();
		/**
		 * @brief 流转数据包.
		 */
		void routeMessage(const std::string& from, MessageBase::ptr message);
		/**
		 * @brief 接收命令行客户端.
		 */
		void acceptCommandClient();
		/**
		 * @brief 接收命令行客户端信息.
		 */
		void recvCommandClientMessage();
		/**
		 * @brief 接收命令行客户端信息超时.
		 */
		void recvCommandTimeout();

	protected:
		//! 命令行参数个数
		int32_t argc_ = 0;
		//! 命令行参数
		char** argv_ = nullptr;
		//! 应用管理类单例指针
		static Application* s_application;
		//! actor 列表
		std::unordered_map<std::string, ActorInterface::ptr> actors_;
		//! 路由规则
		std::unordered_map<std::string, std::unordered_map<uint64_t, std::vector<ActorInterface::ptr>>> routeTable_;
		//! 线程池
		ThreadPool::ptr threadPool_ = nullptr;
		//! 本地command套接字服务端
		LocalSocket::ptr commandService_ = nullptr;
		//! 本地command套接字客户端端
		LocalSocket::ptr commandClient_ = nullptr;
		//! 命令行解析器
		cmdline::parser commandLineParser_;
		//! 命令行帮助手册
		std::map<std::string, std::map<std::string, std::string>> helps_;
		//! 应用锁
		FileLock appLock_;
		//! ClickHouse连接池
		ClickHouseConnectionPool::ptr clichouseConnectionPool_ = nullptr;
		//! MySQL连接池
		MySQLConnectionPool::ptr mysqlConnectionPool_ = nullptr;
	};
	template <typename actor_type>
	void Application::registerActor(const std::string& name) {
		registerActor(std::make_shared<actor_type>(name));
	}
	template <typename actor_type, typename ...args_type>
	void Application::registerActor(const std::string& name, args_type...args) {
		registerActor(std::make_shared<actor_type>(name, std::forward<args_type>(args)...));
	}
}
