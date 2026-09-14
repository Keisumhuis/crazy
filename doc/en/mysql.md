# MySQL

[Back to documentation index](../README.en.md)

The MySQL module provides a native connection wrapper, result sets, prepared statements, transactions, and a managed connection pool.

## Direct connection

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

## Formatting and transactions

```cpp
auto result =
    connection->exec_fmt("SELECT * FROM users WHERE id = %d", 7);

connection->beginTransaction();
connection->exec("UPDATE accounts SET balance = balance - 10 WHERE id = 1");
connection->exec("UPDATE accounts SET balance = balance + 10 WHERE id = 2");
connection->commit();
```

`exec_fmt()` is only safe for trusted format strings. Use prepared statements for user input.

## Connection pool

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

The pool maintains idle connections, wait timeouts, validity checks, and automatic return. `Application` starts it automatically when `[MySQL]` exists.

## Pool configuration

| Field | Default | Meaning |
|-------|---------|---------|
| `host`, `user`, `password`, `database` | empty | Connection identity |
| `port` | `3306` | Server port |
| `charset` | `utf8mb4` | Connection charset |
| `min_connections` | `5` | Minimum idle connections |
| `max_connections` | `20` | Maximum total connections |
| `max_idle_time` | `300` | Idle retirement time in seconds |
| `max_wait_time` | `30` | Acquisition wait in seconds |
| `connection_timeout` | `10` | Connect timeout in seconds |

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

Use `MYSQL_CONNECTION_POOL` or `Application::application()->getMySQLConnectionPool()` in application code.

## Result sets

`row()` returns the next row, `fields()` exposes metadata, `count()` returns row count, and `fields_count()` returns column count.

```cpp
auto result = connection->exec(
    "SELECT id, name FROM users ORDER BY id");

while (auto row = result->row()) {
    const uint64_t id = std::stoull(row[0] ? row[0] : "0");
    const std::string name = row[1] ? row[1] : "";
    CRAZY_SYSTEM_INFO() << id << ", " << name;
}
```

MySQL field values can be null, so check each entry before constructing a string.

## Prepared statements

```cpp
auto statement = connection->create_statement();
statement->prepare("SELECT name FROM users WHERE id = ?");

MYSQL_BIND input[1] = {};
uint64_t userId = 7;
input[0].buffer_type = MYSQL_TYPE_LONGLONG;
input[0].buffer = &userId;

statement->bind_input_param(input);
statement->execute();
statement->fetch();
```

Input and output buffers must remain alive through execution and result fetching.

## Transactions and diagnostics

```cpp
connection->beginTransaction();
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

Use `get_errno()`, `get_error_message()`, `sql_state()`, and `sql_warning_count()` for diagnostics. Keep transactions short and move blocking queries into a worker thread.

Related: [ClickHouse](clickhouse.md), [Configuration](config.md), [ThreadPool](thread-pool.md).
