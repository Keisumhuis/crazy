#include "hook.h"
#include "crazy/coroutine.h"
#include "crazy/lexicl_cast.h"
#include "crazy/selector.h"
#include "fd_context.h"
#include <functional>
#include <iterator>
#include <utility>

namespace crazy {
	static thread_local bool t_hook_enable = false;
	
	void hook_init() {

		sleep_f = (sleep_fun)dlsym(RTLD_NEXT, "sleep");
		usleep_f = (usleep_fun)dlsym(RTLD_NEXT, "usleep");
		nanosleep_f = (nanosleep_fun)dlsym(RTLD_NEXT, "nanosleep");
		socket_f = (socket_fun)dlsym(RTLD_NEXT, "socket");
		connect_f = (connect_fun)dlsym(RTLD_NEXT, "connect");
		accept_f = (accept_fun)dlsym(RTLD_NEXT, "accept");
		read_f = (read_fun)dlsym(RTLD_NEXT, "read");
		readv_f = (readv_fun)dlsym(RTLD_NEXT, "readv");
		recv_f = (recv_fun)dlsym(RTLD_NEXT, "recv");
		recvfrom_f = (recvfrom_fun)dlsym(RTLD_NEXT, "recvfrom");
		recvmsg_f = (recvmsg_fun)dlsym(RTLD_NEXT, "recvmsg");
		write_f = (write_fun)dlsym(RTLD_NEXT, "write");
		writev_f = (writev_fun)dlsym(RTLD_NEXT, "writev");
		send_f = (send_fun)dlsym(RTLD_NEXT, "send");
		sendto_f = (sendto_fun)dlsym(RTLD_NEXT, "sendto");
		sendmsg_f = (sendmsg_fun)dlsym(RTLD_NEXT, "sendmsg");
		close_f = (close_fun)dlsym(RTLD_NEXT, "close");
		fcntl_f = (fcntl_fun)dlsym(RTLD_NEXT, "fcntl");
		ioctl_f = (ioctl_fun)dlsym(RTLD_NEXT, "ioctl");
		getsockopt_f = (getsockopt_fun)dlsym(RTLD_NEXT, "getsockopt");
		setsockopt_f = (setsockopt_fun)dlsym(RTLD_NEXT, "setsockopt");
	}

	struct _HookIniter {
		_HookIniter() {
			hook_init();
		}
	};
	
	bool IsHookEnable() { return t_hook_enable; }
	void SetHookEnable(bool flag) { t_hook_enable = flag; }
	static _HookIniter g_hook_initer;
}
template <typename OriginFunc, typename... Args>
static ssize_t do_io(int32_t fd, OriginFunc func, const char* hook_func_name
		, crazy::SelectEvent event, int32_t timeout_so, Args&&... args) {
	if (!crazy::t_hook_enable) {
		return func(fd, std::forward<Args>(args)...);
	}
	
	auto ctx = FD_MANAGER().Get(fd);
	if (!ctx) {
		return func(fd, std::forward<Args>(args)...);
	}

	if (ctx->IsClose()) {
		errno = EBADF;
		return -1;
	}

	if (!ctx->IsSocket() || ctx->GetUserNonblock()) {
		return func(fd, std::forward<Args>(args)...);
	}
	
	uint64_t timeout = 0;
	if (timeout_so == SO_RCVTIMEO) {
		timeout = ctx->GetRecvTimeout();
	} else {
		timeout = ctx->GetSendTimeout();
	}
	do {
		ssize_t n = func(fd, std::forward<Args>(args)...);
		while (n == -1 && errno == EINTR) {
			n == func(fd, std::forward<Args>(args)...);
		}
		if (n == -1 && errno == EAGAIN) {
			crazy::Coroutine::Ptr coroutine = crazy::Coroutine::GetThis();
			SELECTOR().RegisterEvent(fd, event, [coroutine] () {
				std::cout << "coroutine id = " << coroutine->GetId() << " status = " << static_cast<int32_t>(coroutine->GetCoroutineStatus()) << std::endl;
				coroutine->SetCoroutineStatus(crazy::CoroutineStatus::Ready);
			});
			crazy::Coroutine::YieldToHold();
		} else {
			return n;
		}
	} while (true);
}
extern "C" {
	sleep_fun sleep_f = nullptr;
	usleep_fun usleep_f = nullptr;
	nanosleep_fun nanosleep_f = nullptr;
	socket_fun socket_f = nullptr;
	connect_fun connect_f = nullptr;
	accept_fun accept_f = nullptr;
	read_fun read_f = nullptr;
	readv_fun readv_f = nullptr;
	recv_fun recv_f = nullptr;
	recvfrom_fun recvfrom_f = nullptr;
	recvmsg_fun recvmsg_f = nullptr;
	write_fun write_f = nullptr;
	writev_fun writev_f = nullptr;
	send_fun send_f = nullptr;
	sendto_fun sendto_f = nullptr;
	sendmsg_fun sendmsg_f = nullptr;
	close_fun close_f = nullptr;
	fcntl_fun fcntl_f = nullptr;
	ioctl_fun ioctl_f = nullptr;
	getsockopt_fun getsockopt_f = nullptr;
	setsockopt_fun setsockopt_f = nullptr;
	
	unsigned int sleep(unsigned int seconds) {
		if (!crazy::t_hook_enable) {
			return sleep_f(seconds);
		}
		crazy::Coroutine::Ptr cur_coroutine = crazy::Coroutine::GetThis();
		TIMER_MANAGER().AddTimer(seconds * 1000, [cur_coroutine] () {
			cur_coroutine->SetCoroutineStatus(crazy::CoroutineStatus::Ready);
		});
		crazy::Coroutine::YieldToHold();
	}
	int usleep(useconds_t usec) {
		if (!crazy::t_hook_enable) {
			return sleep_f(usec);
		}
		crazy::Coroutine::Ptr cur_coroutine = crazy::Coroutine::GetThis();
		TIMER_MANAGER().AddTimer(usec / 1000, [cur_coroutine] () {
			cur_coroutine->SetCoroutineStatus(crazy::CoroutineStatus::Ready);
		});
		crazy::Coroutine::YieldToHold();
	}
	int nanosleep(const struct timespec *req, struct timespec *rem) {
		if(!crazy::t_hook_enable) {
    	    return nanosleep_f(req, rem);
    	}
		int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 /1000;
		crazy::Coroutine::Ptr cur_coroutine = crazy::Coroutine::GetThis();
		TIMER_MANAGER().AddTimer(timeout_ms, [cur_coroutine] () {
			cur_coroutine->SetCoroutineStatus(crazy::CoroutineStatus::Ready);
		});
		crazy::Coroutine::YieldToHold();
	}
	int socket(int domain, int type, int protocol) {
		if (!crazy::t_hook_enable) {
			return socket_f(domain, type, protocol);
		}
		auto fd = socket_f(domain, type, protocol);
		if (fd == -1) {
			return fd;
		}
		FD_MANAGER().Get(fd, true);
		return fd;
	}
	int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
		return connect_f(sockfd, addr, addrlen);
	}
	int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
		auto fd = do_io(s, accept_f, "accept", crazy::SelectEvent::Read, SO_RCVTIMEO, addr, addrlen);
		if (fd >= 0) {
			FD_MANAGER().Get(fd, true);
		}
		return fd;
	}
	ssize_t read(int fd, void *buf, size_t count) {
		return do_io(fd, read_f, "read", crazy::SelectEvent::Read, SO_RCVTIMEO, buf, count);
	}
	ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
		return do_io(fd, readv_f, "readv", crazy::SelectEvent::Read, SO_RCVTIMEO, iov, iovcnt);
	}
	ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
		return do_io(sockfd, recv_f, "recv", crazy::SelectEvent::Read, SO_RCVTIMEO, buf, len, flags);
	}
	ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
		return do_io(sockfd, recvfrom_f, "recvfrom", crazy::SelectEvent::Read, SO_RCVTIMEO , buf, len, flags, src_addr, addrlen);
	}
	ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
		return do_io(sockfd, recvmsg_f, "recvmsg", crazy::SelectEvent::Read, SO_RCVTIMEO, msg, flags);
	}
	ssize_t write(int fd, const void *buf, size_t count) {
		return do_io(fd, write_f, "write", crazy::SelectEvent::Write, SO_SNDTIMEO, buf, count);
	}
	ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
		return do_io(fd, writev_f, "writev", crazy::SelectEvent::Write, SO_SNDTIMEO, iov, iovcnt);
	}
	ssize_t send(int s, const void *msg, size_t len, int flags) {
		return do_io(s, send_f, "send", crazy::SelectEvent::Write, SO_SNDTIMEO, msg, len, flags);
	}
	ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
		return do_io(s, sendto_f, "sendto", crazy::SelectEvent::Write, SO_SNDTIMEO, msg, len, flags, to, tolen);
	}
	ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
		return do_io(s, sendmsg_f, "sendmsg", crazy::SelectEvent::Write, SO_SNDTIMEO, msg, flags);
	}
	int close(int fd) {
		if (!crazy::t_hook_enable) {
			return close_f(fd);
		}
		auto ctx = FD_MANAGER().Get(fd);
		if (ctx) {
			SELECTOR().CancelAllEvent(fd);
			FD_MANAGER().Del(fd);
		}
		return close_f(fd);
	}
	int fcntl(int fd, int cmd, ... /* arg */ ) {
		va_list va;
    		va_start(va, cmd);
		switch (cmd) {
		case F_SETFL: {
			int arg = va_arg(va, int);
                	va_end(va);
                	auto ctx =  FD_MANAGER().Get(fd);
			if(!ctx || ctx->IsClose() || !ctx->IsSocket()) {
                    		return fcntl_f(fd, cmd, arg);
                	}
                	ctx->SetUserNonblock(arg & O_NONBLOCK);
                	if(ctx->GetSysNonblock()) {
                    		arg |= O_NONBLOCK;
                	} else {
                    		arg &= ~O_NONBLOCK;
                	}
                	return fcntl_f(fd, cmd, arg);
		} break;
		case F_GETFL: {
			va_end(va);
                	int arg = fcntl_f(fd, cmd);
                	auto ctx = FD_MANAGER().Get(fd);
			if(!ctx || ctx->IsClose() || !ctx->IsSocket()) {
                    		return arg;
                	}
                	if(ctx->GetUserNonblock()) {
                    		return arg | O_NONBLOCK;
                	} else {
                    		return arg & ~O_NONBLOCK;
                	}
		} break;
		case F_DUPFD:
        	case F_DUPFD_CLOEXEC:
        	case F_SETFD:
        	case F_SETOWN:
        	case F_SETSIG:
        	case F_SETLEASE:
        	case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        	case F_SETPIPE_SZ:
#endif
		{
			int arg = va_arg(va, int);
                	va_end(va);
                	return fcntl_f(fd, cmd, arg);
		} break;
		case F_GETFD:
        	case F_GETOWN:
        	case F_GETSIG:
        	case F_GETLEASE:
#ifdef F_GETPIPE_SZ
       		case F_GETPIPE_SZ:
#endif
		{
			va_end(va);
                	return fcntl_f(fd, cmd);
		} break;
		case F_SETLK:
        	case F_SETLKW:
        	case F_GETLK: {
			struct flock* arg = va_arg(va, struct flock*);
                	va_end(va);
                	return fcntl_f(fd, cmd, arg);
		} break;
		case F_GETOWN_EX:
        	case F_SETOWN_EX: {
			struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                	va_end(va);
                	return fcntl_f(fd, cmd, arg);
		} break;
		default: {
			va_end(va);
			return fcntl_f(fd, cmd);
		}
		}
	}
	int ioctl(int d, unsigned long int request, ...) {
		va_list va;
    		va_start(va, request);
    		void* arg = va_arg(va, void*);
    		va_end(va);

    		if(FIONBIO == request) {
        		bool user_nonblock = !!*(int*)arg;
        		auto ctx = FD_MANAGER().Get(d);
			if(!ctx || ctx->IsClose() || !ctx->IsSocket()) {
            			return ioctl_f(d, request, arg);
        		}
        		ctx->SetUserNonblock(user_nonblock);
    		}
    		return ioctl_f(d, request, arg);
	}
	int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
		return getsockopt_f(sockfd, level, optname, optval, optlen);
	}
	int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
		if (!crazy::t_hook_enable) {
			return setsockopt_f(sockfd, level, optname, optval, optlen);
		}
		if (level == SOL_SOCKET) {
			auto v = (const timeval*)optval;
			auto ctx = FD_MANAGER().Get(sockfd);
			if (ctx && optname == SO_RCVTIMEO) {
				ctx->SetRecvTimeout(v->tv_sec * 1000 + v->tv_usec / 1000);
			} else if (ctx && optname == SO_SNDTIMEO) {
				ctx->SetSendTimeout(v->tv_sec * 1000 + v->tv_usec / 1000);
			}
		}
		return setsockopt_f(sockfd, level, optname, optval, optlen);
	}
}
