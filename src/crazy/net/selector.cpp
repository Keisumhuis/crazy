#include "crazy/net/selector.h"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/eventfd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include "crazy/utils.h"

namespace crazy {

    Timer::Timer(const std::string& name, uint64_t interval
        , uint64_t triggerTimestime, std::function<void()> callback)
        : name_(name), interval_(interval)
        , triggerTimestime_(triggerTimestime), callback_(callback) {
    }

    TimePointTask::TimePointTask(const std::string& key, int hour, int minute, int second, std::function<void()> callback)
        : key_(key), hour_(hour), minute_(minute), second_(second), callback_(callback) {
    }

    Selector::Selector() {
#ifdef _WIN32
        epoll_ = epoll_create1(0);
        if (!epoll_) {
            throw std::runtime_error("Failed to create epoll");
        }
        SOCKET sockets[2];
        if (socket(AF_INET, SOCK_STREAM, 0) == INVALID_SOCKET) {
            epoll_close(epoll_);
            throw std::runtime_error("Failed to create socket");
        }
        wakeFd_ = socket(AF_INET, SOCK_STREAM, 0);
        wakeWriteFd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (wakeFd_ == INVALID_SOCKET || wakeWriteFd_ == INVALID_SOCKET) {
            if (wakeFd_ != INVALID_SOCKET) closesocket(wakeFd_);
            if (wakeWriteFd_ != INVALID_SOCKET) closesocket(wakeWriteFd_);
            epoll_close(epoll_);
            throw std::runtime_error("Failed to create wake sockets");
        }
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;

        if (bind(wakeFd_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(wakeFd_);
            closesocket(wakeWriteFd_);
            epoll_close(epoll_);
            throw std::runtime_error("Failed to bind wake socket");
        }
        int addrLen = sizeof(addr);
        getsockname(wakeFd_, (sockaddr*)&addr, &addrLen);

        if (listen(wakeFd_, 1) == SOCKET_ERROR) {
            closesocket(wakeFd_);
            closesocket(wakeWriteFd_);
            epoll_close(epoll_);
            throw std::runtime_error("Failed to listen on wake socket");
        }
        if (connect(wakeWriteFd_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(wakeFd_);
            closesocket(wakeWriteFd_);
            epoll_close(epoll_);
            throw std::runtime_error("Failed to connect wake socket");
        }
        SOCKET accepted = accept(wakeFd_, nullptr, nullptr);
        if (accepted == INVALID_SOCKET) {
            closesocket(wakeFd_);
            closesocket(wakeWriteFd_);
            epoll_close(epoll_);
            throw std::runtime_error("Failed to accept wake connection");
        }
        closesocket(wakeFd_);
        wakeFd_ = accepted;

        u_long nonBlocking = 1;
        ioctlsocket(wakeFd_, FIONBIO, &nonBlocking);
        ioctlsocket(wakeWriteFd_, FIONBIO, &nonBlocking);

        epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = wakeFd_;
        if (epoll_ctl(epoll_, EPOLL_CTL_ADD, wakeFd_, &event) != 0) {
            closesocket(wakeFd_);
            closesocket(wakeWriteFd_);
            epoll_close(epoll_);
            throw std::runtime_error("Failed to add wake socket to epoll");
        }

#else
        epoll_ = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_ < 0) {
            throw std::runtime_error("Failed to create epoll: " + std::string(strerror(errno)));
        }

        wakeFd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (wakeFd_ < 0) {
            close(epoll_);
            throw std::runtime_error("Failed to create eventfd: " + std::string(strerror(errno)));
        }

        epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = wakeFd_;
        if (epoll_ctl(epoll_, EPOLL_CTL_ADD, wakeFd_, &event) < 0) {
            close(wakeFd_);
            close(epoll_);
            throw std::runtime_error("Failed to add wake fd to epoll: " + std::string(strerror(errno)));
        }
#endif
    }

    Selector::~Selector() {
#ifdef _WIN32
        if (wakeFd_ != INVALID_SOCKET) {
            closesocket(wakeFd_);
        }
        if (wakeWriteFd_ != INVALID_SOCKET) {
            closesocket(wakeWriteFd_);
        }
        if (epoll_) {
            epoll_close(epoll_);
        }
#else
        if (epoll_ >= 0) {
            close(epoll_);
        }
        if (wakeFd_ >= 0) {
            close(wakeFd_);
        }
#endif
    }

    void Selector::wakeup() {
#ifdef _WIN32
        char buffer = 1;
        auto n = send(wakeWriteFd_, &buffer, 1, 0);
#else
        uint64_t value = 1;
        ssize_t n = ::write(wakeFd_, &value, sizeof(value));
        if (n != sizeof(value) && errno != EAGAIN) {
            std::cerr << "Wakeup write error: " << strerror(errno) << std::endl;
        }
#endif
    }

    void Selector::registerTimer(const std::string& name, uint32_t milliseconds, std::function<void()> callback) {
        timers_[GetCurrentMS() + milliseconds] = std::make_shared<Timer>(name, milliseconds, GetCurrentMS() + milliseconds, callback);
        wakeup();
    }

    void Selector::unregisterTimer(const std::string& name) {
        for (auto it = timers_.begin(); it != timers_.end(); ) {
            if (name == it->second->name_) {
                it = timers_.erase(it);
                wakeup();
                return;
            }
            ++it;
        }
    }

    void Selector::registerTimePointTask(const std::string& key, int hour, int minute, int second, std::function<void()> callback) {
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
            throw std::invalid_argument("Invalid time point parameters");
        }

        timePointTasks_[key] = std::make_shared<TimePointTask>(key, hour, minute, second, callback);
        wakeup();
    }

    void Selector::unregisterTimePointTask(const std::string& key) {
        auto it = timePointTasks_.find(key);
        if (it != timePointTasks_.end()) {
            timePointTasks_.erase(it);
            wakeup();
        }
    }

    void Selector::registerEvent(int32_t fd, SelectorEventType type, std::function<void()> callback) {
        SelectorEvent::ptr handle;
        auto it = handleEvents_.find(fd);
        if (it == handleEvents_.end()) {
            handle = std::make_shared<SelectorEvent>();
            handle->fd_ = fd;
            handleEvents_[fd] = handle;
        }
        else {
            handle = it->second;
        }

        handle->type_ = static_cast<SelectorEventType>(handle->type_ | type);

        if (type & SelectorEventType::read) {
            handle->read_ = callback;
        }
        if (type & SelectorEventType::write) {
            handle->write_ = callback;
        }

        epoll_event event;
        event.events = handle->type_;
        event.data.fd = fd;

        int op = (it == handleEvents_.end()) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
        if (epoll_ctl(epoll_, op, fd, &event) != 0) {
            throw std::runtime_error("epoll_ctl failed");
        }
        wakeup();
    }

    void Selector::unregisterEvent(int32_t fd, SelectorEventType type) {
        auto it = handleEvents_.find(fd);
        if (it == handleEvents_.end()) {
            return;
        }

        auto handle = it->second;
        handle->type_ = static_cast<SelectorEventType>(handle->type_ & ~type);

        if (type & SelectorEventType::read) {
            handle->read_ = nullptr;
        }
        if (type & SelectorEventType::write) {
            handle->write_ = nullptr;
        }

        if (handle->type_ == 0) {
            epoll_ctl(epoll_, EPOLL_CTL_DEL, fd, nullptr);
            handleEvents_.erase(it);
        }
        else {
            epoll_event event;
            event.events = handle->type_;
            event.data.fd = fd;
            epoll_ctl(epoll_, EPOLL_CTL_MOD, fd, &event);
        }
        wakeup();
    }

    void Selector::cancelEvent(int32_t fd) {
        auto it = handleEvents_.find(fd);
        if (it == handleEvents_.end()) {
            return;
        }

        epoll_ctl(epoll_, EPOLL_CTL_DEL, fd, nullptr);
        handleEvents_.erase(it);
        wakeup();
    }

    void Selector::select() {
        epoll_event events[1024];

        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm = *std::localtime(&now_time_t);

        int64_t min_interval = -1;

        if (!timers_.empty()) {
            uint64_t current_ms = GetCurrentMS();
            uint64_t next_timer = timers_.begin()->second->triggerTimestime_;
            if (next_timer > current_ms) {
                min_interval = next_timer - current_ms;
            }
            else {
                min_interval = 0;
            }
        }

        for (auto& it : timePointTasks_) {
            auto& task = it.second;

            std::tm task_tm = now_tm;
            task_tm.tm_hour = task->hour_;
            task_tm.tm_min = task->minute_;
            task_tm.tm_sec = task->second_;

            auto task_time = std::mktime(&task_tm);
            auto task_time_point = std::chrono::system_clock::from_time_t(task_time);

            if (task_time_point <= now) {
                task_tm.tm_mday += 1;
                task_time = std::mktime(&task_tm);
                task_time_point = std::chrono::system_clock::from_time_t(task_time);
            }

            auto time_until_task = std::chrono::duration_cast<std::chrono::milliseconds>(
                task_time_point - now).count();

            if (time_until_task >= 0) {
                if (min_interval == -1 || time_until_task < min_interval) {
                    min_interval = time_until_task;
                }
            }
        }

        int timeout = min_interval == -1 ? -1 : static_cast<int>(min_interval);

        int numEvents = epoll_wait(epoll_, events, 1024, timeout);

        if (numEvents < 0) {
            if (errno == EINTR) {
                return;
            }
            throw std::runtime_error("epoll_wait failed: " + std::string(strerror(errno)));
        }

        for (int i = 0; i < numEvents; ++i) {
            int fd = events[i].data.fd;
            if (fd == wakeFd_) {
#ifdef _WIN32
                char buffer[1];
                ::recv(wakeFd_, buffer, sizeof(buffer), 0);
#else
                uint64_t value;
                ssize_t n = ::read(wakeFd_, &value, sizeof(value));
                if (n < 0 && errno != EAGAIN) {
                    std::cerr << "Wakeup read error: " << strerror(errno) << std::endl;
                }
#endif
                continue;
            }
            auto it = handleEvents_.find(fd);
            if (it == handleEvents_.end()) {
                continue;
            }
            else {
                if (events[i].events & SelectorEventType::read) {
                    if (it->second->read_) {
                        it->second->read_();
                    }
                }
                if (events[i].events & SelectorEventType::write) {
                    if (it->second->write_) {
                        it->second->write_();
                    }
                }
            }
        }

        auto currentTime = GetCurrentMS();
        auto tmpTimer = timers_;
        for (auto it = tmpTimer.begin(); it != tmpTimer.end(); ) {
            if (it->second->triggerTimestime_ <= currentTime) {
                if (it->second->callback_) {
                    it->second->callback_();
                }
                it->second->triggerTimestime_ = currentTime + it->second->interval_;
                ++it;
            }
            else {
                ++it;
            }
        }

        now = std::chrono::system_clock::now();
        now_time_t = std::chrono::system_clock::to_time_t(now);
        now_tm = *std::localtime(&now_time_t);

        for (auto& it : timePointTasks_) {
            auto& task = it.second;
            if (now_tm.tm_hour == task->hour_ &&
                now_tm.tm_min == task->minute_ &&
                now_tm.tm_sec == task->second_) {
                if (task->callback_) {
                    task->callback_();
                }
            }
        }
    }

}
