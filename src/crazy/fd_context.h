/**
 * @file fd_context.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_FD_CONTEXT_H____
#define ____CRAZY_FD_CONTEXT_H____

#include <stdint.h>

#include <memory>
#include <vector>

#include "hook.h"
#include "mutex.h"
#include "singleton.h"

#define FD_MANAGER() crazy::Singleton<crazy::FdManager>::GetInstance()

namespace crazy {

	class FdContext final
		: public std::enable_shared_from_this<FdContext> {
	public:
		using Ptr = std::shared_ptr<FdContext>;
		FdContext(int32_t fd);
		~FdContext();
		bool IsInit() const;
		bool IsSocket() const;
		bool IsClose() const;
		void SetUserNonblock(bool v);
		bool GetUserNonblock() const;
		void SetSysNonblock(bool v);
		bool GetSysNonblock() const;
		void SetRecvTimeout(int64_t v);
		int64_t GetRecvTimeout() const;
		void SetSendTimeout(int64_t v);
		int64_t GetSendTimeout() const;
	protected:
		void Init();
	private:
		bool m_isInit;
		bool m_isSocket;
		bool m_sysNonblock;
		bool m_userNonblock;
		bool m_isClosed;
		int32_t m_fd;
		int64_t m_recvTimeout;
		int64_t m_sendTimeout;
	};

	class FdManager final {
	public:
		FdManager();
		FdContext::Ptr Get(int32_t fd, bool auto_create = false);
		void Del(int32_t fd);
	private:
		Mutex m_mutex;
		std::vector<FdContext::Ptr> m_fdcontexts;
	}; 
}

#endif // ! ____CRAZY_FD_CONTEXT_H____
