/**
 * @file cond_mutex.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 条件变量互斥锁
 * @version 0.1
 * @date 13
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <condition_variable>
#include <mutex>

namespace crazy {
    /**
     * @brief 条件变量互斥锁.
     */
    class CondMutex {
    public:
        /**
         * @brief 上锁.
         */
        void lock();
        /**
         * @brief 解锁.
         */
        void unlock();
        /**
         * @brief 尝试上锁.
         */
        bool tryLock();
        /**
         * @brief 同步等待.
         */
        void wait();
        /**
         * @brief 通知一个等待的线程.
         */
        void signal();
        /**
         * @brief 通知所有等待的线程.
         */
        void signalAll();

    private:
        //! 条件变量
        std::condition_variable condition_variable_;
        //! 互斥锁
        std::mutex mutex_;
    };
    /**
     * 条件变量互斥锁守护器.
     */
    class CondMutexGuard final {
    public:
        /**
         * @brief 构造函数.
         */
        CondMutexGuard(CondMutex& condMutex);
        /**
         * @brief 析构函数.
         */
        ~CondMutexGuard();

    private:
        //! 条件变量互斥锁
        CondMutex& condMutex_;
    };
}
