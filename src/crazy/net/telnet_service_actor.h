/**
 * @file telnet_service_actor.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief Telnet命令服务actor
 * @version 0.1
 * @date 21
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <map>
#include <memory>
#include <string>

#include "crazy/actor_interface.h"
#include "crazy/net/socket.h"

namespace crazy {
	/**
	 * @brief Telnet命令服务配置.
	 */
	struct TelnetServiceActorConfig {
		//! 绑定地址
		std::string address_;
		//! 监听端口
		uint16_t port_;
		//! 最大连接数
		uint32_t max_sessions_;
		//! 空闲超时时间
		uint32_t idle_timeout_;
		//! 命令超时时间
		uint32_t command_timeout_;
		//! 登录密码
		std::string password_;
	};

	/**
	 * @brief Telnet命令服务actor.
	 */
	class TelnetServiceActor : public ActorInterface {
	public:
		using ptr = std::shared_ptr<TelnetServiceActor>;
		/**
		 * @brief 构造函数.
		 */
		explicit TelnetServiceActor(const std::string& name);
		/**
		 * @brief 析构函数.
		 */
		virtual ~TelnetServiceActor();
		/**
		 * @brief 获取帮助手册.
		 */
		std::map<std::string, std::string> helps() override;

	protected:
		/**
		 * @brief 线程执行函数.
		 */
		void run() override;
		/**
		 * @brief 处理消息.
		 */
		void handleMessageBase(MessageBase::ptr message) override;
		/**
		 * @brief 初始化配置.
		 */
		void initConfig();
		/**
		 * @brief 初始化服务.
		 */
		void initService();
		/**
		 * @brief 初始化命令路由.
		 */
		void initCommandRoutes();
		/**
		 * @brief 检查会话状态.
		 */
		void onCheckSessions();
		/**
		 * @brief 接受连接.
		 */
		void onAccept();
		/**
		 * @brief 读取会话数据.
		 */
		void onRead(uint64_t sessionId);
		/**
		 * @brief 写入会话数据.
		 */
		void onWrite(uint64_t sessionId);
		/**
		 * @brief 关闭会话.
		 */
		void closeSession(uint64_t sessionId);
		/**
		 * @brief 发送会话数据.
		 */
		void sendSession(uint64_t sessionId, const std::string& data, bool closeAfterWrite = false);
		/**
		 * @brief 处理输入字符.
		 */
		void handleInputChar(uint64_t sessionId, unsigned char ch);
		/**
		 * @brief 执行输入命令.
		 */
		void executeLine(uint64_t sessionId);
		/**
		 * @brief 发送提示符.
		 */
		void sendPrompt(uint64_t sessionId);
		/**
		 * @brief 获取Telnet帮助.
		 */
		std::string telnetHelp();
		/**
		 * @brief 获取actor列表.
		 */
		std::string actorList();
		/**
		 * @brief 转换换行符.
		 */
		std::string normalizeNewLine(const std::string& data);
		/**
		 * @brief 生成命令行请求id.
		 */
		std::string genCommandLineRequestId(uint64_t sessionId);

	private:
		struct TelnetSession {
			//! 会话id
			uint64_t sessionId_ = 0;
			//! 客户端socket
			Socket::ptr socket_ = nullptr;
			//! 命令行缓冲区
			std::string lineBuffer_;
			//! 发送缓冲区
			std::string sendBuffer_;
			//! 当前命令请求id
			std::string commandRequestId_;
			//! 最后活跃时间
			uint64_t lastActiveTimestamp_ = 0;
			//! 命令开始时间
			uint64_t commandStartTimestamp_ = 0;
			//! 是否等待命令应答
			bool waitingResponse_ = false;
			//! 是否已经通过认证
			bool authorized_ = false;
			//! 是否注册写事件
			bool writeRegistered_ = false;
			//! 写完后关闭
			bool closeAfterWrite_ = false;
			//! Telnet控制命令状态
			int32_t telnetState_ = 0;
			//! 是否忽略下一个换行符
			bool ignoreNextLf_ = false;
		};

	private:
		//! 生成会话id.
		uint64_t genSessionId();

	private:
		//! Telnet命令服务配置
		TelnetServiceActorConfig config_;
		//! 连接接受器
		Socket::ptr acceptor_ = nullptr;
		//! 会话列表
		std::map<uint64_t, std::shared_ptr<TelnetSession>> sessions_;
		//! 会话自增id
		uint64_t sessionId_ = 0;
		//! 命令行请求自增id
		uint64_t commandLineRequestId_ = 0;
	};
}
