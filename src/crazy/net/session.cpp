#include "crazy/net/session.h"

#include "crazy/logger.h"
#include "crazy/utils.h"

/**
 * @brief 全局 session.
 */
static uint64_t GenSession() {
	static uint64_t g_session = 1;
	if (g_session >= UINT64_MAX) {
		g_session = 1;
	}
	return g_session++;
}

namespace crazy {
	Session::Session(Socket::ptr socket)
		: sessionId_(GenSession())
		, socket_(socket)
		, lastHeartbeatTimestamp_(GetCurrentMS())
		, decoder_(recvBuffer_) 
		, encoder_(sendBuffer_) {
		decoder_.registerParseFinishCallback(std::bind(&Session::onParseFinishMessgae, this, std::placeholders::_1));
		decoder_.registerParseExceptionCallback(std::bind(&Session::onParseException, this));
	}
	Session::~Session() {
		close();
	}
	uint64_t Session::getSessionId() const {
		return sessionId_;
	}
	socket_t Session::socket() const {
		return socket_->socket();
	}
	void Session::close() {
		socket_->close();
	}
	void Session::onReadEvent() {
		recvBuffer_.ensureWritableCount(1024);
		auto ret = socket_->recv(recvBuffer_.writeBegin(), recvBuffer_.writableCount());
		if (0 >= ret) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return;
			}
			else {
				CRAZY_SYSTEM_ERROR() << "recv from " << socket_->remoteAddress() << " fail, errno = " << errno;
				socket_->close();
				return sendDisconnect();
			}
		}
		recvBuffer_.written(ret);
		decoder_.parse();
	}
	void Session::onWriteEvent() {
		if (sendBuffer_.readableCount() == 0) {
			unregisterSelectEvent_(socket_->socket(), SelectorEventType::write);
			isSelectWriteEventRegistered_ = false;
			return;
		}
		auto ret = socket_->send(sendBuffer_.readBegin(), sendBuffer_.readableCount());
		if (0 >= ret) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return;
			}
			else {
				CRAZY_SYSTEM_ERROR() << "send from " << socket_->remoteAddress() << " fail, errno = " << errno;
				socket_->close();
				return sendDisconnect();
			}
		}
		sendBuffer_.readed(ret);
		sendBuffer_.shrink();
	}
	const std::string& Session::remoteAddress() {
		return socket_->remoteAddress();
	}
	void Session::registerDisconnectCallback(std::function<void()> callback) {
		disconnectCallback_ = callback;
	}
	void Session::registerMessageCallback(std::function<void(MessageBase::ptr)> callback) {
		onMessageCallback_ = callback;
	}
	void Session::registerSelectEventRegisterCallback(std::function<void(int32_t, SelectorEventType, std::function<void()>)> registerSelectEvent) {
		registerSelectEvent_ = registerSelectEvent;
	}
	void Session::registerSelectEventUnRegisterCallback(std::function<void(int32_t, SelectorEventType)> unregisterSelectEvent) {
		unregisterSelectEvent_ = unregisterSelectEvent;
	}
	void Session::sendMessage(MessageBase::ptr message) {
		encoder_.stringify(message);
		if (sendBuffer_.readableCount() > 0 && !isSelectWriteEventRegistered_) {
			registerSelectEvent_(socket(), SelectorEventType::write, std::bind(&Session::onWriteEvent, this));
			isSelectWriteEventRegistered_ = true;
		}
	}
	uint64_t Session::lastHeartbeatTimestamp() const {
		return lastHeartbeatTimestamp_;
	}
	void Session::sendDisconnect() {
		if (disconnectCallback_) {
			disconnectCallback_();
		}
	}
	void Session::sendRecvMessage(MessageBase::ptr message) {
		if (onMessageCallback_) {
			onMessageCallback_(message);
		}
	}
	void Session::onParseFinishMessgae(MessageBase::ptr message) {
		if (message->getCmd() == InternalCommand::heartbeat) {
			lastHeartbeatTimestamp_ = GetCurrentMS();
		}
		message->setSessionId(sessionId_);
		sendRecvMessage(message);
	}
	void Session::onParseException() {
		CRAZY_SYSTEM_ERROR() << "recv from " << socket_->remoteAddress() << " fail, exception message";
		sendDisconnect();
	}
}
