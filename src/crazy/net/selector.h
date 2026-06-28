/**
 * @file selector.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 多路复用
 * @version 0.1
 * @date 14
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>
#include <cstdint>
#include <mutex>
#include <string> 

#ifdef _WIN32
#include "crazy/net/wepoll.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/epoll.h>
#endif

#include "crazy/nocopyable.h"

namespace crazy {
    /**
     * @brief 多路复用类型.
     */
    enum SelectorEventType : int32_t {
        read = EPOLLIN,
        write = EPOLLOUT,
    };

    /**
     * @brief 多路复用事件.
     */
    class SelectorEvent {
    public:
        using ptr = std::shared_ptr<SelectorEvent>;
        //! 文件描述符
        int32_t fd_ = -1;
        //! 注册事件
        SelectorEventType type_ = SelectorEventType::read;
        //! 触发事件
        SelectorEventType triggered_ = SelectorEventType::read;
        //! 读事件回调
        std::function<void()> read_;
        //! 写事件回调
        std::function<void()> write_;
    };

    /**
     * @brief 定时任务.
     */
    struct Timer {
        using ptr = std::shared_ptr<Timer>;
        /**
         * @brief 构造函数.
         */
        explicit Timer(const std::string& name, uint64_t interval, uint64_t triggerTimestime, std::function<void()> callback);
        //! 名字
        std::string name_;
        //! 间隔时间
        uint64_t interval_;
        //! 触发时间
        uint64_t triggerTimestime_;
        //! 触发回调函数
        std::function<void()> callback_;
    };

    /**
     * @brief 时间点任务（绝对时间触发）
     */
    struct TimePointTask {
        using ptr = std::shared_ptr<TimePointTask>;
        explicit TimePointTask(const std::string& key, int hour, int minute, int second, std::function<void()> callback);
        //! 任务标识
        std::string key_;
        //! 触发时间（时）
        int hour_;
        //! 触发时间（分）
        int minute_;
        //! 触发时间（秒）
        int second_;
        //! 触发回调函数
        std::function<void()> callback_;
    };

    /**
     * @brief 多路复用基类.
     */
    class Selector : public Noncopyable {
    public:
        using ptr = std::shared_ptr<Selector>;
        /**
         * @brief 构造函数.
         */
        Selector();
        /**
         * @brief 析构函数.
         */
        virtual ~Selector();
        /**
         * @brief 注册事件.
         */
        void registerEvent(int32_t fd, SelectorEventType type, std::function<void()> callback);
        /**
         * @brief 取消注册事件.
         */
        void unregisterEvent(int32_t fd, SelectorEventType type);
        /**
         * @brief 清除所有事件.
         */
        void cancelEvent(int32_t fd);
        /**
         * @brief 多路复用.
         */
        void select();
        /**
         * @brief 唤醒.
         */
        void wakeup();
        /**
         * @brief 注册定时任务.
         */
        void registerTimer(const std::string& name, uint32_t milliseconds, std::function<void()> callback);
        /**
         * @brief 取消注册定时认为.
         */
        void unregisterTimer(const std::string& name);
        /**
         * @brief 注册时间点任务（每日固定时间触发）
         */
        void registerTimePointTask(const std::string& key, int hour, int minute, int second, std::function<void()> callback);
        /**
         * @brief 取消注册时间点任务
         */
        void unregisterTimePointTask(const std::string& key);

    private:
#ifdef _WIN32
        HANDLE epoll_ = nullptr;
        SOCKET wakeFd_ = INVALID_SOCKET;
        SOCKET wakeWriteFd_ = INVALID_SOCKET;
#else
        int epoll_ = -1;
        int wakeFd_ = -1;
#endif
        //! 注册事件
        std::map<int32_t, SelectorEvent::ptr> handleEvents_;
        //! 定时任务管理
        std::map<uint64_t, Timer::ptr> timers_;
        //! 时间点任务管理
        std::map<std::string, TimePointTask::ptr> timePointTasks_;
    };
}
