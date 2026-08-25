/**
 * @file clickhouse_connection.h
 * @author keisum (Keisumhuis@gmail.com)
 * @brief ClickHouse 连接封装，参考 MySQL 连接模式实现
 * @version 0.1
 * @date 2024
 *
 * @copyright Copyright (c) 2024
 */
#pragma once

#include <memory>
#include <mutex>
#include <chrono>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <initializer_list>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "clickhouse/client.h"
#include "crazy/logger.h"
#include "crazy/nocopyable.h"

namespace crazy {
	namespace clickhouse_detail {
		/**
		 * @brief 检查字符串是否为合法 UTF-8.
		 * @param value 字符串
		 * @return bool 合法返回 true，否则返回 false
		 */
		inline bool IsValidUtf8(const std::string& value) {
			size_t i = 0;
			while (i < value.size()) {
				const auto c = static_cast<unsigned char>(value[i]);
				size_t remaining = 0;
				if (c <= 0x7F) {
					++i;
					continue;
				}
				if ((c & 0xE0) == 0xC0) {
					remaining = 1;
					if (c < 0xC2) {
						return false;
					}
				}
				else if ((c & 0xF0) == 0xE0) {
					remaining = 2;
				}
				else if ((c & 0xF8) == 0xF0) {
					remaining = 3;
					if (c > 0xF4) {
						return false;
					}
				}
				else {
					return false;
				}
				if (i + remaining >= value.size()) {
					return false;
				}
				for (size_t j = 1; j <= remaining; ++j) {
					if ((static_cast<unsigned char>(value[i + j]) & 0xC0) != 0x80) {
						return false;
					}
				}
				i += remaining + 1;
			}
			return true;
		}

#ifdef _WIN32
		/**
		 * @brief 将指定 Windows 代码页的字符串转换为 UTF-8.
		 * @param codePage Windows 代码页
		 * @param value 字符串
		 * @return std::string UTF-8 字符串
		 */
		inline std::string MultiByteToUtf8(UINT codePage, const std::string& value) {
			if (value.empty()) {
				return {};
			}
			int wideSize = MultiByteToWideChar(codePage, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
			if (wideSize <= 0) {
				return {};
			}
			std::wstring wide(static_cast<size_t>(wideSize), L'\0');
			MultiByteToWideChar(codePage, 0, value.data(), static_cast<int>(value.size()), wide.data(), wideSize);

			int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideSize, nullptr, 0, nullptr, nullptr);
			if (utf8Size <= 0) {
				return {};
			}
			std::string result(static_cast<size_t>(utf8Size), '\0');
			WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideSize, result.data(), utf8Size, nullptr, nullptr);
			return result;
		}

		/**
		 * @brief 确保 Windows 控制台使用 UTF-8 输出.
		 */
		inline void EnsureUtf8ConsoleOutput() {
			static const bool initialized = []() {
				SetConsoleOutputCP(CP_UTF8);
				return true;
				}();
			(void)initialized;
		}
#endif

		/**
		 * @brief 规范化异常消息编码.
		 * @param message 异常消息
		 * @return std::string UTF-8 异常消息
		 */
		inline std::string NormalizeExceptionMessage(const char* message) {
			if (!message) {
				return {};
			}
			std::string value(message);
#ifdef _WIN32
			EnsureUtf8ConsoleOutput();
			if (IsValidUtf8(value)) {
				return value;
			}
			auto converted = MultiByteToUtf8(CP_ACP, value);
			return converted.empty() ? value : converted;
#else
			return value;
#endif
		}
	}

	/**
	 * @brief ClickHouse 异常基类
	 *
	 * 继承自 std::runtime_error，提供 ClickHouse 特定的错误信息
	 */
	class ClickHouseException : public std::runtime_error {
	public:
		ClickHouseException(const std::string& message, int32_t error_code = 0)
			: std::runtime_error(clickhouse_detail::NormalizeExceptionMessage(message.c_str())), error_code_(error_code) {}

		int32_t getErrorCode() const { return error_code_; }

	private:
		//! 错误码
		int32_t error_code_;
	};

	/**
	 * @brief ClickHouse 连接异常
	 *
	 * 用于连接相关的错误，如连接失败、断开等
	 */
	class ClickHouseConnectionException : public ClickHouseException {
		using ClickHouseException::ClickHouseException;
	};

	/**
	 * @brief ClickHouse 查询异常
	 *
	 * 用于 SQL 查询执行相关的错误
	 */
	class ClickHouseQueryException : public ClickHouseException {
		using ClickHouseException::ClickHouseException;
	};
	/**
	 * @brief ClickHouse 行数据.
	 */
	class ClickHouseRow : public std::vector<std::string> {
	public:
		using std::vector<std::string>::vector;
		/**
		 * @brief 构造行数据.
		 * @param values 字段值列表
		 */
		ClickHouseRow(std::initializer_list<std::string> values)
			: std::vector<std::string>(values) {
		}
		/**
		 * @brief 检查行数据是否有效.
		 */
		explicit operator bool() const {
			return !empty();
		}
	};
	class ClickHouseResult;
	class ClickHouseConnection;

	/**
	 * @brief ClickHouse 查询结果集
	 *
	 * 封装一组列式 Block 对象，提供逐行迭代接口，
	 * 类比 MySQLResult。
	 */
	class ClickHouseResult final : public Noncopyable, public std::enable_shared_from_this<ClickHouseResult> {
		friend class ClickHouseConnection;
	public:
		using ptr = std::shared_ptr<ClickHouseResult>;
		/**
		 * @brief 检查结果集是否有数据
		 * @return bool 有数据返回 true，否则 false
		 */
		bool isValid() const { return !blocks_.empty(); }
		/**
		 * @brief 检查结果集是否为空（零行）
		 * @return bool 为空返回 true，否则 false
		 */
		bool isEmpty() const { return count() == 0; }
		/**
		 * @brief 获取结果集总行数
		 * @return uint64_t 行数
		 */
		uint64_t count() const { return total_rows_; }
		/**
		 * @brief 获取字段（列）数量
		 * @return uint64_t 字段数，根据第一个 Block 决定
		 */
		uint64_t fields_count() const {
			return blocks_.empty() ? 0 : blocks_.front().GetColumnCount();
		}
		/**
		 * @brief 按索引获取字段名
		 * @param idx 列索引（从 0 开始）
		 * @return std::string 字段名
		 */
		std::string field_name(uint64_t idx) const {
			if (blocks_.empty() || idx >= fields_count()) {
				return {};
			}
			return blocks_.front().GetColumnName(idx);
		}
		/**
		 * @brief 前进到下一行
		 * @return bool 有下一行返回 true，迭代完毕返回 false
		 */
		bool next() {
			if (!started_) {
				// 定位到第一行
				current_block_ = 0;
				current_row_ = 0;
				started_ = true;

				if (blocks_.empty()) {
					return false;
				}
				if (current_row_ < blocks_[current_block_].GetRowCount()) {
					return true;
				}
				// 第一个 Block 为空，尝试后续 Block
				return advanceBlock();
			}

			// 已开始 — 在当前 Block 内前进
			if (current_block_ < blocks_.size()) {
				++current_row_;
				if (current_row_ < blocks_[current_block_].GetRowCount()) {
					return true;
				}
				return advanceBlock();
			}
			return false;
		}

		/**
		 * @brief 获取当前行的各列值
		 *
		 * 必须在成功调用 next() 之后调用
		 * @return ClickHouseRow 当前行数据
		 */
		const clickhouse::Block& row() const {
			return getCurrentBlock();
		}
		/**
		 * @brief 添加一个 Block 到内部存储
		 *
		 * 用作 clickhouse::Client::Select / Execute 的回调目标
		 * @param block ClickHouse 数据块
		 */
		void addBlock(const clickhouse::Block& block) {
			if (block.GetRowCount() == 0) {
				return;
			}
			blocks_.push_back(block);
			total_rows_ += block.GetRowCount();
		}
		/**
		 * @brief 尝试前进到下一个非空 Block 的第一行
		 * @return bool 找到行返回 true
		 */
		bool advanceBlock() {
			while (++current_block_ < blocks_.size()) {
				current_row_ = 0;
				if (blocks_[current_block_].GetRowCount() > 0) {
					return true;
				}
			}
			return false;
		}
		/**
		 * @brief 获取当前行的原始数据块（供业务层自行解析类型）
		 * @return const clickhouse::Block& 当前数据块
		 */
		const clickhouse::Block& getCurrentBlock() const {
			return blocks_.at(current_block_);
		}
		/**
		 * @brief 获取当前行索引
		 * @return size_t 当前行索引
		 */
		size_t getCurrentRow() const {
			return current_row_;
		}
		/**
		 * @brief 将 Block 中某单元格转为字符串表示
		 *
		 * 处理 Nullable、数值、字符串、日期等常见类型
		 * @param block 数据块
		 * @param col_idx 列索引
		 * @param row_idx 行索引
		 * @return std::string 字符串值
		 */
		static std::string columnValueToString(const clickhouse::Block& block,
			size_t col_idx,
			size_t row_idx) {
			auto col = block.At(col_idx);
			if (!col) {
				return "NULL";
			}
			// 处理 Nullable 列
			if (auto nullable = col->As<clickhouse::ColumnNullable>()) {
				if (nullable->IsNull(row_idx)) {
					return "NULL";
				}
				col = nullable->Nested();
			}
			try {
				auto item = col->GetItem(row_idx);
				return itemViewToString(item);
			}
			catch (const clickhouse::UnimplementedError&) {
				// 回退到基于类型的提取
			}
			catch (const std::exception&) {
				// 回退
			}

			return fallbackColumnValue(col, row_idx);
		}
		/**
		 * @brief 将 ItemView 转为人类可读字符串
		 * @param item 数据视图
		 * @return std::string 字符串
		 */
		static std::string itemViewToString(const clickhouse::ItemView& item) {
			using Code = clickhouse::Type::Code;
			switch (item.type) {
			case Code::Int8:
				return std::to_string(static_cast<int32_t>(item.get<int8_t>()));
			case Code::Int16:
				return std::to_string(item.get<int16_t>());
			case Code::Int32:
				return std::to_string(item.get<int32_t>());
			case Code::Int64:
				return std::to_string(item.get<int64_t>());
			case Code::UInt8:
				return std::to_string(static_cast<uint32_t>(item.get<uint8_t>()));
			case Code::UInt16:
				return std::to_string(item.get<uint16_t>());
			case Code::UInt32:
				return std::to_string(item.get<uint32_t>());
			case Code::UInt64:
				return std::to_string(item.get<uint64_t>());
			case Code::Float32:
				return std::to_string(item.get<float>());
			case Code::Float64:
				return std::to_string(item.get<double>());
			case Code::String:
			case Code::FixedString:
				return std::string(item.data);
			default:
				return {};
			}
		}
		/**
		 * @brief 回退：通过类型化 API 直接读取常见列类型
		 * @param col 列引用
		 * @param row_idx 行索引
		 * @return std::string 字符串值
		 */
		static std::string fallbackColumnValue(const clickhouse::ColumnRef& col,
			size_t row_idx) {
			using namespace clickhouse;
			if (auto s = col->As<ColumnString>()) {
				return std::string(s->At(row_idx));
			}
			if (auto fs = col->As<ColumnFixedString>()) {
				return std::string(fs->At(row_idx));
			}
			if (auto u8 = col->As<ColumnUInt8>()) {
				return std::to_string(static_cast<uint32_t>(u8->At(row_idx)));
			}
			if (auto u16 = col->As<ColumnUInt16>()) {
				return std::to_string(u16->At(row_idx));
			}
			if (auto u32 = col->As<ColumnUInt32>()) {
				return std::to_string(u32->At(row_idx));
			}
			if (auto u64 = col->As<ColumnUInt64>()) {
				return std::to_string(u64->At(row_idx));
			}
			if (auto i8 = col->As<ColumnInt8>()) {
				return std::to_string(static_cast<int32_t>(i8->At(row_idx)));
			}
			if (auto i16 = col->As<ColumnInt16>()) {
				return std::to_string(i16->At(row_idx));
			}
			if (auto i32 = col->As<ColumnInt32>()) {
				return std::to_string(i32->At(row_idx));
			}
			if (auto i64 = col->As<ColumnInt64>()) {
				return std::to_string(i64->At(row_idx));
			}
			if (auto f32 = col->As<ColumnFloat32>()) {
				return std::to_string(f32->At(row_idx));
			}
			if (auto f64 = col->As<ColumnFloat64>()) {
				return std::to_string(f64->At(row_idx));
			}

			return {};
		}

		//! 存储的列式数据块
		std::vector<clickhouse::Block> blocks_;
		//! 所有 Block 的行数总和
		uint64_t total_rows_ = 0;
		//! 迭代状态
		size_t current_block_ = 0;
		//! 当前行
		size_t current_row_ = 0;
		//! 开始状态
		bool started_ = false;
	};
	/**
	 * @brief ClickHouse 连接类
	 *
	 * 封装原生 clickhouse::Client，提供与 MySQLConnection 类似的流畅接口：
	 * exec()、exec_fmt()、连接管理、连接池集成
	 */
	class ClickHouseConnection final
		: public Noncopyable, public std::enable_shared_from_this<ClickHouseConnection> {
	public:
		using ptr = std::shared_ptr<ClickHouseConnection>;
		/**
		 * @brief 默认构造函数
		 *
		 * 创建未连接的连接对象
		 */
		ClickHouseConnection() = default;
		/**
		 * @brief 构造函数并立即连接
		 * @param host 服务器地址
		 * @param port 端口（默认 9000）
		 * @param user 用户名
		 * @param password 密码
		 * @param db 数据库名（默认 default）
		 */
		ClickHouseConnection(const std::string& host, uint16_t port, const std::string& user, const std::string& password, const std::string& db = "default")
			: opts_(buildOptions(host, port, user, password, db))
			, client_(std::make_shared<clickhouse::Client>(opts_))
			, last_host_(host)
			, last_port_(port)
			, last_user_(user)
			, last_password_(password)
			, db_(db)
			, connected_(true) {
			updateLastUsedTime();
			CRAZY_SYSTEM_INFO() << "Connected to ClickHouse successfully, host = " << host;
		}
		/**
		 * @brief 验证连接参数有效性
		 * @param host 主机地址
		 * @param user 用户名
		 * @param port 端口
		 * @return bool 参数有效返回 true，否则 false
		 */
		static bool validateParams(const std::string& host, const std::string& user, uint16_t port) {
			return !host.empty() && !user.empty() && port > 0;
		}
		/**
		 * @brief 连接到 ClickHouse 服务器
		 * @param host 服务器地址
		 * @param port 端口
		 * @param user 用户名
		 * @param password 密码
		 * @param db 数据库名（默认 default）
		 * @return bool 连接成功返回 true，否则 false
		 */
		bool connect(const std::string& host, uint16_t port,
			const std::string& user, const std::string& password,
			const std::string& db = "default") {
			if (!validateParams(host, user, port)) {
				CRAZY_SYSTEM_ERROR() << "Invalid ClickHouse connection parameters";
				return false;
			}
			try {
				opts_ = buildOptions(host, port, user, password, db);
				client_ = std::make_shared<clickhouse::Client>(opts_);
				last_host_ = host;
				last_port_ = port;
				last_user_ = user;
				last_password_ = password;
				db_ = db;
				connected_ = true;
				updateLastUsedTime();
				CRAZY_SYSTEM_INFO() << "Connected to ClickHouse successfully, host = " << host;
				return true;
			}
			catch (const std::exception& e) {
				auto errorMessage = clickhouse_detail::NormalizeExceptionMessage(e.what());
				CRAZY_SYSTEM_ERROR() << "ClickHouse connection failed: " << errorMessage;
				connected_ = false;
				return false;
			}
		}
		/**
		 * @brief 检查连接是否有效
		 * @return bool 连接有效返回 true，否则 false
		 */
		bool isConnected() const {
			if (!client_ || !connected_) {
				return false;
			}
			try {
				client_->Ping();
				return true;
			}
			catch (...) {
				return false;
			}
		}
		/**
		 * @brief Ping 服务器检查连接存活
		 * @return bool 连接存活返回 true，否则 false
		 */
		bool ping() {
			if (!client_) return false;
			try {
				client_->Ping();
				return true;
			}
			catch (const std::exception& e) {
				CRAZY_SYSTEM_ERROR() << "ClickHouse ping failed: " << clickhouse_detail::NormalizeExceptionMessage(e.what());
				return false;
			}
		}
		/**
		 * @brief 重置连接
		 */
		void resetConnection() {
			if (client_) {
				try {
					client_->ResetConnection();
				}
				catch (const std::exception& e) {
					CRAZY_SYSTEM_ERROR() << "ClickHouse reset connection failed: " << clickhouse_detail::NormalizeExceptionMessage(e.what());
				}
			}
		}
		/**
		 * @brief 使用保存的参数重新连接
		 * @return bool 重连成功返回 true
		 */
		bool reconnect() {
			if (connected_) {
				try {
					client_->ResetConnection();
					return true;
				}
				catch (...) {
					// 若重置失败，回退到完整重连
				}
			}
			return connect(last_host_, last_port_, last_user_, last_password_, db_);
		}
		/**
		 * @brief 关闭连接
		 */
		void close() {
			connected_ = false;
			client_.reset();
			CRAZY_SYSTEM_INFO() << "ClickHouse connection closed";
		}
		/**
		 * @brief 执行 SQL 查询（SELECT、DDL、DML 等）
		 *
		 * 对于 INSERT 语句，建议使用专门的 insert() 方法
		 * @param sql SQL 语句
		 * @return ClickHouseResult::ptr 结果集（非 SELECT 查询返回空结果集）
		 * @throws ClickHouseConnectionException 如果未连接
		 * @throws ClickHouseQueryException 如果服务器返回错误
		 */
		ClickHouseResult::ptr exec(const std::string& sql) {
			return doExec(sql);
		}
		/**
		 * @brief 安全格式化并执行 SQL 查询（关键方法）
		 *
		 * 使用 printf 风格的格式化构建 SQL 字符串，然后委托给 exec()。
		 * 完美对应 MySQLConnection::exec_fmt。
		 * @param fmt 格式化字符串
		 * @param ... 格式化参数
		 * @return ClickHouseResult::ptr 查询结果
		 * @throws ClickHouseQueryException 如果格式化失败
		 */
		ClickHouseResult::ptr exec_fmt(const char* fmt, ...) {
			va_list args;
			va_start(args, fmt);
			auto size = vsnprintf(nullptr, 0, fmt, args) + 1;
			va_end(args);

			if (size <= 0) {
				throw ClickHouseQueryException("Format string error");
			}

			std::string sql;
			sql.resize(size);

			va_start(args, fmt);
			vsnprintf(sql.data(), size, fmt, args);
			va_end(args);

			return doExec(sql);
		}
		/**
		 * @brief 执行 SELECT 查询（exec 的别名）
		 * @param sql SQL 语句
		 * @return ClickHouseResult::ptr 结果集
		 */
		ClickHouseResult::ptr select(const std::string& sql) {
			return doExec(sql);
		}
		/**
		 * @brief 向表中插入一个 Block
		 * @param table_name 目标表名
		 * @param block 数据块
		 * @throws ClickHouseConnectionException 如果未连接
		 * @throws ClickHouseQueryException 如果插入失败
		 */
		void insert(const std::string& table_name, const clickhouse::Block& block) {
			ensureConnected();
			try {
				client_->Insert(table_name, block);
				CRAZY_SYSTEM_DEBUG() << "Inserted " << block.GetRowCount() << " rows into " << table_name;
			}
			catch (const std::exception& e) {
				throw ClickHouseQueryException("ClickHouse insert failed: " + clickhouse_detail::NormalizeExceptionMessage(e.what()));
			}
			updateLastUsedTime();
		}
		/**
		 * @brief 开始事务
		 * @return bool 成功返回 true
		 */
		bool beginTransaction() {
			return doExec("BEGIN TRANSACTION") != nullptr;
		}
		/**
		 * @brief 提交事务
		 * @return bool 成功返回 true
		 */
		bool commit() {
			return doExec("COMMIT") != nullptr;
		}
		/**
		 * @brief 回滚事务
		 * @return bool 成功返回 true
		 */
		bool rollback() {
			return doExec("ROLLBACK") != nullptr;
		}
		/**
		 * @brief 创建数据库
		 * @param db_name 数据库名
		 * @return bool 成功返回 true
		 */
		bool createDatabase(const std::string& db_name) {
			return doExec("CREATE DATABASE IF NOT EXISTS " + db_name) != nullptr;
		}
		/**
		 * @brief 删除数据库
		 * @param db_name 数据库名
		 * @return bool 成功返回 true
		 */
		bool dropDatabase(const std::string& db_name) {
			return doExec("DROP DATABASE IF EXISTS " + db_name) != nullptr;
		}
		/**
		 * @brief 获取最后使用时间
		 * @return std::chrono::time_point 最后使用时间点
		 */
		std::chrono::time_point<std::chrono::system_clock> getLastUsedTime() const noexcept {
			return last_used_time_;
		}
		/**
		 * @brief 获取空闲时间
		 * @return std::chrono::seconds 空闲时间（秒）
		 */
		std::chrono::seconds getIdleTime() const noexcept {
			return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - last_used_time_);
		}
		/**
		 * @brief 更新最后使用时间
		 */
		void updateLastUsedTime() noexcept {
			last_used_time_ = std::chrono::system_clock::now();
		}
		/**
		 * @brief 获取底层客户端（用于高级用法）
		 * @return std::shared_ptr<clickhouse::Client>
		 */
		std::shared_ptr<clickhouse::Client> getRawClient() { return client_; }
		/**
		 * @brief 获取当前数据库名
		 * @return const std::string& 数据库名
		 */
		const std::string& getDatabase() const { return db_; }
		/**
		 * @brief 获取当前行的原始数据块（供业务层自行解析类型）
		 * @return const clickhouse::Block& 当前数据块
		 */
		/**
		 * @brief 获取服务器信息
		 * @return clickhouse::ServerInfo 服务器信息
		 */
		clickhouse::ServerInfo getServerInfo() const {
			if (client_) {
				return client_->GetServerInfo();
			}
			return clickhouse::ServerInfo{};
		}

	private:
		/**
		 * @brief 从连接参数构建 ClientOptions
		 * @param host 主机
		 * @param port 端口
		 * @param user 用户名
		 * @param password 密码
		 * @param db 数据库
		 * @return clickhouse::ClientOptions 配置
		 */
		static clickhouse::ClientOptions buildOptions(const std::string& host, uint16_t port, const std::string& user, const std::string& password, const std::string& db) {
			clickhouse::ClientOptions opts;
			opts.SetHost(host).SetPort(port).SetUser(user).SetPassword(password).SetDefaultDatabase(db).SetRethrowException(true);
			return opts;
		}
		/**
		 * @brief 确保客户端已连接，否则抛出异常
		 * @throws ClickHouseConnectionException 如果未连接
		 */
		void ensureConnected() {
			if (!client_ || !connected_) {
				throw ClickHouseConnectionException("Connection is not established");
			}
		}
		/**
		 * @brief 内部查询执行，由 exec()、exec_fmt()、select() 共享
		 *
		 * 创建 Query 对象，设置 OnData 回调收集 Block，
		 * 再通过 client_.Execute() 同步执行
		 * @param sql SQL 语句
		 * @return ClickHouseResult::ptr 结果集
		 */
		ClickHouseResult::ptr doExec(const std::string& sql) {
			ensureConnected();
			updateLastUsedTime();
			auto result = std::make_shared<ClickHouseResult>();

			try {
				clickhouse::Query query(sql.c_str());
				query.OnData([result](const clickhouse::Block& block) {
					result->addBlock(block);
					});
				client_->Execute(query);

			}
			catch (const clickhouse::ServerException& e) {
				auto errorMessage = clickhouse_detail::NormalizeExceptionMessage(e.what());
				CRAZY_SYSTEM_ERROR() << "ClickHouse query failed, SQL: " << sql << ", Error: " << errorMessage;
				throw ClickHouseQueryException("ClickHouse query execution failed, SQL: " + sql + ", Error: " + errorMessage, e.GetCode());
			}
			catch (const std::exception& e) {
				auto errorMessage = clickhouse_detail::NormalizeExceptionMessage(e.what());
				CRAZY_SYSTEM_ERROR() << "ClickHouse query failed, SQL: " << sql << ", Error: " << errorMessage;
				throw ClickHouseQueryException("ClickHouse query execution failed, SQL: " + sql + ", Error: " + errorMessage);
			}
			return result;
		}

		//! 链接参数
		clickhouse::ClientOptions opts_;
		//! clickhouse 客户端
		std::shared_ptr<clickhouse::Client> client_;
		//! 连接参数（用于重连）
		std::string last_host_;
		//! 端口号
		uint16_t last_port_ = 9000;
		//! 用户名
		std::string last_user_;
		//! 密码
		std::string last_password_;
		//! 默认数据库
		std::string db_ = "default";
		//! 默认数据库
		bool connected_ = false;
		//! 最后使用时间
		std::chrono::time_point<std::chrono::system_clock> last_used_time_ = std::chrono::system_clock::now();
	};

} // namespace crazy
