# MySQL

[返回文档目录](../README.md)

MySQL 模块提供原生连接封装、结果集、预处理语句、事务和自动管理的连接池。

## 直接连接

```cpp
#include "crazy/mysql/mysql_connection.h"

auto connection = std::make_shared<crazy::MySQLConnection>();
if (!connection->connect(
        "127.0.0.1", "root", "password", "app", 3306)) {
    CRAZY_SYSTEM_ERROR() << connection->get_error_message();
    return;
}

auto result = connection->exec(
    "SELECT id, name FROM users WHERE enabled = 1");
while (auto row = result->row()) {
    CRAZY_SYSTEM_INFO() << row[0] << ", " << row[1];
}
```

## 格式化 SQL 与事务

```cpp
auto result =
    connection->exec_fmt("SELECT * FROM users WHERE id = %d", 7);

connection->beginTransaction();
connection->exec("UPDATE accounts SET balance = balance - 10 WHERE id = 1");
connection->exec("UPDATE accounts SET balance = balance + 10 WHERE id = 2");
connection->commit();
```

`exec_fmt()` 只适合可信格式字符串。包含用户输入时应优先使用预处理语句，避免 SQL 注入。

## 连接池

```cpp
crazy::MySQLConnectionPoolConfig config;
config.host = "127.0.0.1";
config.user = "root";
config.password = "password";
config.database = "app";
config.min_connections = 2;
config.max_connections = 10;

auto pool = std::make_shared<crazy::MySQLConnectionPool>(config);
pool->start();

auto connection = pool->getConnection(3000);
auto result = connection->exec("SELECT 1");
CRAZY_SYSTEM_INFO() << pool->getStats();

pool->stop();
```

连接池会维护最小空闲连接、等待超时、有效性检查和自动归还。`Application` 检测到 `[MySQL]` 配置段时会自动创建并启动连接池。

## 连接池配置

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `host` | 空 | MySQL 主机 |
| `user` / `password` | 空 | 登录凭据 |
| `database` | 空 | 默认数据库 |
| `port` | `3306` | 服务端口 |
| `charset` | `utf8mb4` | 连接字符集 |
| `min_connections` | `5` | 最小空闲连接数 |
| `max_connections` | `20` | 最大连接数 |
| `max_idle_time` | `300` | 空闲回收时间，秒 |
| `max_wait_time` | `30` | 获取连接等待时间，秒 |
| `connection_timeout` | `10` | 建连超时，秒 |

`Application` 可以从以下配置段创建池：

```ini
[MySQL]
host = "127.0.0.1"
port = 3306
user = "app"
password = "secret"
database = "app"
min_connections = 2
max_connections = 10
```

应用代码通过 `MYSQL_CONNECTION_POOL` 或 `Application::application()->getMySQLConnectionPool()` 获取。未配置 `[MySQL]` 时访问会抛出 `std::logic_error`。

## 结果集

`exec()` 返回 `MySQLResult::ptr`。查询结果通过 `row()` 逐行读取，`fields()` 提供字段元数据，`count()` 返回行数，`fields_count()` 返回列数。

```cpp
auto result = connection->exec(
    "SELECT id, name, created_at FROM users ORDER BY id");

if (!result || result->isEmpty()) {
    CRAZY_SYSTEM_INFO() << "no rows";
    return;
}

while (auto row = result->row()) {
    const uint64_t id = std::stoull(row[0] ? row[0] : "0");
    const std::string name = row[1] ? row[1] : "";
    CRAZY_SYSTEM_INFO() << id << ", " << name;
}
```

MySQL C API 的字段值可能为 `nullptr`，读取前必须判空。不要对 NULL 直接构造 `std::string`。

## 预处理语句

预处理语句适合重复执行带参数查询：

```cpp
auto statement = connection->create_statement();
if (!statement->prepare(
        "SELECT name FROM users WHERE id = ?")) {
    return;
}

MYSQL_BIND input[1] = {};
uint64_t userId = 7;
input[0].buffer_type = MYSQL_TYPE_LONGLONG;
input[0].buffer = &userId;

statement->bind_input_param(input);
statement->execute();

while (statement->fetch()) {
    // 根据输出绑定读取结果。
}
```

示例中的 `MYSQL_BIND` 生命周期必须覆盖执行和结果读取阶段。输出字符串缓冲区需要提前分配，具体长度应从 `param_metadata()` 或业务字段约束获得。

## 事务

```cpp
if (!connection->beginTransaction()) {
    return;
}

try {
    connection->exec("UPDATE wallet SET balance = balance - 100 WHERE id = 1");
    connection->exec("UPDATE wallet SET balance = balance + 100 WHERE id = 2");

    if (!connection->commit()) {
        connection->rollback();
    }
} catch (...) {
    connection->rollback();
    throw;
}
```

事务应保持短小，并确保异常路径执行回滚。连接池返回的连接通过包装对象在析构时自动归还，不应手工销毁池中的连接。

## 错误诊断

`get_errno()`、`get_error_message()`、`sql_state()` 和 `sql_warning_count()` 可提供详细错误。池级 `getStats()` 返回总数、空闲数和活跃数。常见问题包括 DNS、认证、数据库不存在、连接耗尽和 SQL 语法错误。

数据库调用是阻塞操作，不应直接放在 Actor 或 HTTP 请求线程中执行长查询。可以在线程池任务中查询，再把结果作为 `MessageBase` 发送回 Actor。

相关文档：[ClickHouse](clickhouse.md)、[配置管理](config.md)、[ThreadPool](thread-pool.md)。
