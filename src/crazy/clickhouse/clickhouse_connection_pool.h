/**
 * @file clickhouse_connection_pool.h
 * @author keisum (Keisumhuis@gmail.com)
 * @brief ClickHouse 连接池
 * @version 0.1
 * @date 2024
 *
 * @copyright Copyright (c) 2024
 */
#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>
#include <unordered_set>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "crazy/clickhouse/clickhouse_connection.h"
#include "crazy/logger.h"

namespace crazy {
	/**
	 * @brief ClickHouse 连接池配置结构体
	 */
	struct ClickHouseConnectionPoolConfig {
		//！ 服务器地址
		std::string host;
		//！ 用户名
		std::string user;
		//！ 密码
		std::string password;
		//！ 默认数据库
		std::string database = "default";
		//！ 端口，默认 9000
		uint16_t port = 9000;
		//！ 最小连接数，默认 5
		uint32_t min_connections = 5;
		//！ 最大连接数，默认 20
		uint32_t max_connections = 20;
		//！ 最大空闲时间（秒），默认 300
		uint32_t max_idle_time = 300;
		//！ 最大等待时间（秒），默认 30
		uint32_t max_wait_time = 30;
		//！ 连接超时时间（秒），默认 10
		uint32_t connection_timeout = 10;

		/**
		 * @brief 验证配置有效性
		 * @return bool 配置有效返回 true，否则 false
		 */
		bool isValid() const {
			return !host.empty() && !user.empty() &&
				min_connections > 0 && max_connections >= min_connections &&
				port > 0;
		}
	};
	/**
	 * @brief ClickHouse 连接池异常类
	 */
	class ClickHouseConnectionPoolException : public ClickHouseException {
		using ClickHouseException::ClickHouseException;
	};
	/**
	 * @brief ClickHouse 连接池类
	 *
	 * 提供连接的获取、归还、健康检查等功能
	 * 支持连接泄漏检测和自动重连
	 */
	class ClickHouseConnectionPool final : public Noncopyable {
	public:
		using ptr = std::shared_ptr<ClickHouseConnectionPool>;

		/**
		 * @brief 构造函数
		 * @param config 连接池配置
		 */
		explicit ClickHouseConnectionPool(const ClickHouseConnectionPoolConfig& config)
			: config_(config)
			, running_(false) {
			if (!config_.isValid()) {
				throw ClickHouseConnectionPoolException("Invalid ClickHouse connection pool configuration");
			}
		}
		/**
		 * @brief 析构函数
		 */
		~ClickHouseConnectionPool() {
			stop();
		}
		/**
		 * @brief 启动连接池
		 * @return bool 启动成功返回 true，否则 false
		 */
		bool start() {
			std::lock_guard<std::mutex> lock(mutex_);

			if (running_) {
				CRAZY_SYSTEM_WARN() << "ClickHouse connection pool is already running";
				return true;
			}

			running_ = true;

			// 创建最小连接数的连接
			for (uint32_t i = 0; i < config_.min_connections; ++i) {
				if (!createConnection()) {
					CRAZY_SYSTEM_ERROR() << "Failed to create initial ClickHouse connection " << i;
				}
			}

			if (connections_.size() < config_.min_connections) {
				CRAZY_SYSTEM_WARN() << "Created only " << connections_.size()
					<< " connections, less than min_connections "
					<< config_.min_connections;
			}

			// 启动监控线程
			monitor_thread_ = std::thread(&ClickHouseConnectionPool::monitor, this);

			CRAZY_SYSTEM_INFO() << "ClickHouse connection pool started successfully, initial connections: "
				<< connections_.size();
			return true;
		}
		/**
		 * @brief 停止连接池
		 */
		void stop() {
			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (!running_) {
					return;
				}
				running_ = false;
			}
			condition_.notify_all();

			if (monitor_thread_.joinable()) {
				monitor_thread_.join();
			}
			std::lock_guard<std::mutex> lock(mutex_);
			while (!idle_connections_.empty()) {
				auto conn = idle_connections_.front();
				idle_connections_.pop();
				conn->close();
			}
			connections_.clear();
			CRAZY_SYSTEM_INFO() << "ClickHouse connection pool stopped";
		}
		/**
		 * @brief 获取数据库连接
		 * @param timeout_ms 超时时间（毫秒），0 表示无限等待
		 * @return ClickHouseConnection::ptr 数据库连接指针
		 * @throws ClickHouseConnectionPoolException 如果获取连接超时或失败
		 */
		ClickHouseConnection::ptr getConnection(uint32_t timeout_ms = 0) {
			std::unique_lock<std::mutex> lock(mutex_);

			auto wait_until = timeout_ms > 0 ?
				std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms) :
				std::chrono::steady_clock::time_point::max();

			while (running_) {
				if (!idle_connections_.empty()) {
					auto conn = idle_connections_.front();
					idle_connections_.pop();

					if (isConnectionValid(conn)) {
						CRAZY_SYSTEM_DEBUG() << "Get ClickHouse connection from pool, idle: " << idle_connections_.size();
						return createManagedConnection(conn);
					}
					else {
						CRAZY_SYSTEM_WARN() << "Removing invalid ClickHouse connection from pool";
						removeConnection(conn);
					}
				}

				if (connections_.size() < config_.max_connections) {
					auto conn = createConnection(false);
					if (conn) {
						CRAZY_SYSTEM_DEBUG() << "Create ClickHouse connection from pool, total: " << connections_.size();
						return createManagedConnection(conn);
					}
				}

				if (timeout_ms == 0) {
					condition_.wait(lock);
				}
				else {
					if (condition_.wait_until(lock, wait_until) == std::cv_status::timeout) {
						throw ClickHouseConnectionPoolException("Get ClickHouse connection timeout after " + std::to_string(timeout_ms) + "ms, "
							"pool stats: total=" + std::to_string(connections_.size()) +
							", idle=" + std::to_string(idle_connections_.size()));
					}
				}
			}
			throw ClickHouseConnectionPoolException("ClickHouse connection pool is not running");
		}
		/**
		 * @brief 获取连接池统计信息
		 * @return std::string 统计信息字符串
		 */
		std::string getStats() const {
			std::lock_guard<std::mutex> lock(mutex_);
			return "ClickHouse Connection Pool Stats: " +
				std::to_string(connections_.size()) + " total, " +
				std::to_string(idle_connections_.size()) + " idle, " +
				std::to_string(connections_.size() - idle_connections_.size()) + " active";
		}
		/**
		 * @brief 获取当前总连接数
		 * @return uint32_t 连接数
		 */
		uint32_t getTotalConnections() const {
			std::lock_guard<std::mutex> lock(mutex_);
			return connections_.size();
		}
		/**
		 * @brief 获取空闲连接数
		 * @return uint32_t 空闲连接数
		 */
		uint32_t getIdleConnections() const {
			std::lock_guard<std::mutex> lock(mutex_);
			return idle_connections_.size();
		}
		/**
		 * @brief 获取活跃连接数
		 * @return uint32_t 活跃连接数
		 */
		uint32_t getActiveConnections() const {
			std::lock_guard<std::mutex> lock(mutex_);
			return connections_.size() - idle_connections_.size();
		}
		/**
		 * @brief 检查连接池是否在运行
		 * @return bool 运行中返回 true，否则 false
		 */
		bool isRunning() const {
			std::lock_guard<std::mutex> lock(mutex_);
			return running_;
		}
		/**
		 * @brief 收缩连接池到最小连接数
		 */
		void shrink() {
			std::lock_guard<std::mutex> lock(mutex_);

			uint32_t target_size = config_.min_connections;
			if (connections_.size() <= target_size) {
				return;
			}

			uint32_t remove_count = connections_.size() - target_size;
			uint32_t removed = 0;

			std::queue<ClickHouseConnection::ptr> new_idle_queue;
			while (!idle_connections_.empty() && removed < remove_count) {
				auto conn = idle_connections_.front();
				idle_connections_.pop();

				conn->close();
				connections_.erase(conn);
				removed++;

				CRAZY_SYSTEM_DEBUG() << "Shrink: removed one idle ClickHouse connection";
			}

			while (!idle_connections_.empty()) {
				new_idle_queue.push(idle_connections_.front());
				idle_connections_.pop();
			}
			idle_connections_.swap(new_idle_queue);

			CRAZY_SYSTEM_INFO() << "ClickHouse connection pool shrunk, removed " << removed << " connections, current: " << connections_.size();
		}

	private:
		/**
		 * @brief 创建新的数据库连接
		 * @return ClickHouseConnection::ptr 新创建的连接
		 */
		ClickHouseConnection::ptr createConnection(bool push_to_idle = true) {
			try {
				auto conn = std::make_shared<ClickHouseConnection>();

				if (!conn->connect(config_.host, config_.port, config_.user,
					config_.password, config_.database)) {
					CRAZY_SYSTEM_ERROR() << "Failed to connect to ClickHouse";
					return nullptr;
				}
				connections_.insert(conn);
				if (push_to_idle) {
					idle_connections_.push(conn);
				}
				CRAZY_SYSTEM_DEBUG() << "New ClickHouse connection created successfully";
				return conn;

			}
			catch (const std::exception& e) {
				CRAZY_SYSTEM_ERROR() << "Exception while creating ClickHouse connection: " << clickhouse_detail::NormalizeExceptionMessage(e.what());
				return nullptr;
			}
		}

		/**
		 * @brief 创建托管连接（包装器，用于自动归还）
		 * @param conn 原始连接
		 * @return ClickHouseConnection::ptr 托管连接
		 */
		ClickHouseConnection::ptr createManagedConnection(ClickHouseConnection::ptr conn) {
			auto deleter = [this, conn](ClickHouseConnection*) mutable {
				returnConnection(std::move(conn));
				};
			return ClickHouseConnection::ptr(conn.get(), deleter);
		}
		/**
		 * @brief 归还连接到池中
		 * @param conn 要归还的连接
		 */
		void returnConnection(ClickHouseConnection::ptr conn) {
			std::lock_guard<std::mutex> lock(mutex_);

			if (!running_) {
				conn->close();
				return;
			}

			if (!isConnectionValid(conn)) {
				CRAZY_SYSTEM_WARN() << "Returned ClickHouse connection is invalid, removing it";
				removeConnection(conn);
				condition_.notify_one();
				return;
			}

			conn->updateLastUsedTime();

			if (connections_.size() > config_.max_connections) {
				CRAZY_SYSTEM_WARN() << "ClickHouse connection pool is full, closing returned connection";
				removeConnection(conn);
			}
			else {
				idle_connections_.push(conn);
				CRAZY_SYSTEM_DEBUG() << "ClickHouse connection returned to pool, idle: "
					<< idle_connections_.size();
				condition_.notify_one();
			}
		}
		/**
		 * @brief 检查连接是否有效
		 * @param conn 要检查的连接
		 * @return bool 有效返回 true，否则 false
		 */
		bool isConnectionValid(ClickHouseConnection::ptr conn) {
			try {
				return conn && conn->isConnected();
			}
			catch (const std::exception& e) {
				CRAZY_SYSTEM_ERROR() << "Exception while checking ClickHouse connection validity: " << clickhouse_detail::NormalizeExceptionMessage(e.what());
				return false;
			}
		}
		/**
		 * @brief 从连接池中移除连接
		 * @param conn 要移除的连接
		 */
		void removeConnection(ClickHouseConnection::ptr conn) {
			if (conn) {
				conn->close();
				connections_.erase(conn);
			}
		}
		/**
		 * @brief 连接池监控线程函数
		 */
		void monitor() {
			CRAZY_SYSTEM_INFO() << "ClickHouse connection pool monitor started";

			while (running_) {
				std::this_thread::sleep_for(std::chrono::seconds(1));

				if (!running_) break;

				try {
					checkConnections();
				}
				catch (const std::exception& e) {
					CRAZY_SYSTEM_ERROR() << "Exception in ClickHouse connection pool monitor: " << clickhouse_detail::NormalizeExceptionMessage(e.what());
				}
			}
			CRAZY_SYSTEM_INFO() << "ClickHouse connection pool monitor stopped";
		}
		/**
		 * @brief 检查并维护连接池中的连接
		 */
		void checkConnections() {
			std::lock_guard<std::mutex> lock(mutex_);

			if (!running_) return;

			std::queue<ClickHouseConnection::ptr> new_idle_queue;
			while (!idle_connections_.empty()) {
				auto conn = idle_connections_.front();
				idle_connections_.pop();

				auto idle_time = conn->getIdleTime();
				if (idle_time.count() > config_.max_idle_time) {
					removeConnection(conn);
					continue;
				}

				if (!isConnectionValid(conn)) {
					removeConnection(conn);

					if (connections_.size() < config_.min_connections) {
						createConnection();
					}
					continue;
				}

				new_idle_queue.push(conn);
			}
			idle_connections_.swap(new_idle_queue);
			while (connections_.size() < config_.min_connections) {
				if (!createConnection()) {
					break;
				}
			}
		}

	private:
		//! 连接池配置
		ClickHouseConnectionPoolConfig config_;
		//! 互斥锁
		mutable std::mutex mutex_;
		//! 条件变量
		std::condition_variable condition_;
		//! 运行状态
		std::atomic<bool> running_;
		//! 监控线程
		std::thread monitor_thread_;
		//! 所有连接集合
		std::unordered_set<ClickHouseConnection::ptr> connections_;
		//! 空闲连接队列
		std::queue<ClickHouseConnection::ptr> idle_connections_;
	};

} // namespace crazy
