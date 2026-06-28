#include "crazy/net/client_actor.h"

#include "crazy/common.h"
#include "crazy/config.h"
#include "crazy/logger.h"

namespace crazy {
	ClientActor::ClientActor(const std::string& name) 
		: ActorInterface(name), connection_(std::make_shared<Connection>()) {
	}
	ClientActor::~ClientActor() {
	}
	void ClientActor::run() {
		initConfig();
		initClient();
		ActorInterface::run();
	}
	void ClientActor::initConfig() {
		config_.address_ = Config::GetString(name_, "address", "127.0.0.1");
		config_.port_ = Config::GetIntager(name_, "port", 8901);
		config_.heartbeat_interval_ = Config::GetIntager(name_, "heartbeat_interval", 5000);
	}
	void ClientActor::initClient() {
		connection_->registerConnectedCallback(std::bind(&ClientActor::onConnected, this));
		connection_->registerDisconnectedCallback(std::bind(&ClientActor::onDisconnected, this));
		connection_->registerRecvMessageCallback(std::bind(&ClientActor::onRecvMessage, this, std::placeholders::_1));
		connection_->registerSelectEventRegisterCallback([this](int32_t fd, SelectorEventType type, std::function<void()> callback) {
			this->registerEvent(fd, type, callback);
			});
		connection_->registerSelectEventUnRegisterCallback([this](int32_t fd, SelectorEventType type) {
			this->unregisterEvent(fd, type);
			});

		registerTimer("heartbeat", config_.heartbeat_interval_, std::bind(&ClientActor::onCheckKeepLive, this));

		connection_->connect(config_.address_, config_.port_);
	}
	void ClientActor::handleMessgaBase(MessageBase::ptr message) {
		connection_->sendMessage(message);
	}
	void ClientActor::onCheckKeepLive() {
		if (!connection_->isConnected()) {
			connection_->connect(config_.address_, config_.port_);
		}
		else {
			auto heartbeat = std::make_shared<MessageBase>();
			heartbeat->setCmd(InternalCommand::heartbeat);
			connection_->sendMessage(heartbeat);
		}
	}
	void ClientActor::onConnected() {
		CRAZY_SYSTEM_INFO() << "connected to " << config_.address_ << ":" << config_.port_ << " success";
		registerEvent(connection_->socket(), SelectorEventType::read, std::bind(&Connection::onReadEvent, connection_));
		auto message = std::make_shared<MessageBase>();
		message->setCmd(InternalCommand::command_connected_service);
		sendMessage(message);
	}
	void ClientActor::onDisconnected() {
		CRAZY_SYSTEM_INFO() << "disconnected to " << config_.address_ << ":" << config_.port_ << ", try connecting";
		auto message = std::make_shared<MessageBase>();
		message->setCmd(InternalCommand::command_disconnected_service);
		sendMessage(message);
		cancelEvent(connection_->socket());
		connection_->connect(config_.address_, config_.port_);
	}
	void ClientActor::onRecvMessage(MessageBase::ptr message) {
		sendMessage(message);
	}
}
