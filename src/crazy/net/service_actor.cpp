#include "crazy/net/service_actor.h"

#include "crazy/config.h"
#include "crazy/common.h"
#include "crazy/logger.h"

namespace crazy {
	ServiceActor::ServiceActor(const std::string& name)
		: ActorInterface(name), acceptor_(std::make_shared<Acceptor>()) {
	}
	ServiceActor::~ServiceActor() {
		sessions_.clear();
	}
	void ServiceActor::run() {
		initConfig();
		initService();
		ActorInterface::run();
	}
	void ServiceActor::initConfig() {
		config_.address_ = Config::GetString(name_, "address", "0.0.0.0");
		config_.port_ = Config::GetIntager(name_, "port", 8901);
		config_.heartbeat_interval_ = Config::GetIntager(name_, "heartbeat_interval", 5000);
	}
	void ServiceActor::initService() {
		if (!acceptor_->listen(config_.port_, config_.address_)) {
			CRAZY_SYSTEM_INFO() << "service listening " << config_.address_ << ":" << config_.port_ << " fail";
			return;
		}
		CRAZY_SYSTEM_INFO() << "service listening " << config_.address_ << ":" << config_.port_;
		registerEvent(acceptor_->socket(), SelectorEventType::read, std::bind(&ServiceActor::onAccept, this));
		registerTimer("heartbeat", config_.heartbeat_interval_, std::bind(&ServiceActor::onCheckKeepLive, this));
	}
	void ServiceActor::handleMessgaBase(MessageBase::ptr message) {
		if (message->getSessionId() == 0) {
			for (auto& itSession : sessions_) {
				itSession.second->sendMessage(message);
			}
		}
		else {
			auto itSession = sessions_.find(message->getSessionId());
			if (sessions_.end() != itSession) {
				itSession->second->sendMessage(message);
			}
		}
	}
	void ServiceActor::onCheckKeepLive() {
		auto currentTimestamp = GetCurrentMS();
		for (auto it = sessions_.begin(); it != sessions_.end(); ) {
			if (abs((int64_t)currentTimestamp - (int64_t)it->second->lastHeartbeatTimestamp()) > 3 * config_.heartbeat_interval_) {
				cancelEvent(it->second->socket());
				it->second->close();
				it = sessions_.erase(it);
				continue;
			}
			++it;
		}
	}
	void ServiceActor::onAccept() {
		auto session = acceptor_->accept();
		if (!session) {
			return;
		}
		CRAZY_SYSTEM_INFO() << "new client connected, remote address = " << session->remoteAddress();
		registerEvent(session->socket(), SelectorEventType::read, std::bind(&Session::onReadEvent, session));
		session->registerDisconnectCallback(std::bind(&ServiceActor::onDisconnect, this, session));
		session->registerMessageCallback(std::bind(&ServiceActor::onMessage, this, std::placeholders::_1));
		session->registerSelectEventRegisterCallback([this](int32_t fd, SelectorEventType type, std::function<void()> callback) {
			this->registerEvent(fd, type, callback);
			});
		session->registerSelectEventUnRegisterCallback([this](int32_t fd, SelectorEventType type) {
			this->unregisterEvent(fd, type);
			});
		
		sessions_[session->getSessionId()] = session;
		auto connectedMessage = std::make_shared<crazy::MessageBase>();
		connectedMessage->setSessionId(session->getSessionId());
		connectedMessage->setCmd(InternalCommand::command_client_connected);
		sendMessage(connectedMessage);
	}
	void ServiceActor::onDisconnect(Session::ptr session) {
		CRAZY_SYSTEM_INFO() << "client disconnected, remote address = " << session->remoteAddress();
		cancelEvent(session->socket());
		session->close();

		auto connectedMessage = std::make_shared<crazy::MessageBase>();
		connectedMessage->setSessionId(session->getSessionId());
		connectedMessage->setCmd(InternalCommand::command_client_disconnected);
		sendMessage(connectedMessage);
		sessions_.erase(session->getSessionId());
	}
	void ServiceActor::onMessage(MessageBase::ptr message) {
		sendMessage(message);
	}
}
