#include "selector.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>

#include <memory>
#include <utility>

namespace crazy {

Selector::Selector() {
	m_fd = epoll_create(5000);
	CRAZY_ASSERT(m_fd != -1);
	auto rt = pipe(m_pipe);
	CRAZY_ASSERT(!rt);

	epoll_event event;
	memset(&event, 0, sizeof(event));
	event.events = EPOLLIN;
	event.data.fd = m_pipe[0];

	rt = fcntl(m_pipe[0], F_SETFL, O_NONBLOCK);
	CRAZY_ASSERT(!rt);

	rt = epoll_ctl(m_fd, EPOLL_CTL_ADD, m_pipe[0], &event);
	CRAZY_ASSERT(!rt);
	Start();
}
Selector::~Selector() {
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "destory";
	if (!m_stopping) {
		Stop();
	}
	close(m_fd);
	close(m_pipe[0]);
	close(m_pipe[1]);
}
void Selector::Start() {
	m_stopping = false;
	m_thread = std::make_shared<Thread>([this](){this->Run();});
}
void Selector::Stop() {
	m_stopping = true;
	Tickle();
}
std::map<int32_t, AsyncEvent::Ptr> Selector::Steal(size_t count) {
	if (count > m_events.size()) {
		count = m_events.size();
	}

	std::map<int32_t, AsyncEvent::Ptr> tmp;
	for (auto it = m_events.begin(); it != m_events.end(); ++it) {
		tmp[it->first] = it->second;
		m_events.erase(it);
		if (tmp.size() >= count) {
			break;
		}
	}
	return tmp;
}
std::map<int32_t, AsyncEvent::Ptr> Selector::StealAll() {
	Mutex::Guard guard(m_mutex);
	for (auto& [key, val] : m_events) {
		CancelAllEvent(key);
	}
	m_events.clear();
	return m_events;
}
size_t Selector::GetAsyncEventCount() const {
	return m_events.size();
}
std::map<int32_t, AsyncEvent::Ptr>& Selector::GetEvents() {
	return m_events;
}
void Selector::RegisterEvent(int32_t fd, SelectEvent events
		, std::function<void ()>&& cb) {
	Mutex::Guard gurad(m_mutex);
	AsyncEvent::Ptr ctx;
	auto it = m_events.find(fd);
	if (m_events.end() != it) {
		ctx = it->second;
	} else {
		ctx = std::make_shared<AsyncEvent>();
	}
	ctx->fd = fd;
	
	if (static_cast<int32_t>(SelectEvent::Read) & static_cast<int32_t>(events)) {
		ctx->in.swap(cb);
	}
	if (static_cast<int32_t>(SelectEvent::Write) & static_cast<int32_t>(events)) {
		ctx->out.swap(cb);
	}
	m_events[fd] = ctx;
	
	int32_t op = ctx->event == SelectEvent::None ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
	epoll_event event;
	event.events = static_cast<int32_t>(ctx->event) | static_cast<int32_t>(events);
	event.data.ptr = ctx.get();
	ctx->event = static_cast<SelectEvent>(static_cast<int32_t>(ctx->event) | static_cast<int32_t>(events));

	auto rt = epoll_ctl(m_fd, op, fd, &event);
	if (rt) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "epoll_ctl(" << m_fd << ", " << op << ", " << fd << "), "
			<< " rt = " << rt << " errno = " << errno << " errstr = " << strerror(errno);
	}
	Tickle();
}
void Selector::UnregisterEvent(int32_t fd, SelectEvent events) {
	Mutex::Guard gurad(m_mutex);
	AsyncEvent::Ptr ctx;
	auto it = m_events.find(fd);
	if (m_events.end() == it) {
		return ;
	}
	
	if (static_cast<int32_t>(ctx->event) & 
		static_cast<int32_t>(events)) {
		return;
	}
	SelectEvent newEvent = static_cast<SelectEvent>(static_cast<int32_t>(ctx->event) & ~static_cast<int32_t>(events));
	int32_t op = newEvent == SelectEvent::None ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
	epoll_event event;
	event.events = static_cast<int32_t>(newEvent);
	event.data.ptr = ctx.get();

	auto rt = epoll_ctl(m_fd, op, fd, &event);
	if (rt) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "epoll_ctl(" << m_fd << ", " << op << ", " << fd << "), "
			<< " rt = " << rt << " errno = " << errno << " errstr = " << strerror(errno);
	}
	Tickle();
}
void Selector::CancelAllEvent(int32_t fd) {
	Mutex::Guard guard(m_mutex);
	AsyncEvent::Ptr ctx;
	auto it = m_events.find(fd);
	if (m_events.end() == it) {
		return;
	}
	
	epoll_event event;
	event.events = static_cast<int32_t>(it->second->event);
	event.data.ptr = it->second.get();

	auto rt = epoll_ctl(m_fd, EPOLL_CTL_DEL, fd, &event);
	if (rt) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "epoll_ctl(" << m_fd << ", " << EPOLL_CTL_DEL << ", " << fd << "), "
			<< " rt = " << rt << " errno = " << errno << " errstr = " << strerror(errno);
	}

	m_events.erase(fd);
	Tickle();
}
void Selector::Tickle() {
	auto rt = write(m_pipe[1], "T", 1);
	CRAZY_ASSERT(rt);
}
bool Selector::IsStopping() const {
	return m_stopping;
}
void Selector::AddTimer(uint64_t ms, std::function<void ()> cb, bool recurring) {
	TimerManager::AddTimer(ms, cb, recurring);
	Tickle();
}
void Selector::Run() {
	try {
		uint64_t maxEvent = 256;
		epoll_event events[maxEvent];
		while (!m_stopping) {

			if (m_stopping) {
				CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "Selector is exit";
				break;
			}
			
			int64_t timeout_ms = GetNextTimer();
			timeout_ms = timeout_ms ? timeout_ms : -1;

			int32_t rt = 0;	
			do {
				rt = epoll_wait(m_fd, events, maxEvent, timeout_ms);
				if (rt < 0 && errno == EINTR) {
				} else {
					break;
				}
			} while (true);
			
			if (rt == 0) {
				Trigger();
				continue;
			}

			for (auto i = 0; i < rt; ++i) {
				epoll_event &event = events[i];
				if (event.data.fd == m_pipe[0]) {
					uint8_t data[256];
					while (read(m_pipe[0], data, sizeof(data)) > 0);
					continue;
				}

				auto asyncEvent = (AsyncEvent*)event.data.ptr;		
				auto it = m_events.find(asyncEvent->fd);
				if (it != m_events.end()) {
					if (event.events & EPOLLIN) {
						Mutex::Guard guard(it->second->mutex);
						(it->second->in)();
					}
					if (event.events & EPOLLOUT) {
						Mutex::Guard guard(it->second->mutex);
						(it->second->out)();
					}
				}
			}
		}
	} catch (std::exception& e) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "exception : " << e.what();
	}
}

}
