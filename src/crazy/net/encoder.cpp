#include "crazy/net/encoder.h"

namespace crazy {
	Encoder::Encoder(Buffer& buffer) 
		: buffer_(buffer) {
	}
	void Encoder::stringify(MessageBase::ptr message) {
		uint64_t size = sizeof(message->getCmd()) + message->getData().size();
		buffer_.append((char*)&size, sizeof(size));
		auto cmd = message->getCmd();
		buffer_.append((char*)&cmd, sizeof(cmd));
		buffer_.append(message->getData().data(), message->getData().size());
	}
	void Encoder::reset() {
	}
}
