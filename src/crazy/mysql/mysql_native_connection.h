/**
 * @file mysql_native_connection.h
 * @author keisum (Keisumhuis@gmail.com)
 * @brief
 * @version 0.1
 * @date 30
 *
 * @copyright Copyright (c) 2024
 */
#pragma once

#include <memory>

#include "third_party/MySQL/include/mysql.h"
#include "crazy/nocopyable.h"

namespace crazy {
	class MySQLNativeResult final
		: public Noncopyable, public std::enable_shared_from_this<MySQLNativeResult> {
	public:
		using ptr = std::shared_ptr<MySQLNativeResult>;
		MySQLNativeResult()
			: result_(nullptr) {
		}
		MySQLNativeResult(MYSQL_RES* result)
			: result_(result) {
		}
		~MySQLNativeResult() {
			free_result();
		}

		// @brief Free the result set resources. Checks if the'result_' pointer is not null, 
		// and if so, calls'mysql_free_result' to release the memory and other resources 
		// occupied by the result set.
		void free_result() {
			if (result_) {
				mysql_free_result(result_);
			}
		}

		// @brief Get the number of rows in the result set. Calls'mysql_num_rows' function 
		// with the internal'result_' pointer and returns the count of rows as an unsigned 64-bit integer.
		// This value represents the total number of rows in the result set retrieved from the database query.
		uint64_t rows_count() {
			return mysql_num_rows(result_);
		}

		// @brief Get the number of fields (columns) in the result set. Calls'mysql_num_fields' function 
		// with the internal'result_' pointer and returns the count of fields as an unsigned 32-bit integer.
		// This value indicates how many columns are present in each row of the result set.
		uint32_t fields_count() {
			return mysql_num_fields(result_);
		}

		// @brief Get the information of a specific field (column) in the result set by its column number. 
		// Calls'mysql_fetch_field_direct' function with the internal'result_' pointer and the specified field number. 
		// Returns a pointer to the MYSQL_FIELD structure which contains details about the column such as its name, type, etc.
		// If the specified column number is out of range or an error occurs, it may return nullptr.
		MYSQL_FIELD* field_with_clumn(uint32_t fieldnr) {
			return mysql_fetch_field_direct(result_, fieldnr);
		}

		// @brief Get an array of MYSQL_FIELD structures representing all the fields (columns) in the result set. 
		// Calls'mysql_fetch_fields' function with the internal'result_' pointer. 
		// Returns a pointer to the array of MYSQL_FIELD structures which can be used to access details of all columns.
		// If there is an issue with fetching the fields or the result set is empty/null, it may return nullptr.
		MYSQL_FIELD* fields() {
			return mysql_fetch_fields(result_);
		}

		// @brief Get the current row position within the result set. Calls'mysql_row_tell' function 
		// with the internal'result_' pointer. Returns the current row offset which can be used 
		// for subsequent operations like seeking to a specific row position.
		MYSQL_ROW_OFFSET row_tell() {
			return mysql_row_tell(result_);
		}

		// @brief Get the current field position within the result set. Calls'mysql_field_tell' function 
		// with the internal'result_' pointer. Returns the current field offset which might be used 
		// in operations related to navigating through fields in the result set.
		MYSQL_FIELD_OFFSET field_tell() {
			return mysql_field_tell(result_);
		}

		// @brief Seek to a specific row position within the result set. Calls'mysql_row_seek' function 
		// with the internal'result_' pointer and the provided row offset. Returns the new row offset 
		// after seeking, allowing further operations from the new position.
		MYSQL_ROW_OFFSET row_seek(MYSQL_ROW_OFFSET offset) {
			return mysql_row_seek(result_, offset);
		}

		// @brief Seek to a specific field position within the result set. Calls'mysql_field_seek' function 
		// with the internal'result_' pointer and the provided field offset. Returns the new field offset 
		// after seeking, enabling operations related to the new field position.
		MYSQL_FIELD_OFFSET field_seek(MYSQL_FIELD_OFFSET offset) {
			return mysql_field_seek(result_, offset);
		}

		// @brief Get the current row data as a MYSQL_ROW pointer. Calls'mysql_fetch_row' function 
		// with the internal'result_' pointer. Returns a pointer to the MYSQL_ROW structure which represents 
		// the current row data. If there are no more rows to fetch (e.g., end of result set reached), it returns nullptr.
		MYSQL_ROW row() {
			return mysql_fetch_row(result_);
		}

		// @brief Get an array of lengths for each field in the current row. Calls'mysql_fetch_lengths' function 
		// with the internal'result_' pointer. Returns a pointer to an array of unsigned long integers, 
		// where each element represents the length of the corresponding field in the current row.
		// If there is an error or no current row available (e.g., end of result set), it may return nullptr.
		unsigned long* lengths() {
			return mysql_fetch_lengths(result_);
		}

	private:
		MYSQL_RES* result_;
	};

	class MySQLNativeStatement final
		: public Noncopyable, public std::enable_shared_from_this<MySQLNativeStatement> {
	public:
		using ptr = std::shared_ptr<MySQLNativeStatement>;
		MySQLNativeStatement()
			: statement_(nullptr) {
		};
		MySQLNativeStatement(MYSQL_STMT* statement)
			: statement_(statement) {
		};
		~MySQLNativeStatement() {
			stmt_free_result();
			stmt_close();
		}

		// @brief Prepare the SQL statement for execution. Calls the'mysql_stmt_prepare' function, 
		// passing in the pointer to the prepared statement, the data of the SQL statement, and its length. 
		// Returns an operation result code (usually 0 indicates success, non-zero indicates failure).
		int32_t stmt_prepare(const std::string& sql) {
			return mysql_stmt_prepare(statement_, sql.data(), sql.size());
		}

		// @brief Execute the prepared SQL statement. Calls the'mysql_stmt_execute' function 
		// using the internal'statement_' pointer. Returns an operation result code (usually 0 indicates success, non-zero indicates failure).
		int32_t stmt_execute() {
			return mysql_stmt_execute(statement_);
		}

		// @brief Fetch a row from the result set of the executed prepared statement. 
		// Calls the'mysql_stmt_fetch' function with the internal'statement_' pointer. 
		// Returns an operation result code (usually 0 indicates success, non-zero indicates failure).
		int32_t stmt_fetch() {
			return mysql_stmt_fetch(statement_);
		}

		// @brief Fetch a specific column from the current row of the result set. 
		// Calls the'mysql_stmt_fetch_column' function with the provided bind argument, column index, 
		// and offset. Returns an operation result code (usually 0 indicates success, non-zero indicates failure).
		int32_t stmt_fetch_column(MYSQL_BIND* bind_arg, uint32_t column, uint32_t offset) {
			return mysql_stmt_fetch_column(statement_, bind_arg, column, offset);
		}

		// @brief Store the entire result set of the executed prepared statement in the client side. 
		// Calls the'mysql_stmt_store_result' function using the internal'statement_' pointer. 
		// Returns an operation result code (usually 0 indicates success, non-zero indicates failure).
		int32_t stmt_store_result() {
			return mysql_stmt_store_result(statement_);
		}

		// @brief Get the number of parameters in the prepared statement. 
		// Calls the'mysql_stmt_param_count' function with the internal'statement_' pointer. 
		// Returns the count of parameters as an unsigned 32-bit integer.
		uint32_t stmt_param_count() {
			return mysql_stmt_param_count(statement_);
		}

		// @brief Set an attribute for the prepared statement. Calls the'mysql_stmt_attr_set' function 
		// with the provided attribute type and the pointer to the attribute value. 
		// Returns true if the attribute is set successfully, false otherwise.
		bool stmt_attr_set(enum enum_stmt_attr_type attr_type, const void* attr) {
			return mysql_stmt_attr_set(statement_, attr_type, attr);
		}

		// @brief Get an attribute value of the prepared statement. Calls the'mysql_stmt_attr_get' function 
		// with the provided attribute type and a pointer to store the retrieved attribute value. 
		// Returns true if the attribute value is retrieved successfully, false otherwise.
		bool stmt_attr_get(enum enum_stmt_attr_type attr_type, void* attr) {
			return!mysql_stmt_attr_get(statement_, attr_type, attr);
		}

		// @brief Bind parameters to the prepared statement. Calls the'mysql_stmt_bind_param' function 
		// with the provided MYSQL_BIND structure pointer. Returns true if the binding is successful, false otherwise.
		bool stmt_bind_param(MYSQL_BIND* bnd) {
			return!mysql_stmt_bind_param(statement_, bnd);
		}

		// @brief Bind the result set columns to variables. Calls the'mysql_stmt_bind_result' function 
		// with the provided MYSQL_BIND structure pointer. Returns true if the binding is successful, false otherwise.
		bool stmt_bind_result(MYSQL_BIND* bnd) {
			return!mysql_stmt_bind_result(statement_, bnd);
		}

		// @brief Close the prepared statement and release associated resources. 
		// Checks if the'statement_' pointer is not null and then calls'mysql_stmt_close' to close it. 
		// Always returns true as long as the pointer was valid initially, false if it was nullptr.
		bool stmt_close() {
			if (statement_) {
				mysql_stmt_close(statement_);
			}
			return true;
		}

		// @brief Reset the prepared statement to its initial state so that it can be re-executed. 
		// Calls the'mysql_stmt_reset' function with the internal'statement_' pointer. 
		// Returns true if the reset is successful, false otherwise.
		bool stmt_reset() {
			return!mysql_stmt_reset(statement_);
		}

		// @brief Free the result set resources associated with the prepared statement. 
		// Calls the'mysql_stmt_free_result' function with the internal'statement_' pointer. 
		// Returns true if the resources are freed successfully, false otherwise.
		bool stmt_free_result() {
			return!mysql_stmt_free_result(statement_);
		}

		// @brief Send long data (such as large text or binary data) for a specific parameter of the prepared statement. 
		// Calls the'mysql_stmt_send_long_data' function with the parameter number, the data pointer, and its size. 
		// Returns true if the data is sent successfully, false otherwise.
		bool stmt_send_long_data(uint32_t param_number, const std::string& sql) {
			return!mysql_stmt_send_long_data(statement_, param_number, sql.data(), sql.size());
		}

		// @brief Get the metadata of the result set of the prepared statement. 
		// Calls the'mysql_stmt_result_metadata' function with the internal'statement_' pointer. 
		// Returns a shared pointer to a MySQLNativeResult object which is expected to manage the retrieved metadata.
		// If the metadata retrieval fails, a nullptr will be returned.
		MySQLNativeResult::ptr stmt_result_metadata() {
			return std::make_shared<MySQLNativeResult>(mysql_stmt_result_metadata(statement_));
		}

		// @brief Get the metadata of the parameters of the prepared statement. 
		// Calls the'mysql_stmt_param_metadata' function with the internal'statement_' pointer. 
		// Returns a shared pointer to a MySQLNativeResult object which is expected to manage the retrieved metadata.
		// If the metadata retrieval fails, a nullptr will be returned.
		MySQLNativeResult::ptr stmt_param_metadata() {
			return std::make_shared<MySQLNativeResult>(mysql_stmt_param_metadata(statement_));
		}

		// @brief Get the error number associated with the prepared statement. 
		// Calls the'mysql_stmt_errno' function with the internal'statement_' pointer. 
		// Returns the error number as an unsigned 32-bit integer.
		uint32_t stmt_errno() {
			return mysql_stmt_errno(statement_);
		}

		// @brief Get the error message associated with the prepared statement. 
		// Calls the'mysql_stmt_error' function with the internal'statement_' pointer. 
		// Returns the error message as a std::string. If there is no error message, an empty string is returned.
		std::string stmt_error() {
			auto error_msg = mysql_stmt_error(statement_);
			return error_msg ? error_msg : "";
		}

		// @brief Get the SQLSTATE value associated with the prepared statement. 
		// Calls the'mysql_stmt_sqlstate' function with the internal'statement_' pointer. 
		// Returns the SQLSTATE value as a std::string. If there is no SQLSTATE value, an empty string is returned.
		std::string stmt_sqlstate() {
			const char* sqlstate = mysql_stmt_sqlstate(statement_);
			return sqlstate ? sqlstate : "";
		}

		// @brief Seek to a specific row position within the result set of the prepared statement. 
		// Calls the'mysql_stmt_row_seek' function with the provided row offset. 
		// Returns the new row offset after seeking.
		MYSQL_ROW_OFFSET stmt_row_seek(MYSQL_ROW_OFFSET offset) {
			return mysql_stmt_row_seek(statement_, offset);
		}

		// @brief Get the current row position within the result set of the prepared statement. 
		// Calls the'mysql_stmt_row_tell' function with the internal'statement_' pointer. 
		// Returns the current row offset.
		MYSQL_ROW_OFFSET stmt_row_tell() {
			return mysql_stmt_row_tell(statement_);
		}

		// @brief Seek to a specific row position within the result set by offset. 
		// Calls the'mysql_stmt_data_seek' function with the provided offset value. 
		// This function has no return value and directly adjusts the internal state of the result set.
		void stmt_data_seek(uint64_t offset) {
			mysql_stmt_data_seek(statement_, offset);
		}

		// @brief Get the number of rows in the result set of the prepared statement. 
		// Calls the'mysql_stmt_num_rows' function with the internal'statement_' pointer. 
		// Returns the row count as an unsigned 64-bit integer.
		uint64_t stmt_num_rows() {
			return mysql_stmt_num_rows(statement_);
		}

		// @brief Get the number of rows affected by the execution of the prepared statement (usually for UPDATE, DELETE, INSERT operations). 
		// Calls the'mysql_stmt_affected_rows' function with the internal'statement_' pointer. 
		// Returns the affected row count as an unsigned 64-bit integer.
		uint64_t stmt_affected_rows() {
			return mysql_stmt_affected_rows(statement_);
		}

		// @brief Get the ID of the last inserted row (if applicable, for example, when an AUTO_INCREMENT column is used in an INSERT operation). 
		// Calls the'mysql_stmt_insert_id' function with the internal'statement_' pointer. 
		// Returns the inserted row ID as an unsigned 64-bit integer.
		uint64_t stmt_insert_id() {
			return mysql_stmt_insert_id(statement_);
		}

		// @brief Get the number of columns in the result set of the prepared statement. 
		// Calls the'mysql_stmt_field_count' function with the internal'statement_' pointer. 
		// Returns the column count as an unsigned 32-bit integer.
		uint32_t stmt_field_count() {
			return mysql_stmt_field_count(statement_);
		}

		// @brief Get the next result set if the prepared statement execution generated multiple result sets. 
		// Calls the'mysql_stmt_next_result' function with the internal'statement_' pointer. 
		// Returns an operation result code (usually 0 indicates success in getting the next result set, non-zero indicates an error or no more result sets).
		int32_t stmt_next_result() {
			return mysql_stmt_next_result(statement_);
		}

	private:
		MYSQL_STMT* statement_;
	};

	class MySQLNativeConnection final
		: public Noncopyable, public std::enable_shared_from_this<MySQLNativeConnection> {
	public:
		using ptr = std::shared_ptr<MySQLNativeConnection>;
		MySQLNativeConnection() : connection_(nullptr) { init(); }
		MySQLNativeConnection(const std::string& host, const std::string& user, const std::string& password, const std::string& db = "", uint32_t port = 3306)
			: MySQLNativeConnection() {
			if (db.empty()) {
				connect(host.data(), user.data(), password.data(), nullptr, port);
			}
			else {
				connect(host.data(), user.data(), password.data(), db.data(), port);
			}
		}
		~MySQLNativeConnection() { close(); }

		// @brief Connect to the MySQL server. If the 'connection_' pointer is null, 
		// it initializes the connection object first. Then, it configures the SSL mode 
		// and attempts to establish a real connection using'mysql_real_connect'. 
		// If the connection fails, it logs an error message and returns false. 
		// Additionally, it tries to set the character set to "utf8" and returns false 
		// if that operation fails as well. Otherwise, it returns true upon successful connection.
		bool connect(const char* host, const char* user, const char* password, const char* db = "", uint32_t port = 3306, const char* unix_socket = nullptr, int32_t client_flag = 0) {
			if (!connection_) {
				init();
			}
			unsigned int ssl_mode = SSL_MODE_DISABLED;
			options(MYSQL_OPT_SSL_MODE, &ssl_mode);
			my_bool reconnect = 1;
			options(MYSQL_OPT_RECONNECT, &reconnect);
			if (!mysql_real_connect(connection_, host, user, password, db, port, unix_socket, client_flag)) {
				std::cerr << "connect to mysql server fail, host = " << host << ", port = " << port << ", user = " << user << ", password = " << password
					<< ", errno = " << get_errno() << ", error message = " << get_error_message() << std::endl;
				return false;
			}
			if (!set_character_set("utf8")) {
				std::cerr << "set mysql charset name = utf8 fail, errno = " << get_errno() << ", error message = " << get_error_message() << std::endl;
				return false;
			}
			return true;
		}

		// @brief Reset the current connection by calling'mysql_reset_connection'. 
		// Returns true if the reset operation is successful (i.e., the function returns 0), 
		// and false otherwise.
		bool reset_connection() {
			return !mysql_reset_connection(connection_);
		}

		// @brief Close the current MySQL connection. Checks if the 'connection_' pointer 
		// is not null and, if so, calls'mysql_close' to release the connection and sets 
		// the 'connection_' pointer to nullptr to indicate that the connection is closed.
		void close() {
			if (connection_) {
				mysql_close(connection_);
				connection_ = nullptr;
			}
		}

		// @brief Set a MySQL option. Calls'mysql_options' with the provided option and argument. 
		// Returns true if the option is set successfully (i.e., the function returns 0), 
		// and false otherwise.
		bool options(mysql_option option, const void* arg) {
			return!mysql_options(connection_, option, arg);
		}

		// @brief Set a MySQL option that requires two arguments. Calls'mysql_options4' 
		// with the provided option, first argument, and second argument. Returns true if 
		// the option is set successfully (i.e., the function returns 0), and false otherwise.
		bool options4(mysql_option option, const void* arg1, const void* arg2) {
			return!mysql_options4(connection_, option, arg1, arg2);
		}

		// @brief Get a MySQL option. Calls'mysql_get_option' with the provided option and argument. 
		// Returns true if the option value is retrieved successfully (i.e., the function returns 0), 
		// and false otherwise.
		bool get_option(mysql_option option, const void* arg) {
			return!mysql_get_option(connection_, option, arg);
		}

		// @brief Set the MySQL character set. Calls'mysql_set_character_set' with the 
		// provided character set name (default is "utf8"). Returns true if the character 
		// set is set successfully (i.e., the function returns 0), and false otherwise.
		bool set_character_set(const std::string& charset = "utf8") {
			return!mysql_set_character_set(connection_, charset.data());
		}

		// @brief Get the name of the currently set MySQL character set. Calls'mysql_character_set_name' 
		// and returns the character set name as a std::string.
		const std::string get_character_set_name() {
			return mysql_character_set_name(connection_);
		}

		// @brief Get detailed information about the currently set MySQL character set. Calls'mysql_get_character_set_info' 
		// to fill the provided 'MY_CHARSET_INFO' structure and returns it.
		MY_CHARSET_INFO get_character_set_info() {
			MY_CHARSET_INFO info;
			mysql_get_character_set_info(connection_, &info);
			return info;
		}

		// @brief Set the SSL configuration for the MySQL connection. Calls'mysql_ssl_set' with the 
		// provided SSL key, certificate, CA certificate, CA path, and cipher. Returns true if 
		// the SSL configuration is set successfully (i.e., the function returns 0), and false otherwise.
		bool set_ssl(const std::string& key, const std::string& cert, const std::string& ca, const std::string& capath, const std::string& cipher) {
			return mysql_ssl_set(connection_, key.data(), cert.data(), ca.data(), capath.data(), cipher.data());
		}

		// @brief Get the currently used SSL cipher for the MySQL connection. Calls'mysql_get_ssl_cipher' 
		// and returns the cipher as a std::string. If the result is null, an empty string is returned.
		std::string get_ssl_cipher() {
			auto result = mysql_get_ssl_cipher(connection_);
			return result ? result : "";
		}

		// @brief Change the user for the current MySQL connection. Calls'mysql_change_user' with the 
		// provided new user, password, and database name. Returns true if the user change is successful 
		// (i.e., the function returns 0), and false otherwise.
		bool change_user(const std::string& user, const std::string& password, const std::string& db) {
			return!mysql_change_user(connection_, user.data(), password.data(), db.data());
		}

		// @brief Get the error number associated with the last MySQL operation on this connection. 
		// Calls'mysql_errno' and returns the error number as an unsigned 32-bit integer.
		uint32_t get_errno() {
			return mysql_errno(connection_);
		}

		// @brief Get the error message associated with the last MySQL operation on this connection. 
		// Calls'mysql_error' and returns the error message as a std::string.
		std::string get_error_message() {
			return mysql_error(connection_);
		}

		// @brief Get the SQL state associated with the last SQL operation on this connection. 
		// Calls'mysql_sqlstate' and returns the SQL state as a std::string. If the result is null, 
		// an empty string is returned.
		std::string sql_state() {
			auto result = mysql_sqlstate(connection_);
			return result ? result : "";
		}

		// @brief Get the count of SQL warnings generated by the last SQL operation on this connection. 
		// Calls'mysql_warning_count' and returns the warning count as an unsigned 32-bit integer.
		uint32_t sql_warning_count() {
			return mysql_warning_count(connection_);
		}

		// @brief Get additional information about the last SQL operation on this connection. 
		// Calls'mysql_info' and returns the information as a std::string. If the result is null, 
		// an empty string is returned.
		std::string sql_info() {
			auto result = mysql_info(connection_);
			return result ? result : "";
		}

		// @brief Get the ID of the current thread handling the MySQL session for this connection. 
		// Calls'mysql_thread_id' and returns the thread ID as an unsigned 32-bit integer.
		uint32_t get_mysql_session_thread_id() {
			return mysql_thread_id(connection_);
		}

		// @brief Get the current status of the MySQL server related to this connection. 
		// Calls'mysql_stat' and returns the status as a std::string. If the result is null, 
		// an empty string is returned.
		std::string server_status() {
			auto result = mysql_stat(connection_);
			return result ? result : "";
		}

		// @brief Get general information about the MySQL server. Calls'mysql_get_server_info' 
		// and returns the server information as a std::string. If the result is null, 
		// an empty string is returned.
		std::string server_info() {
			auto result = mysql_get_server_info(connection_);
			return result ? result : "";
		}

		// @brief Get information about the host where the MySQL server is located. Calls'mysql_get_host_info' 
		// and returns the host information as a std::string. If the result is null, 
		// an empty string is returned.
		std::string server_host_info() {
			auto result = mysql_get_host_info(connection_);
			return result ? result : "";
		}

		// @brief Get the version number of the MySQL server. Calls'mysql_get_server_version' 
		// and returns the version as an unsigned 32-bit integer.
		uint32_t server_version() {
			return mysql_get_server_version(connection_);
		}

		// @brief Get protocol information related to the MySQL connection. Calls'mysql_get_proto_info' 
		// and returns the protocol information as an unsigned 32-bit integer.
		uint32_t proto_info() {
			return mysql_get_proto_info(connection_);
		}

		// @brief Get information about the MySQL client being used. Calls'mysql_get_client_info' 
		// and returns the client information as a std::string. If the result is null, 
		// an empty string is returned.
		std::string client_info() {
			auto result = mysql_get_client_info();
			return result ? result : "";
		}

		// @brief Get the version number of the MySQL client. Calls'mysql_get_client_version' 
		// and returns the version as an unsigned 32-bit integer.
		uint32_t client_version() {
			return mysql_get_client_version();
		}

		// @brief Ping the MySQL server to check if the connection is still alive. Calls'mysql_ping' 
		// and returns 0 if the connection is alive (i.e., the function returns 0), and a non-zero 
		// value otherwise. The return value is negated to make it consistent with the convention 
		// of returning true for success and false for failure in other functions.
		uint32_t ping() {
			return mysql_ping(connection_);
		}

		// @brief Select a database on the MySQL server. Calls'mysql_select_db' with the 
		// provided database name. Returns true if the database selection is successful 
		// (i.e., the function returns 0), and false otherwise.
		bool select_db(const std::string& db) {
			return!mysql_select_db(connection_, db.data());
		}

		// @brief Get information about the fields (columns) of a specific table on the MySQL server. 
		// Calls'mysql_list_fields' with the provided table name and creates a shared pointer 
		// to a 'MySQLNativeResult' object using the result of'mysql_list_fields'. This object 
		// can be used to further access and manipulate the table fields information.
		MySQLNativeResult::ptr table_fields(const std::string& table) {
			return std::make_shared<MySQLNativeResult>(mysql_list_fields(connection_, table.data(), nullptr));
		}

		// @brief Execute an SQL query on the MySQL server. Calls'mysql_query' with the 
		// provided SQL statement. Returns true if the query execution is successful 
		// (i.e., the function returns 0), and false otherwise.
		bool query(const std::string& sql) {
			return!mysql_query(connection_, sql.data());
		}

		// @brief Store the result of the last SQL query executed on the MySQL server. 
		// Calls'mysql_store_result' and creates a shared pointer to a 'MySQLNativeResult' 
		// object using the result of'mysql_store_result'. This object can be used to 
		// access and manipulate the query result data.
		MySQLNativeResult::ptr store_result() {
			return std::make_shared<MySQLNativeResult>(mysql_store_result(connection_));
		}

		// @brief Get the count of fields (columns) in the result of the last SQL query. 
		// Calls'mysql_field_count' and returns the field count as an unsigned 32-bit integer.
		uint32_t field_count() {
			return mysql_field_count(connection_);
		}

		// @brief Get the number of rows affected by the last SQL query (usually for UPDATE, DELETE, INSERT operations). 
		// Calls'mysql_affected_rows' and returns the affected row count as an unsigned 64-bit integer.
		uint64_t affected_rows() {
			return mysql_affected_rows(connection_);
		}

		// @brief Get the ID of the last inserted row (if applicable, for example, when an AUTO_INCREMENT column is used in an INSERT operation). 
		// Calls'mysql_insert_id' and returns the inserted row ID as an unsigned 64-bit integer.
		uint64_t insert_id() {
			return mysql_insert_id(connection_);
		}

		// @brief Check if there is another result set available after the last SQL query execution. 
		// Calls'mysql_more_results' and returns true if there is another result set, and false otherwise.
		bool has_next_result() {
			return mysql_more_results(connection_);
		}

		// @brief Get the next result set if available after the last SQL query execution. 
		// Calls'mysql_next_result' and returns true if the next result set is successfully retrieved, 
		// and false otherwise.
		bool next_result() {
			return!mysql_next_result(connection_);
		}

		// @brief Commit the current transaction on the MySQL server. Calls'mysql_commit' 
		// and returns true if the commit operation is successful (i.e., the function returns 0), 
		// and false otherwise.
		bool commit() {
			return mysql_commit(connection_);
		}

		// @brief Rollback the current transaction on the MySQL server. Calls'mysql_rollback' 
		// and returns true if the rollback operation is successful (i.e., the function returns 0), 
		// and false otherwise.
		bool rollback() {
			return mysql_rollback(connection_);
		}

		// @brief Set the auto-commit mode for the MySQL connection. Calls'mysql_autocommit' 
		// with the provided auto-commit mode (default is true). Returns true if the mode is set successfully, 
		// and false otherwise.
		bool autocommit(bool auto_mode = true) {
			return mysql_autocommit(connection_, auto_mode);
		}

		// @brief Create a new 'MySQLNativeStatement' object for preparing and executing SQL statements. 
		// Calls'mysql_stmt_init' to initialize a statement object and creates a shared pointer 
		// to a 'MySQLNativeStatement' object using the result.
		MySQLNativeStatement::ptr create_statement() {
			return std::make_shared<MySQLNativeStatement>(mysql_stmt_init(connection_));
		}

	protected:
		// @brief Initialize the MySQL connection object. Calls'mysql_init' to create a new 
		// MySQL connection handle. If the initialization fails, it logs an error message.
		void init() {
			connection_ = mysql_init(nullptr);
			if (!connection_) {
				std::cerr << "init mysql connection fail, errno = " << mysql_errno(connection_) << ", error message = " << mysql_error(connection_) << std::endl;
				return;
			}
		}

	private:
		MYSQL* connection_;
	};
}
