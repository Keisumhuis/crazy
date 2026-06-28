#include "crazy/message_base.h"

namespace crazy {
	void MessageBase::setSource(const std::string& source) {
		source_ = source;
	}
	const std::string& MessageBase::getSource() const {
		return source_;
	}
	uint64_t MessageBase::getSessionId() const {
		return sessionId_;
	}
	void MessageBase::setSessionId(uint64_t sessionId) {
		sessionId_ = sessionId;
	}
	uint64_t MessageBase::getCmd() const {
		return cmd_;
	}
	void MessageBase::setCmd(uint64_t cmd) {
		cmd_ = cmd;
	}
	void MessageBase::setComment(const std::string& comment) {
		comment_ = comment;
	}
	const std::string& MessageBase::getComment() const {
		return comment_;
	}
	const std::string& MessageBase::getData() const {
		return data_;
	}
	void MessageBase::setData(const std::string& data) {
		data_ = data;
	}
	MessageBase::ptr MessageBase::createResponse() {
		auto response = std::make_shared<MessageBase>();
		response->setCmd(cmd_);
		response->setSessionId(sessionId_);
		return response;
	}
}
