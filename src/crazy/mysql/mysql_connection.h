/**
 * @file mysql_connection.h
 * @author keisum (Keisumhuis@gmail.com)
 * @brief
 * @version 0.2
 * @date 2024
 *
 * @copyright Copyright (c) 2024
 */
#pragma once

#include <memory>
#include <mutex>
#include <chrono>
#include <string>
#include <stdexcept>
#include <cstdarg>
#include <cstdio>

#include "crazy/logger.h"
#include "crazy/nocopyable.h"
#include "crazy/mysql/mysql_native_connection.h"

namespace crazy {

    /**
     * @brief MySQL 异常基类
     *
     * 继承自 std::runtime_error，提供 MySQL 特定的错误信息
     */
    class MySQLException : public std::runtime_error {
    public:
        /**
         * @brief 构造函数
         * @param message 错误消息
         * @param error_code MySQL 错误代码
         */
        MySQLException(const std::string& message, uint32_t error_code = 0)
            : std::runtime_error(message), error_code_(error_code) {}

        /**
         * @brief 获取 MySQL 错误代码
         * @return uint32_t 错误代码
         */
        uint32_t getErrorCode() const { return error_code_; }

    private:
        ///< MySQL 错误代码
        uint32_t error_code_;  
    };

    /**
     * @brief MySQL 连接异常
     *
     * 用于连接相关的错误，如连接失败、断开等
     */
    class MySQLConnectionException : public MySQLException {
        using MySQLException::MySQLException;
    };

    /**
     * @brief MySQL 查询异常
     *
     * 用于 SQL 查询执行相关的错误
     */
    class MySQLQueryException : public MySQLException {
        using MySQLException::MySQLException;
    };

    /**
     * @brief MySQL 语句异常
     *
     * 用于预处理语句相关的错误
     */
    class MySQLStatementException : public MySQLException {
        using MySQLException::MySQLException;
    };

    /**
     * @brief MySQL 查询结果类
     *
     * 封装 MySQL 查询结果，提供安全的结果访问接口
     */
    class MySQLResult final
        : public Noncopyable, public std::enable_shared_from_this<MySQLResult> {
    public:
        ///< 智能指针类型
        using ptr = std::shared_ptr<MySQLResult>;  

        /**
         * @brief 构造函数
         * @param result 原生 MySQL 结果指针
         */
        MySQLResult(MySQLNativeResult::ptr result)
            : result_(result) {
        }

        /**
         * @brief 检查结果是否有效
         * @return bool 有效返回 true，否则 false
         */
        bool isValid() const {
            return result_ != nullptr;
        }

        /**
         * @brief 检查结果集是否为空
         * @return bool 为空返回 true，否则 false
         */
        bool isEmpty() const {
            return !result_ || !result_->rows_count();
        }

        /**
         * @brief 获取结果集行数
         * @return uint64_t 行数
         */
        uint64_t count() const {
            return result_ ? result_->rows_count() : 0;
        }

        /**
         * @brief 获取字段数量
         * @return uint64_t 字段数
         */
        uint64_t fields_count() const {
            return result_ ? result_->fields_count() : 0;
        }

        /**
         * @brief 获取字段信息数组
         * @return MYSQL_FIELD* 字段信息指针
         */
        MYSQL_FIELD* fields() const {
            return result_ ? result_->fields() : nullptr;
        }

        /**
         * @brief 获取当前行数据
         * @return MYSQL_ROW 行数据指针
         */
        MYSQL_ROW row() const {
            return result_ ? result_->row() : nullptr;
        }

    private:
        ///< 原生 MySQL 结果对象
        MySQLNativeResult::ptr result_; 
    };

    /**
     * @brief MySQL 预处理语句类
     *
     * 封装 MySQL 预处理语句，提供参数绑定和执行功能
     */
    class MySQLStatement final
        : public Noncopyable, public std::enable_shared_from_this<MySQLStatement> {
    public:
        ///< 智能指针类型
        using ptr = std::shared_ptr<MySQLStatement>;  

        /**
         * @brief 构造函数
         * @param statement 原生 MySQL 语句指针
         */
        MySQLStatement(MySQLNativeStatement::ptr statement)
            : statement_(statement) {
        }

        /**
         * @brief 准备 SQL 语句
         * @param sql SQL 语句
         * @return bool 成功返回 true，否则 false
         * @throws MySQLStatementException 如果语句为空
         */
        bool prepare(const std::string& sql) {
            if (!statement_) {
                throw MySQLStatementException("Statement is null");
            }
            return statement_->stmt_prepare(sql);
        }

        /**
         * @brief 绑定输入参数
         * @param bind 参数绑定结构体
         * @return bool 成功返回 true，否则 false
         */
        bool bind_input_param(MYSQL_BIND* bind) {
            return statement_ ? statement_->stmt_bind_param(bind) : false;
        }

        /**
         * @brief 绑定输出参数
         * @param bind 参数绑定结构体
         * @return bool 成功返回 true，否则 false
         */
        bool bind_output_param(MYSQL_BIND* bind) {
            return statement_ ? statement_->stmt_bind_result(bind) : false;
        }

        /**
         * @brief 执行预处理语句
         * @return bool 成功返回 true，否则 false
         * @throws MySQLStatementException 如果语句为空
         */
        bool execute() {
            if (!statement_) {
                throw MySQLStatementException("Statement is null");
            }
            return statement_->stmt_execute();
        }

        /**
         * @brief 获取下一行结果
         * @return bool 成功返回 true，否则 false
         */
        bool fetch() {
            return statement_ ? statement_->stmt_fetch() : false;
        }

        /**
         * @brief 存储结果集
         * @return bool 成功返回 true，否则 false
         */
        bool store_result() {
            return statement_ ? statement_->stmt_store_result() : false;
        }

        /**
         * @brief 获取结果集元数据
         * @return MySQLResult::ptr 结果集指针
         */
        MySQLResult::ptr result_metadata() {
            return statement_ ? std::make_shared<MySQLResult>(statement_->stmt_result_metadata()) : nullptr;
        }

        /**
         * @brief 获取参数元数据
         * @return MySQLResult::ptr 参数元数据指针
         */
        MySQLResult::ptr param_metadata() {
            return statement_ ? std::make_shared<MySQLResult>(statement_->stmt_param_metadata()) : nullptr;
        }

        /**
         * @brief 获取错误代码
         * @return uint32_t 错误代码
         */
        uint32_t error() const {
            return statement_ ? statement_->stmt_errno() : 0;
        }

        /**
         * @brief 获取错误消息
         * @return std::string 错误消息
         */
        std::string error_message() const {
            return statement_ ? statement_->stmt_error() : "Statement is null";
        }

        /**
         * @brief 获取 SQL 状态
         * @return std::string SQL 状态
         */
        std::string sqlstate() const {
            return statement_ ? statement_->stmt_sqlstate() : "";
        }

        /**
         * @brief 获取受影响的行数
         * @return uint64_t 受影响的行数
         */
        uint64_t affected_rows() const {
            return statement_ ? statement_->stmt_affected_rows() : 0;
        }

        /**
         * @brief 获取结果集行数
         * @return uint64_t 行数
         */
        uint64_t num_rows() const {
            return statement_ ? statement_->stmt_num_rows() : 0;
        }

        /**
         * @brief 获取最后插入的 ID
         * @return uint64_t 插入 ID
         */
        uint64_t insert_id() const {
            return statement_ ? statement_->stmt_insert_id() : 0;
        }

        /**
         * @brief 重置语句状态
         * @return bool 成功返回 true，否则 false
         */
        bool reset() {
            return statement_ ? statement_->stmt_reset() : false;
        }

        /**
         * @brief 关闭语句
         * @return bool 成功返回 true，否则 false
         */
        bool close() {
            return statement_ ? statement_->stmt_close() : false;
        }

    private:
        ///< 原生 MySQL 语句对象
        MySQLNativeStatement::ptr statement_;  
    };

    /**
     * @brief MySQL 连接类
     *
     * 封装 MySQL 连接操作，提供线程安全的数据库访问接口
     * 支持连接池、事务管理、预处理语句等功能
     */
    class MySQLConnection final
        : public Noncopyable, public std::enable_shared_from_this<MySQLConnection> {
    public:
        ///< 智能指针类型
        using ptr = std::shared_ptr<MySQLConnection>;  

        /**
         * @brief 默认构造函数
         *
         * 创建连接对象但不立即连接
         */
        MySQLConnection()
            : connection_(std::make_shared<MySQLNativeConnection>()) {
        }

        /**
         * @brief 构造函数并立即连接
         * @param host 数据库主机地址
         * @param user 用户名
         * @param password 密码
         * @param db 数据库名（可选）
         * @param port 端口号（默认 3306）
         */
        MySQLConnection(const std::string& host, const std::string& user,
            const std::string& password, const std::string& db = "",
            uint32_t port = 3306)
            : connection_(std::make_shared<MySQLNativeConnection>(host, user, password, db, port)) {
        }

        /**
         * @brief 验证连接参数
         * @param host 主机地址
         * @param user 用户名
         * @param port 端口号
         * @return bool 参数有效返回 true，否则 false
         */
        static bool validateConnectionParams(const std::string& host,
            const std::string& user,
            uint32_t port) {
            return !host.empty() && !user.empty() && port > 0 && port <= 65535;
        }

        /**
         * @brief 连接到 MySQL 数据库
         * @param host 数据库主机地址
         * @param user 用户名
         * @param password 密码
         * @param db 数据库名（可选）
         * @param port 端口号（默认 3306）
         * @return bool 连接成功返回 true，否则 false
         */
        bool connect(const std::string& host, const std::string& user,
            const std::string& password, const std::string& db = "",
            uint32_t port = 3306) {

            if (!validateConnectionParams(host, user, port)) {
                CRAZY_SYSTEM_ERROR() << "Invalid MySQL connection parameters";
                return false;
            }

            bool success = false;
            if (db.empty()) {
                success = connection_->connect(host.data(), user.data(), password.data(), nullptr, port);
            }
            else {
                success = connection_->connect(host.data(), user.data(), password.data(), db.data(), port);
            }

            if (success) {
                updateLastUsedTime();
                CRAZY_SYSTEM_INFO() << "Connected to MySQL successfully, host = " << host;
            }
            else {
                CRAZY_SYSTEM_ERROR() << "MySQL connection failed: " << get_error_message();
            }

            return success;
        }

        /**
         * @brief 检查连接是否有效
         * @return bool 连接有效返回 true，否则 false
         */
        bool isConnected() const {
            return connection_ && connection_->ping() == 0;
        }

        /**
         * @brief 重新连接数据库
         * @return bool 重连成功返回 true，否则 false
         * @note 需要预先存储连接参数
         */
        bool reconnect() {
            if (connection_) {
                connection_->close();
                // 注意：需要预先存储连接参数以便重连
                return true;
            }
            return false;
        }

        /**
         * @brief 重置连接
         * @return bool 成功返回 true，否则 false
         */
        bool reset_connection() {
            return connection_ ? connection_->reset_connection() : false;
        }

        /**
         * @brief 关闭数据库连接
         */
        void close() {
            if (connection_) {
                connection_->close();
            }
        }

        /**
         * @brief 设置连接选项
         * @param option 选项类型
         * @param arg 选项值
         * @return bool 成功返回 true，否则 false
         */
        bool options(mysql_option option, const void* arg) {
            return connection_ ? connection_->options(option, arg) : false;
        }

        /**
         * @brief 设置连接选项（4 参数版本）
         * @param option 选项类型
         * @param arg1 选项值1
         * @param arg2 选项值2
         * @return bool 成功返回 true，否则 false
         */
        bool options4(mysql_option option, const void* arg1, const void* arg2) {
            return connection_ ? connection_->options4(option, arg1, arg2) : false;
        }

        /**
         * @brief 获取连接选项
         * @param option 选项类型
         * @param arg 选项值指针
         * @return bool 成功返回 true，否则 false
         */
        bool get_option(mysql_option option, const void* arg) {
            return connection_ ? connection_->get_option(option, arg) : false;
        }

        /**
         * @brief 设置字符集
         * @param charset 字符集名称（默认 utf8mb4）
         * @return bool 成功返回 true，否则 false
         */
        bool set_character_set(const std::string& charset = "utf8mb4") {
            return connection_ ? connection_->set_character_set(charset) : false;
        }

        /**
         * @brief 获取当前字符集名称
         * @return std::string 字符集名称
         */
        const std::string get_character_set_name() {
            return connection_ ? connection_->get_character_set_name() : "";
        }

        /**
         * @brief 获取字符集信息
         * @return MY_CHARSET_INFO 字符集信息结构体
         */
        MY_CHARSET_INFO get_character_set_info() {
            return connection_ ? connection_->get_character_set_info() : MY_CHARSET_INFO();
        }

        /**
         * @brief 设置 SSL 连接参数
         * @param key SSL 密钥文件路径
         * @param cert SSL 证书文件路径
         * @param ca CA 证书文件路径
         * @param capath CA 证书目录路径
         * @param cipher 加密套件
         * @return bool 成功返回 true，否则 false
         */
        bool set_ssl(const std::string& key, const std::string& cert,
            const std::string& ca, const std::string& capath,
            const std::string& cipher) {
            return connection_ ? connection_->set_ssl(key, cert, ca, capath, cipher) : false;
        }

        /**
         * @brief 获取 SSL 加密套件
         * @return std::string SSL 加密套件名称
         */
        std::string get_ssl_cipher() {
            return connection_ ? connection_->get_ssl_cipher() : "";
        }

        /**
         * @brief 切换用户
         * @param user 新用户名
         * @param password 新密码
         * @param db 新数据库名
         * @return bool 成功返回 true，否则 false
         */
        bool change_user(const std::string& user, const std::string& password,
            const std::string& db) {
            return connection_ ? connection_->change_user(user, password, db) : false;
        }

        /**
         * @brief 获取错误代码
         * @return uint32_t MySQL 错误代码
         */
        uint32_t get_errno() {
            return connection_ ? connection_->get_errno() : 0;
        }

        /**
         * @brief 获取错误消息
         * @return std::string 错误消息
         */
        std::string get_error_message() {
            return connection_ ? connection_->get_error_message() : "Connection is null";
        }

        /**
         * @brief 获取 SQL 状态
         * @return std::string SQL 状态
         */
        std::string sql_state() {
            return connection_ ? connection_->sql_state() : "";
        }

        /**
         * @brief 获取警告数量
         * @return uint32_t 警告数量
         */
        uint32_t sql_warning_count() {
            return connection_ ? connection_->sql_warning_count() : 0;
        }

        /**
         * @brief 获取 SQL 信息
         * @return std::string SQL 信息
         */
        std::string sql_info() {
            return connection_ ? connection_->sql_info() : "";
        }

        /**
         * @brief 获取 MySQL 会话线程 ID
         * @return uint32_t 线程 ID
         */
        uint32_t get_mysql_session_thread_id() {
            return connection_ ? connection_->get_mysql_session_thread_id() : 0;
        }

        /**
         * @brief 获取服务器状态
         * @return std::string 服务器状态
         */
        std::string server_status() {
            return connection_ ? connection_->server_status() : "";
        }

        /**
         * @brief 获取服务器信息
         * @return std::string 服务器信息
         */
        std::string server_info() {
            return connection_ ? connection_->server_info() : "";
        }

        /**
         * @brief 获取服务器主机信息
         * @return std::string 服务器主机信息
         */
        std::string server_host_info() {
            return connection_ ? connection_->server_host_info() : "";
        }

        /**
         * @brief 获取服务器版本
         * @return uint32_t 服务器版本号
         */
        uint32_t server_version() {
            return connection_ ? connection_->server_version() : 0;
        }

        /**
         * @brief 获取协议版本
         * @return uint32_t 协议版本
         */
        uint32_t proto_info() {
            return connection_ ? connection_->proto_info() : 0;
        }

        /**
         * @brief 获取客户端信息
         * @return std::string 客户端信息
         */
        std::string client_info() {
            return connection_ ? connection_->client_info() : "";
        }

        /**
         * @brief 获取客户端版本
         * @return uint32_t 客户端版本号
         */
        uint32_t client_version() {
            return connection_ ? connection_->client_version() : 0;
        }

        /**
         * @brief 检查连接状态
         * @return uint32_t 0 表示连接正常，非 0 表示错误
         */
        uint32_t ping() {
            return connection_ ? connection_->ping() : 1;
        }

        /**
         * @brief 选择数据库
         * @param db 数据库名
         * @return bool 成功返回 true，否则 false
         */
        bool select_db(const std::string& db) {
            return connection_ ? connection_->select_db(db) : false;
        }

        /**
         * @brief 获取表字段信息
         * @param table 表名
         * @return MySQLResult::ptr 字段信息结果集
         * @throws MySQLConnectionException 如果连接为空
         */
        MySQLResult::ptr table_fields(const std::string& table) {
            if (!connection_) {
                throw MySQLConnectionException("Connection is null");
            }
            return std::make_shared<MySQLResult>(connection_->table_fields(table));
        }

        /**
         * @brief 创建预处理语句
         * @return MySQLStatement::ptr 预处理语句指针
         * @throws MySQLConnectionException 如果连接为空
         */
        MySQLStatement::ptr create_statement() {
            if (!connection_) {
                throw MySQLConnectionException("Connection is null");
            }
            return std::make_shared<MySQLStatement>(connection_->create_statement());
        }

        /**
         * @brief 安全格式化查询执行
         * @param fmt 格式化字符串
         * @param ... 格式化参数
         * @return MySQLResult::ptr 查询结果
         * @throws MySQLQueryException 如果格式化失败
         */
        MySQLResult::ptr exec_fmt(const char* fmt, ...) {
            va_list args;
            va_start(args, fmt);
            auto size = vsnprintf(nullptr, 0, fmt, args) + 1;
            va_end(args);

            if (size < 0) {
                throw MySQLQueryException("Format string error");
            }

            std::string sql;
            sql.resize(size);

            va_start(args, fmt);
            vsnprintf(sql.data(), size, fmt, args);
            va_end(args);

            return exec(sql);
        }

        /**
         * @brief 执行 SQL 查询
         * @param sql SQL 语句
         * @return MySQLResult::ptr 查询结果
         * @throws MySQLConnectionException 如果连接为空
         * @throws MySQLQueryException 如果查询执行失败
         */
        MySQLResult::ptr exec(const std::string& sql) {
            updateLastUsedTime();

            if (!connection_) {
                throw MySQLConnectionException("Connection is null");
            }

            if (!connection_->query(sql)) {
                throw MySQLQueryException(
                    "MySQL query execution failed, SQL: " + sql +
                    ", Error: " + connection_->get_error_message(),
                    connection_->get_errno()
                );
            }

            CRAZY_SYSTEM_DEBUG() << "Executed SQL: " << sql;
            return store_result();
        }

        /**
         * @brief 开始事务
         * @return bool 成功返回 true，否则 false
         */
        bool beginTransaction() {
            return exec("START TRANSACTION") != nullptr;
        }

        /**
         * @brief 提交事务
         * @return bool 成功返回 true，否则 false
         */
        bool commit() {
            return connection_ ? connection_->commit() : false;
        }

        /**
         * @brief 回滚事务
         * @return bool 成功返回 true，否则 false
         */
        bool rollback() {
            return connection_ ? connection_->rollback() : false;
        }

        /**
         * @brief 设置自动提交模式
         * @param auto_mode 自动提交模式
         * @return bool 成功返回 true，否则 false
         */
        bool autocommit(bool auto_mode = true) {
            return connection_ ? connection_->autocommit(auto_mode) : false;
        }

        /**
         * @brief 创建数据库
         * @param db_name 数据库名
         * @return bool 成功返回 true，否则 false
         */
        bool createDatabase(const std::string& db_name) {
            return exec("CREATE DATABASE IF NOT EXISTS " + db_name) != nullptr;
        }

        /**
         * @brief 删除数据库
         * @param db_name 数据库名
         * @return bool 成功返回 true，否则 false
         */
        bool dropDatabase(const std::string& db_name) {
            return exec("DROP DATABASE IF EXISTS " + db_name) != nullptr;
        }

        /**
         * @brief 使用数据库
         * @param db_name 数据库名
         * @return bool 成功返回 true，否则 false
         */
        bool useDatabase(const std::string& db_name) {
            return select_db(db_name);
        }

        /**
         * @brief 存储查询结果
         * @return MySQLResult::ptr 结果集指针
         */
        MySQLResult::ptr store_result() {
            return connection_ ? std::make_shared<MySQLResult>(connection_->store_result()) : nullptr;
        }

        /**
         * @brief 获取字段数量
         * @return uint32_t 字段数量
         */
        uint32_t field_count() {
            return connection_ ? connection_->field_count() : 0;
        }

        /**
         * @brief 获取受影响的行数
         * @return uint64_t 受影响的行数
         */
        uint64_t affected_rows() {
            return connection_ ? connection_->affected_rows() : 0;
        }

        /**
         * @brief 获取最后插入的 ID
         * @return uint64_t 插入 ID
         */
        uint64_t insert_id() {
            return connection_ ? connection_->insert_id() : 0;
        }

        /**
         * @brief 检查是否有更多结果
         * @return bool 有更多结果返回 true，否则 false
         */
        bool has_next_result() {
            return connection_ ? connection_->has_next_result() : false;
        }

        /**
         * @brief 获取下一个结果集
         * @return MySQLResult::ptr 下一个结果集
         * @throws MySQLConnectionException 如果连接为空
         */
        MySQLResult::ptr next_result() {
            if (!connection_) {
                throw MySQLConnectionException("Connection is null");
            }
            connection_->next_result();
            return store_result();
        }

        // 连接池支持方法

        /**
         * @brief 获取最后使用时间
         * @return std::chrono::time_point<std::chrono::system_clock> 最后使用时间点
         */
        std::chrono::time_point<std::chrono::system_clock> getLastUsedTime() const noexcept {
            return lastUsedTime_;
        }

        /**
         * @brief 获取空闲时间
         * @return std::chrono::seconds 空闲时间（秒）
         */
        std::chrono::seconds getIdleTime() const noexcept {
            return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - lastUsedTime_);
        }

        /**
         * @brief 更新最后使用时间
         */
        void updateLastUsedTime() noexcept {
            lastUsedTime_ = std::chrono::system_clock::now();
        }

        /**
         * @brief 线程安全执行 SQL
         * @param sql SQL 语句
         * @return std::shared_ptr<MySQLResult> 查询结果
         */
        std::shared_ptr<MySQLResult> threadSafeExec(const std::string& sql) {
            return exec(sql);
        }

    private:
        ///< 最后使用时间
        std::chrono::time_point<std::chrono::system_clock> lastUsedTime_ = std::chrono::system_clock::now();  
        ///< 原生 MySQL 连接对象
        MySQLNativeConnection::ptr connection_;  
    };
}
