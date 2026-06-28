#include "crazy/net/decoder.h"

#include <string.h>

namespace crazy {
	Decoder::Decoder(Buffer& buffer)
		: buffer_(buffer) {
	}
	void Decoder::setMaxMessgaeLength(uint64_t length) {
		maxMessageLength_ = length;
	}
	uint64_t Decoder::getMaxMessgaeLength() const {
		return maxMessageLength_;
	}
	void Decoder::parse() {
		do {
			if ((DecodeStatus::begin == status_) && buffer_.readableCount() >= sizeof(uint64_t)) {
				memcpy(&size_, buffer_.readBegin(), sizeof(uint64_t));
				buffer_.readed(sizeof(uint64_t));
				if (size_ > maxMessageLength_) {
					sendException();
					break;
				}
				cacheMessage_ = std::make_shared<MessageBase>();
				status_ = DecodeStatus::command;
			}
			else if (DecodeStatus::command == status_ && buffer_.readableCount() >= sizeof(uint64_t)) {
				uint64_t command = 0;
				memcpy(&command, buffer_.readBegin(), sizeof(uint64_t));
				buffer_.readed(sizeof(uint64_t));
				cacheMessage_->setCmd(command);
				size_ -= sizeof(uint64_t);
				if (size_ != 0) {
					status_ = DecodeStatus::body;
				}
				else {
					sendFinishMessageBase(cacheMessage_);
					status_ = DecodeStatus::begin;
				}
			}
			else if (DecodeStatus::body == status_ && buffer_.readableCount() >= size_) {
				std::string body;
				body.resize(size_);
				memcpy(body.data(), buffer_.readBegin(), size_);
				buffer_.readed(size_);
				cacheMessage_->setData(body);
				buffer_.shrink();
				sendFinishMessageBase(cacheMessage_);
				status_ = DecodeStatus::begin;
			}
			else {
				break;
			}
		} while (buffer_.readableCount() != 0);
	}
	void Decoder::registerParseFinishCallback(std::function<void(MessageBase::ptr)> callback) {
		sendMessage_ = callback;
	}
	void Decoder::registerParseExceptionCallback(std::function<void()> callback) {
		sendException_ = callback;
	}
	void Decoder::sendFinishMessageBase(MessageBase::ptr message) {
		if (sendMessage_) {
			sendMessage_(message);
		}
	}
	void Decoder::sendException() {
		if (sendException_) {
			sendException_();
		}
	}
	void Decoder::reset() {
		status_ = DecodeStatus::begin;
		size_ = 0;
	}
}
