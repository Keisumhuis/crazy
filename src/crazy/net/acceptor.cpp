#include "crazy/net/acceptor.h"

#include "crazy/logger.h"

namespace crazy {
	Session::ptr Acceptor::accept() {
		auto clientSocket = Socket::accept();
		if (!clientSocket) {
			return nullptr;
		}
		return std::make_shared<Session>(clientSocket);
	}
}
