/**
 * @file mysql_connection_pool.h
 * @author keisum (Keisumhuis@gmail.com)
 * @brief
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

#include "crazy/mysql/mysql_connection.h"
#include "crazy/logger.h"

#ifdef max
#undef max
#endif //! max

namespace crazy {

	/**
	 * @brief MySQL 连接池配置结构体
	 */
	struct MySQLConnectionPoolConfig {
		///< 数据库主机地址
		std::string host;
		///< 用户名
		std::string user;
		///< 密码
		std::string password;
		///< 数据库名
		std::string database;
		///< 端口号，默认 3306
		uint32_t port = 3306;
		///< 字符集，默认 utf8mb4
		std::string charset = "utf8mb4";
		///< 最小连接数，默认 5
		uint32_t min_connections = 5;
		///< 最大连接数，默认 20
		uint32_t max_connections = 20;
		///< 最大空闲时间（秒），默认 300
		uint32_t max_idle_time = 300;
		///< 最大等待时间（秒），默认 30
		uint32_t max_wait_time = 30;
		///< 连接超时时间（秒），默认 10
		uint32_t connection_timeout = 10;

		/**
		 * @brief 验证配置有效性
		 * @return bool 配置有效返回 true，否则 false
		 */
		bool isValid() const {
			return !host.empty() && !user.empty() &&
				min_connections > 0 && max_connections >= min_connections &&
				port > 0 && port <= 65535;
		}
	};

	/**
	 * @brief MySQL 连接池异常类
	 */
	class MySQLConnectionPoolException : public MySQLException {
		using MySQLException::MySQLException;
	};

	/**
	 * @brief MySQL 连接池类
	 *
	 * 提供连接的获取、归还、健康检查等功能
	 * 支持连接泄漏检测和自动重连
	 */
	class MySQLConnectionPool final : public Noncopyable {
	public:
		///< 智能指针类型
		using ptr = std::shared_ptr<MySQLConnectionPool>;  

		/**
		 * @brief 构造函数
		 * @param config 连接池配置
		 */
		explicit MySQLConnectionPool(const MySQLConnectionPoolConfig& config)
			: config_(config)
			, running_(false) {
			if (!config_.isValid()) {
				throw MySQLConnectionPoolException("Invalid MySQL connection pool configuration");
			}
		}

		/**
		 * @brief 析构函数
		 */
		~MySQLConnectionPool() {
			stop();
		}

		/**
		 * @brief 启动连接池
		 * @return bool 启动成功返回 true，否则 false
		 */
		bool start() {
			std::lock_guard<std::mutex> lock(mutex_);

			if (running_) {
				CRAZY_SYSTEM_WARN() << "MySQL connection pool is already running";
				return true;
			}

			running_ = true;

			// 创建最小连接数的连接
			for (uint32_t i = 0; i < config_.min_connections; ++i) {
				if (!createConnection()) {
					CRAZY_SYSTEM_ERROR() << "Failed to create initial connection " << i;
					// 继续创建其他连接，不立即失败
				}
			}

			if (connections_.size() < config_.min_connections) {
				CRAZY_SYSTEM_WARN() << "Created only " << connections_.size()
					<< " connections, less than min_connections "
					<< config_.min_connections;
			}

			// 启动监控线程
			monitor_thread_ = std::thread(&MySQLConnectionPool::monitor, this);

			CRAZY_SYSTEM_INFO() << "MySQL connection pool started successfully, initial connections: "
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

			// 关闭所有连接
			std::lock_guard<std::mutex> lock(mutex_);
			while (!idle_connections_.empty()) {
				auto conn = idle_connections_.front();
				idle_connections_.pop();
				conn->close();
			}
			connections_.clear();

			CRAZY_SYSTEM_INFO() << "MySQL connection pool stopped";
		}

		/**
		 * @brief 获取数据库连接
		 * @param timeout_ms 超时时间（毫秒），0 表示无限等待
		 * @return MySQLConnection::ptr 数据库连接指针
		 * @throws MySQLConnectionPoolException 如果获取连接超时或失败
		 */
		MySQLConnection::ptr getConnection(uint32_t timeout_ms = 0) {
			std::unique_lock<std::mutex> lock(mutex_);

			auto wait_until = timeout_ms > 0 ?
				std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms) :
				std::chrono::steady_clock::time_point::max();

			while (running_) {
				// 尝试从空闲队列获取连接
				if (!idle_connections_.empty()) {
					auto conn = idle_connections_.front();
					idle_connections_.pop();

					// 检查连接是否有效
					if (isConnectionValid(conn)) {
						CRAZY_SYSTEM_DEBUG() << "Get connection from pool, idle: "
							<< idle_connections_.size();
						return createManagedConnection(conn);
					}
					else {
						CRAZY_SYSTEM_WARN() << "Removing invalid connection from pool";
						removeConnection(conn);
					}
				}

				// 等待连接可用
				if (timeout_ms == 0) {
					condition_.wait(lock);
				}
				else {
					if (condition_.wait_until(lock, wait_until) == std::cv_status::timeout) {
						throw MySQLConnectionPoolException(
							"Get connection timeout after " + std::to_string(timeout_ms) + "ms, " +
							"pool stats: total=" + std::to_string(connections_.size()) +
							", idle=" + std::to_string(idle_connections_.size()));
					}
				}
			}

			throw MySQLConnectionPoolException("Connection pool is not running");
		}

		/**
		 * @brief 获取连接池统计信息
		 * @return std::string 统计信息字符串
		 */
		std::string getStats() const {
			std::lock_guard<std::mutex> lock(mutex_);
			return "MySQL Connection Pool Stats: " +
				std::to_string(connections_.size()) + " total, " +
				std::to_string(idle_connections_.size()) + " idle, " +
				std::to_string(connections_.size() - idle_connections_.size()) + " active";
		}

		/**
		 * @brief 获取当前连接数
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

			// 从空闲连接中移除超出的连接
			std::queue<MySQLConnection::ptr> new_idle_queue;
			while (!idle_connections_.empty() && removed < remove_count) {
				auto conn = idle_connections_.front();
				idle_connections_.pop();

				conn->close();
				connections_.erase(conn);
				removed++;

				CRAZY_SYSTEM_DEBUG() << "Shrink: removed one idle connection";
			}

			// 将剩余的空闲连接放回队列
			while (!idle_connections_.empty()) {
				new_idle_queue.push(idle_connections_.front());
				idle_connections_.pop();
			}
			idle_connections_.swap(new_idle_queue);

			CRAZY_SYSTEM_INFO() << "Connection pool shrunk, removed " << removed
				<< " connections, current: " << connections_.size();
		}

	private:
		/**
		 * @brief 创建新的数据库连接
		 * @return MySQLConnection::ptr 新创建的连接
		 */
		MySQLConnection::ptr createConnection() {
			try {
				auto conn = std::make_shared<MySQLConnection>();

				if (!conn->connect(config_.host, config_.user, config_.password,
					config_.database, config_.port)) {
					CRAZY_SYSTEM_ERROR() << "Failed to connect to MySQL: "
						<< conn->get_error_message();
					return nullptr;
				}

				if (!config_.charset.empty() && !conn->set_character_set(config_.charset)) {
					CRAZY_SYSTEM_WARN() << "Failed to set charset: " << config_.charset;
				}

				connections_.insert(conn);
				idle_connections_.push(conn);
				CRAZY_SYSTEM_DEBUG() << "New connection created successfully";
				return conn;

			}
			catch (const std::exception& e) {
				CRAZY_SYSTEM_ERROR() << "Exception while creating connection: " << e.what();
				return nullptr;
			}
		}

		/**
		 * @brief 创建托管连接（包装器，用于自动归还）
		 * @param conn 原始连接
		 * @return MySQLConnection::ptr 托管连接
		 */
		MySQLConnection::ptr createManagedConnection(MySQLConnection::ptr conn) {
			// 使用自定义删除器，在连接被销毁时自动归还到连接池
			auto deleter = [this, raw_conn = conn.get()](MySQLConnection*) {
				returnConnection(raw_conn->shared_from_this());
				};

			return MySQLConnection::ptr(conn.get(), deleter);
		}

		/**
		 * @brief 归还连接到池中
		 * @param conn 要归还的连接
		 */
		void returnConnection(MySQLConnection::ptr conn) {
			std::lock_guard<std::mutex> lock(mutex_);

			if (!running_) {
				conn->close();
				return;
			}

			// 检查连接是否仍然有效
			if (!isConnectionValid(conn)) {
				CRAZY_SYSTEM_WARN() << "Returned connection is invalid, removing it";
				removeConnection(conn);
				condition_.notify_one(); // 通知等待的线程可以尝试创建新连接
				return;
			}

			// 更新最后使用时间
			conn->updateLastUsedTime();

			// 如果连接池已满，关闭连接而不是放回池中
			if (connections_.size() > config_.max_connections) {
				CRAZY_SYSTEM_WARN() << "Connection pool is full, closing returned connection";
				removeConnection(conn);
			}
			else {
				idle_connections_.push(conn);
				CRAZY_SYSTEM_DEBUG() << "Connection returned to pool, idle: "
					<< idle_connections_.size();
				condition_.notify_one(); // 通知等待的线程有可用连接
			}
		}

		/**
		 * @brief 检查连接是否有效
		 * @param conn 要检查的连接
		 * @return bool 有效返回 true，否则 false
		 */
		bool isConnectionValid(MySQLConnection::ptr conn) {
			try {
				return conn && conn->isConnected();
			}
			catch (const std::exception& e) {
				CRAZY_SYSTEM_ERROR() << "Exception while checking connection validity: " << e.what();
				return false;
			}
		}

		/**
		 * @brief 从连接池中移除连接
		 * @param conn 要移除的连接
		 */
		void removeConnection(MySQLConnection::ptr conn) {
			if (conn) {
				conn->close();
				connections_.erase(conn);
			}
		}

		/**
		 * @brief 连接池监控线程函数
		 */
		void monitor() {
			CRAZY_SYSTEM_INFO() << "MySQL connection pool monitor started";

			while (running_) {
				std::this_thread::sleep_for(std::chrono::seconds(1));

				if (!running_) break;

				try {
					checkConnections();
				}
				catch (const std::exception& e) {
					CRAZY_SYSTEM_ERROR() << "Exception in connection pool monitor: " << e.what();
				}
			}

			CRAZY_SYSTEM_INFO() << "MySQL connection pool monitor stopped";
		}

		/**
		 * @brief 检查并维护连接池中的连接
		 */
		void checkConnections() {
			std::lock_guard<std::mutex> lock(mutex_);

			if (!running_) return;

			uint32_t checked = 0;
			uint32_t removed = 0;
			uint32_t recreated = 0;

			// 检查空闲连接的超时和有效性
			std::queue<MySQLConnection::ptr> new_idle_queue;
			while (!idle_connections_.empty()) {
				auto conn = idle_connections_.front();
				idle_connections_.pop();
				checked++;

				// 检查连接是否超时
				auto idle_time = conn->getIdleTime();
				if (idle_time.count() > config_.max_idle_time) {
					removeConnection(conn);
					removed++;
					continue;
				}

				// 检查连接是否仍然有效
				if (!isConnectionValid(conn)) {
					removeConnection(conn);
					removed++;

					// 如果连接数低于最小值，尝试重新创建
					if (connections_.size() < config_.min_connections) {
						if (createConnection()) {
							recreated++;
						}
					}
					continue;
				}

				new_idle_queue.push(conn);
			}
			idle_connections_.swap(new_idle_queue);

			// 确保连接数不低于最小值
			while (connections_.size() < config_.min_connections) {
				if (createConnection()) {
					recreated++;
				}
				else {
					break; // 创建失败，下次再试
				}
			}

			if (checked > 0) {
			}
		}

	private:
		///< 连接池配置
		MySQLConnectionPoolConfig config_;                          
		///< 互斥锁
		mutable std::mutex mutex_;                                  
		///< 条件变量
		std::condition_variable condition_;                         
		///< 运行状态
		std::atomic<bool> running_;                                 
		///< 监控线程
		std::thread monitor_thread_;                                
		///< 所有连接集合
		std::unordered_set<MySQLConnection::ptr> connections_;      
		///< 空闲连接队列
		std::queue<MySQLConnection::ptr> idle_connections_;         
	};
} // namespace crazy
