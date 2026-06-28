#include "crazy/net/connection.h"

#include "crazy/logger.h"

namespace crazy {
	Connection::Connection() 
		: decoder_(recvBuffer_)
		, encoder_(sendBuffer_) {
		decoder_.registerParseFinishCallback(std::bind(&Connection::onParseFinishMessgae, this, std::placeholders::_1));
		decoder_.registerParseExceptionCallback(std::bind(&Connection::onParseException, this));
	}
	void Connection::connect(const std::string& ip, uint16_t port) {
		if (Socket::connect(ip, port)) {
			sendConnected();
			recvBuffer_.reset();
			sendBuffer_.reset();
			decoder_.reset();
			encoder_.reset();
		}
	}
	void Connection::registerConnectedCallback(std::function<void()> callback) {
		onConnected_ = callback;
	}
	void Connection::registerDisconnectedCallback(std::function<void()> callback) {
		onDisconnected_ = callback;
	}
	void Connection::registerRecvMessageCallback(std::function<void(MessageBase::ptr)> callback) {
		onRecvMessgae_ = callback;
	}
	void Connection::registerSelectEventRegisterCallback(std::function<void(int32_t, SelectorEventType, std::function<void()>)> registerSelectEvent) {
		registerSelectEvent_ = registerSelectEvent;
	}
	void Connection::registerSelectEventUnRegisterCallback(std::function<void(int32_t, SelectorEventType)> unregisterSelectEvent) {
		unregisterSelectEvent_ = unregisterSelectEvent;
	}
	void Connection::onReadEvent() {
		recvBuffer_.ensureWritableCount(1024);
		auto ret = Socket::recv(recvBuffer_.writeBegin(), recvBuffer_.writableCount());
		if (0 >= ret) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return;
			}
			else {
				CRAZY_SYSTEM_ERROR() << "recv from " << Socket::remoteAddress() << " fail, errno = " << errno;
				Socket::close();
				return sendDisconnected();
			}
		}
		recvBuffer_.written(ret);
		decoder_.parse();
	}
	void Connection::onWriteEvent() {
		if (sendBuffer_.readableCount() == 0) {
			unregisterSelectEvent_(socket(), SelectorEventType::write);
			return;
		}
		auto ret = Socket::send(sendBuffer_.readBegin(), sendBuffer_.readableCount());
		if (0 >= ret) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return;
			}
			else {
				CRAZY_SYSTEM_ERROR() << "send from " << Socket::remoteAddress() << " fail, errno = " << errno;
				Socket::close();
				return sendDisconnected();
			}
		}
		sendBuffer_.readed(ret);
		sendBuffer_.shrink();
		if (sendBuffer_.readableCount() == 0) {
			unregisterSelectEvent_(socket(), SelectorEventType::write);
			return;
		}
	}
	void Connection::sendMessage(MessageBase::ptr message) {
		if (!isConnected()) {
			return;
		}
		encoder_.stringify(message);
		if (sendBuffer_.readableCount() > 0) {
			registerSelectEvent_(socket(), SelectorEventType::write, std::bind(&Connection::onWriteEvent, this));
		}
	}
	bool Connection::isConnected() const {
		return isConnected_;
	}
	void Connection::sendHeartbeat() {
	}
	void Connection::sendConnected() {
		isConnected_ = true;
		if (onConnected_) {
			onConnected_();
		}
	}
	void Connection::sendDisconnected() {
		isConnected_ = false;
		Socket::close();
		if (onDisconnected_) {
			onDisconnected_();
		}
	}
	void Connection::sendRecvMessage(MessageBase::ptr message) {
		if (onRecvMessgae_) {
			onRecvMessgae_(message);
		}
	}
	void Connection::onParseFinishMessgae(MessageBase::ptr message) {
		sendRecvMessage(message);
	}
	void Connection::onParseException() {
		sendDisconnected();
	}
}
