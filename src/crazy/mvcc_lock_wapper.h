/**
 * @file mvcc_lock_wapper.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief MVCC锁 - 纯事务版本（修正版）
 * @version 0.3
 * @date 2025
 *
 * @copyright Copyright (c) 2025
 */
#pragma once
#include <atomic>
#include <type_traits>
#include "crazy/nocopyable.h"
#include "crazy/cond_mutex.h"

namespace crazy {
    template <typename type_value>
    class MVCCLockWapper;

    /**
     * @brief MVCC写事务类
     * @details 在构造函数中锁定互斥锁，在析构函数中提交修改并自动解锁
     *          支持多次修改后统一提交，异常安全。
     */
    template <typename type_value>
    class MVCCWriteTransaction : public Noncopyable {
        friend class MVCCLockWapper<type_value>;
    public:
        /**
         * @brief 移动构造函数
         */
        MVCCWriteTransaction(MVCCWriteTransaction&& other) noexcept
            : wrapper_(other.wrapper_)
            , next_idx_(other.next_idx_)
            , is_active_(other.is_active_) {
            other.is_active_ = false;
            other.wrapper_ = nullptr; // 避免原对象析构时误操作
        }

        /**
         * @brief 移动赋值运算符
         */
        MVCCWriteTransaction& operator=(MVCCWriteTransaction&& other) noexcept {
            if (this != &other) {
                if (is_active_) {
                    commit(); // 提交当前事务
                }
                wrapper_ = other.wrapper_;
                next_idx_ = other.next_idx_;
                is_active_ = other.is_active_;
                other.is_active_ = false;
                other.wrapper_ = nullptr;
            }
            return *this;
        }

        /**
         * @brief 析构时自动提交修改并释放锁
         */
        ~MVCCWriteTransaction() {
            if (is_active_) {
                commit(); // 提交并解锁
            }
            else if (wrapper_) {
                // 如果事务已提交但 wrapper_ 还存在，确保锁已释放（防御性）
                wrapper_->writeMutex_.unlock();
            }
        }

        /**
         * @brief 获取可修改的引用
         * @return 备用版本的引用（可修改）
         */
        type_value& get()& {
            return wrapper_->value_[next_idx_];
        }

        /**
         * @brief 禁止在右值上调用get()
         */
        type_value& get() && = delete;

        /**
         * @brief 获取当前值的副本
         * @return 当前值的副本
         */
        type_value getCurrentValue() const {
            return wrapper_->value_[wrapper_->currentIndex_.load(std::memory_order_acquire)];
        }

        /**
         * @brief 设置新值（拷贝语义）
         * @param new_value 新的值
         */
        void set(const type_value& new_value)& {
            wrapper_->value_[next_idx_] = new_value;
        }

        /**
         * @brief 设置新值（移动语义）
         * @param new_value 新的值
         */
        void set(type_value&& new_value)& {
            wrapper_->value_[next_idx_] = std::move(new_value);
        }

        /**
         * @brief 手动提交修改
         * @details 提交后事务变为非活动状态，析构时不会再次提交，并释放锁
         */
        void commit() {
            if (is_active_) {
                wrapper_->currentIndex_.store(next_idx_, std::memory_order_release);
                wrapper_->version_.fetch_add(1, std::memory_order_release);
                is_active_ = false;
                wrapper_->writeMutex_.unlock(); // 释放锁
                wrapper_ = nullptr; // 防止再次操作
            }
        }

        /**
         * @brief 放弃修改
         * @details 放弃后事务变为非活动状态，析构时不会提交，但锁仍要释放
         */
        void abort() noexcept {
            if (is_active_) {
                is_active_ = false;
                wrapper_->writeMutex_.unlock(); // 释放锁
                wrapper_ = nullptr;
            }
        }

        /**
         * @brief 检查事务是否处于活动状态
         * @return true 如果事务仍处于活动状态
         */
        bool isActive() const noexcept {
            return is_active_;
        }

        /**
         * @brief 获取下一个版本的索引（调试用）
         * @return 下一个版本的索引
         */
        int32_t getNextIndex() const noexcept {
            return next_idx_;
        }

    protected:
        /**
         * @brief 私有构造函数，只能由MVCCLockWapper创建
         * @param wrapper 父MVCCLockWapper对象的引用
         */
        explicit MVCCWriteTransaction(MVCCLockWapper<type_value>& wrapper)
            : wrapper_(&wrapper)
            , next_idx_(1 - wrapper.currentIndex_.load(std::memory_order_acquire))
            , is_active_(true) {
            wrapper_->writeMutex_.lock();
        }

    private:
        MVCCLockWapper<type_value>* wrapper_;
        int32_t next_idx_;
        bool is_active_;
    };

    /**
     * @brief MVCC锁 包装器 - 纯事务版本
     * @details 所有写操作都必须通过 WriteTransaction 进行，读操作仍然无锁
     */
    template <typename type_value>
    class MVCCLockWapper final : public Noncopyable {
        friend class MVCCWriteTransaction<type_value>;
    public:
        /**
         * @brief 构造函数
         * @param initial_value 初始值
         */
        explicit MVCCLockWapper(const type_value& initial_value = type_value{}) {
            value_[0] = initial_value;
            value_[1] = initial_value;
            currentIndex_.store(0, std::memory_order_release);
            version_.store(0, std::memory_order_release);
        }

        /**
         * @brief 开始一个写事务
         * @return MVCCWriteTransaction 对象，析构时自动提交并解锁
         */
        MVCCWriteTransaction<type_value> beginWrite() {
            return MVCCWriteTransaction<type_value>(*this);
        }

        /**
         * @brief 读取当前值（无锁，线程安全）
         * @return 当前的值副本
         */
        type_value read() const {
            int32_t idx = currentIndex_.load(std::memory_order_acquire);
            return value_[idx];
        }

        /**
         * @brief 读取当前值的引用
         * @return 当前值的常量引用
         * @warning 返回的引用只在当前版本未切换前有效
         *          如果后续有写操作，引用可能失效
         */
        const type_value& readRef() const {
            int32_t idx = currentIndex_.load(std::memory_order_acquire);
            return value_[idx];
        }

        /**
         * @brief 带版本号的读取
         * @param[out] version 返回当前版本号
         * @return 当前的值副本
         */
        type_value readWithVersion(int64_t& version) const {
            int32_t idx = currentIndex_.load(std::memory_order_acquire);
            version = version_.load(std::memory_order_acquire);
            return value_[idx];
        }

        /**
         * @brief 检查数据是否已更新（基于版本号）
         * @param old_version 之前读取的版本号
         * @return true 如果数据已更新
         */
        bool isUpdated(int64_t old_version) const {
            return version_.load(std::memory_order_acquire) != old_version;
        }

        /**
         * @brief 获取当前版本索引（用于调试）
         * @return 当前活跃版本的索引
         */
        int32_t getCurrentVersionIndex() const {
            return currentIndex_.load(std::memory_order_acquire);
        }

        /**
         * @brief 获取当前版本号（用于调试）
         * @return 当前版本号
         */
        int64_t getCurrentVersion() const {
            return version_.load(std::memory_order_acquire);
        }

    private:
        type_value value_[2];
        std::atomic<int32_t> currentIndex_{ 0 };
        std::atomic<int64_t> version_{ 0 };
        CondMutex writeMutex_;
    };
}
