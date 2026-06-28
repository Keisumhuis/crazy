/**
 * @file service_actor.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 服务端actor
 * @version 0.1
 * @date 16
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <memory>

#include "crazy/net/acceptor.h"
#include "crazy/actor_interface.h"
#include "crazy/net/selector.h"
#include "crazy/net/session.h"

namespace crazy {
	/**
	 * @brief 服务端配置.
	 */
	struct ServiceActorConfig {
		//! 绑定地址
		std::string address_;
		//! 监听端口
		uint16_t port_;
		//! 心跳包间隔
		uint32_t heartbeat_interval_;
	};
	/**
	 * @brief 服务端actor.
	 */
	class ServiceActor : public ActorInterface {
	public:
		using ptr = std::shared_ptr<ServiceActor>;
		/**
		 * @brief 构造函数.
		 */
		explicit ServiceActor(const std::string& name);
		/**
		 * @brief 析构函数.
		 */
		virtual ~ServiceActor();

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
		void initService();
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
		 * @brief 接受连接回调.
		 */
		void onAccept();

	protected:
		/**
		 * @brief 断开链接.
		 */
		void onDisconnect(Session::ptr session);
		/**
		 * @brief 收到消息.
		 */
		void onMessage(MessageBase::ptr message);

	private:
		//! 服务端配置
		ServiceActorConfig config_;
		//! 链接接受器
		Acceptor::ptr acceptor_ = nullptr;
		//! 会话管理
		std::map<uint64_t, Session::ptr> sessions_;
	};
}
