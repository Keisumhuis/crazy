#include "hook.h"

namespace crazy {
	static bool t_hook_enable = false;
	
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

	// unsigned int sleep(unsigned int seconds) {
	// }
	// int usleep(useconds_t usec) {
	// }
	// int nanosleep(const struct timespec *req, struct timespec *rem) {
	// }
	// int socket(int domain, int type, int protocol) {
	// }
	// int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	// }
	// int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
	// }
	// ssize_t read(int fd, void *buf, size_t count) {
	// }
	// ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
	// }
	// ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
	// }
	// ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
	// }
	// ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
	// }
	// ssize_t write(int fd, const void *buf, size_t count) {
	// }
	// ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
	// }
	// ssize_t send(int s, const void *msg, size_t len, int flags) {
	// }
	// ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
	// }
	// ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
	// }
	// int close(int fd) {
	// }
	// int fcntl(int fd, int cmd, ... /* arg */ ) {
	// }
	// int ioctl(int d, unsigned long int request, ...) {
	// }
	// int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
	// }
	// int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
	// }
}
