/**
 * @file acceptor.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief socket链接接受器
 * @version 0.1
 * @date 16
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <memory>

#include "crazy/net/session.h"
#include "crazy/net/socket.h"

namespace crazy {
	/**
	 * @brief socket链接接受器.
	 */
	class Acceptor final : public Socket {
	public:
		using ptr = std::shared_ptr<Acceptor>;
		/**
		 * @brief 接受一个连接.
		 */
		Session::ptr accept();
	};
}
