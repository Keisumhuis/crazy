/**
 * @file test_clickhouse.cc
 * @author keisum (Keisumhuis@gmail.com)
 * @brief ClickHouse 封装测试
 * @version 0.1
 * @date 2024
 *
 * @copyright Copyright (c) 2024
 */
#include "crazy.h"
#include <iostream>

int main() {
    // 测试异常类
    {
        crazy::ClickHouseException e("test error", 100);
        assert(e.getErrorCode() == 100);
        assert(std::string(e.what()) == "test error");
    }
    {
        crazy::ClickHouseConnectionException e("connection error");
        assert(std::string(e.what()) == "connection error");
    }
    {
        crazy::ClickHouseQueryException e("query error", 200);
        assert(e.getErrorCode() == 200);
    }

    // 测试 ClickHouseRow
    {
        crazy::ClickHouseRow row({"a", "b", "c"});
        assert(row.size() == 3);
        assert(row[0] == "a");
        assert(row[1] == "b");
        assert(row[2] == "c");
        assert(static_cast<bool>(row) == true);
    }
    {
        crazy::ClickHouseRow empty_row;
        assert(empty_row.size() == 0);
        assert(static_cast<bool>(empty_row) == false);
    }

    // 测试 ClickHouseResult 空结果
    {
        auto result = std::make_shared<crazy::ClickHouseResult>();
        assert(result->isValid() == false);
        assert(result->isEmpty() == true);
        assert(result->count() == 0);
        assert(result->fields_count() == 0);
        assert(result->next() == false);
    }

    // 测试 exec_fmt 异常抛出（未连接时）
    {
        auto conn = std::make_shared<crazy::ClickHouseConnection>();
        try {
            conn->exec_fmt("SELECT %d", 1);
            assert(false && "Should have thrown exception");
        } catch (const crazy::ClickHouseConnectionException& e) {
            // 未连接时应该抛出连接异常
        }
    }

    // 测试 validateParams
    {
        assert(crazy::ClickHouseConnection::validateParams("localhost", "default", 9000) == true);
        assert(crazy::ClickHouseConnection::validateParams("", "default", 9000) == false);
        assert(crazy::ClickHouseConnection::validateParams("localhost", "", 9000) == false);
        assert(crazy::ClickHouseConnection::validateParams("localhost", "default", 0) == false);
    }

    // 尝试连接（如果 ClickHouse 服务可用）
    {
        auto conn = std::make_shared<crazy::ClickHouseConnection>();
        bool connected = conn->connect("localhost", 9000, "default", "", "default");
        if (connected) {
            CRAZY_ROOT_INFO() << "ClickHouse connected successfully";
            
            // 测试 exec
            auto result = conn->exec("SELECT 1 AS test_col");
            assert(result != nullptr);

            if (result->next()) {
                auto row = result->row();
                CRAZY_ROOT_DEBUG() << "Row: " << row[0];
            }

            // 测试 exec_fmt
            auto result2 = conn->exec_fmt("SELECT %d AS num, '%s' AS str", 42, "hello");
            assert(result2 != nullptr);

            if (result2->next()) {
                auto row = result2->row();
                CRAZY_ROOT_DEBUG() << "exec_fmt result: num=" << row[0] << ", str=" << row[1];
            }

            // 测试 insert
            conn->exec("CREATE DATABASE IF NOT EXISTS test_db");
            conn->exec("DROP TABLE IF EXISTS test_db.test_table");
            conn->exec("CREATE TABLE IF NOT EXISTS test_db.test_table ("
                       "id UInt32, name String"
                       ") ENGINE = Memory");

            {
                clickhouse::Block block;
                auto id_col = std::make_shared<clickhouse::ColumnUInt32>();
                auto name_col = std::make_shared<clickhouse::ColumnString>();
                id_col->Append(1u);
                name_col->Append("Alice");
                id_col->Append(2u);
                name_col->Append("Bob");
                block.AppendColumn("id", id_col);
                block.AppendColumn("name", name_col);
                conn->insert("test_db.test_table", block);
            }

            // 验证插入
            auto result3 = conn->exec("SELECT id, name FROM test_db.test_table ORDER BY id");
            assert(result3 != nullptr);
            uint32_t row_count = 0;
            while (result3->next()) {
                auto row = result3->row();
                CRAZY_ROOT_DEBUG() << "id=" << row[0] << ", name=" << row[1];
                ++row_count;
            }
            assert(row_count == 2);

            // 清理
            conn->exec("DROP TABLE IF EXISTS test_db.test_table");
            conn->exec("DROP DATABASE IF EXISTS test_db");

            // 测试 isConnected/ping/close
            assert(conn->isConnected() == true);
            assert(conn->ping() == true);
            conn->close();
            assert(conn->isConnected() == false);

            CRAZY_ROOT_INFO() << "All ClickHouse connection tests passed!";
        } else {
            CRAZY_ROOT_WARN() << "ClickHouse server not available, skipping integration tests";
        }
    }

    // 测试连接池配置验证
    {
        crazy::ClickHouseConnectionPoolConfig config;
        config.host = "";
        assert(config.isValid() == false);

        config.host = "localhost";
        config.user = "default";
        config.port = 9000;
        assert(config.isValid() == true);

        config.max_connections = 1;
        assert(config.isValid() == false);
    }

    // 测试连接池启动/停止（不依赖实际服务器）
    {
        crazy::ClickHouseConnectionPoolConfig config;
        config.host = "localhost";
        config.user = "default";
        config.password = "";
        config.database = "default";
        config.port = 9000;
        config.min_connections = 1;
        config.max_connections = 5;

        auto pool = std::make_shared<crazy::ClickHouseConnectionPool>(config);
        assert(pool->isRunning() == false);
        pool->start();
        pool->stop();
        assert(pool->isRunning() == false);
    }

    CRAZY_ROOT_INFO() << "All ClickHouse tests completed!";
    return 0;
}
