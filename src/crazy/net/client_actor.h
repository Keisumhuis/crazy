/**
 * @file client_actor.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 客户端actor
 * @version 0.1
 * @date 21
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <memory>

#include "crazy/actor_interface.h"
#include "crazy/net/connection.h"

namespace crazy {
	/**
	 * @brief 客户端配置.
	 */
	struct ClientActorConfig {
		//! 绑定地址
		std::string address_;
		//! 监听端口
		uint16_t port_;
		//! 心跳包间隔
		uint32_t heartbeat_interval_;
	};
	/**
	 * @brief 客户端actor.
	 */
	class ClientActor : public ActorInterface {
	public:
		using ptr = std::shared_ptr<ClientActor>;
		/**
		 * @brief 构造函数.
		 */
		explicit ClientActor(const std::string& name);
		/**
		 * @brief 析构函数.
		 */
		virtual ~ClientActor();

	protected:
		/**
		 * @brief 线程执行函数.
		 */
		virtual void run();
		/**
		 * @brief 初始化配置.
		 */
		void initConfig();
		/**
		 * @brief 初始化服务.
		 */
		void initClient();
		/**
		 * @brief 处理消息.
		 * @param message 消息
		 */
		void handleMessgaBase(MessageBase::ptr message) override;
		/**
		 * @brief 保活检测.
		 */
		void onCheckKeepLive();
		/**
		 * @brief 连接上服务器.
		 */
		void onConnected();
		/**
		 * @brief 断开链接.
		 */
		void onDisconnected();
		/**
		 * @brief 接收到完整消息.
		 */
		void onRecvMessage(MessageBase::ptr message);

	private:
		//! 客户端配置
		ClientActorConfig config_;
		//! 连接器
		Connection::ptr connection_;
	};
}
