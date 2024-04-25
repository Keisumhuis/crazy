#include "fd_context.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace crazy {
FdContext::FdContext(int32_t fd) 
	: m_isInit (false) 
	, m_isSocket (false) 
	, m_sysNonblock (false) 
	, m_userNonblock (false)
	, m_isClosed (false) 
	, m_fd (fd)
	, m_recvTimeout (-1)
	, m_sendTimeout (-1) {
	Init();
}
FdContext::~FdContext() {
}
bool FdContext::IsInit() const {
	return m_isInit;
}
bool FdContext::IsSocket() const {
	return m_isSocket;
}
bool FdContext::IsClose() const {
	return m_isClosed;
}
void FdContext::SetUserNonblock(bool v) {
	m_userNonblock = v;
}
bool FdContext::GetUserNonblock() const {
	return m_userNonblock;
}
void FdContext::SetSysNonblock(bool v) {
	m_sysNonblock = v;
}
bool FdContext::GetSysNonblock() const {
	return m_sysNonblock;
}
void FdContext::SetRecvTimeout(int64_t v) {
	m_recvTimeout = v;
}
int64_t FdContext::GetRecvTimeout() const {
	return m_recvTimeout;
}
void FdContext::SetSendTimeout(int64_t v) {
	m_sendTimeout = v;
}
int64_t FdContext::GetSendTimeout() const {
	return m_sendTimeout;
}
void FdContext::Init() {
	if (m_isInit) {
		return;
	}	
	m_recvTimeout = -1;
	m_sendTimeout = -1;
	
	struct stat fd_stat;
	if (-1 == fstat(m_fd, &fd_stat)) {
		m_isInit = false;
		m_isSocket = false;
	} else {
		m_isInit = true;
		m_isSocket = S_ISSOCK(fd_stat.st_mode);
	}

	if (m_isSocket) {
		auto flags = fcntl_f(m_fd, F_GETFL, 0);
		if (!(flags & O_NONBLOCK)) {
			fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
		}
		m_sysNonblock = true;
	} else {
		m_sysNonblock = false;
	}

	m_userNonblock = false;
	m_isClosed = false;
}
FdManager::FdManager() {
	m_fdcontexts.resize(64);
}
FdContext::Ptr FdManager::Get(int32_t fd, bool auto_create) {
	if (-1 == fd) {
		return nullptr;
	}
	Mutex::Guard guard(m_mutex);
	if (m_fdcontexts.size() <= fd) {
		if (!auto_create) {
			return nullptr;
		}
	} else {
		if (m_fdcontexts[fd] || !auto_create) {
			return m_fdcontexts[fd];
		}
	}
	
	FdContext::Ptr ctx(new FdContext(fd));
	if (m_fdcontexts.size() <= fd) {
		m_fdcontexts.resize(fd * 1.5);
	}
	m_fdcontexts[fd] = ctx;
	return ctx;
}
void FdManager::Del(int32_t fd) {
	Mutex::Guard guard(m_mutex);
	if (m_fdcontexts.size() <= fd) {
		return;
	}
	m_fdcontexts[fd].reset();
}
}
