# ClickHouse

[Back to documentation index](../README.en.md)

The ClickHouse module provides connections, queries, Block inserts, database operations, transaction interfaces, and a connection pool.

## Query

```cpp
#include "crazy/clickhouse/clickhouse_connection.h"

auto connection = std::make_shared<crazy::ClickHouseConnection>();
if (!connection->connect(
        "127.0.0.1", 9000, "default", "", "default")) {
    return;
}

auto result = connection->exec("SELECT number FROM numbers(3)");
while (result->next()) {
    const auto& block = result->row();
    const auto row = result->getCurrentRow();
    CRAZY_SYSTEM_INFO()
        << crazy::ClickHouseResult::columnValueToString(block, 0, row);
}
```

## Insert a Block

```cpp
clickhouse::Block block;
auto id = std::make_shared<clickhouse::ColumnUInt64>();
auto name = std::make_shared<clickhouse::ColumnString>();

id->Append(1);
name->Append("alice");
block.AppendColumn("id", id);
block.AppendColumn("name", name);

connection->insert("users", block);
```

## Connection pool

```cpp
crazy::ClickHouseConnectionPoolConfig config;
config.host = "127.0.0.1";
config.port = 9000;
config.user = "default";
config.database = "default";
config.min_connections = 1;
config.max_connections = 5;

auto pool =
    std::make_shared<crazy::ClickHouseConnectionPool>(config);
pool->start();

auto connection = pool->getConnection(3000);
connection->exec("SELECT 1");
pool->stop();
```

The wrapper also exposes `exec_fmt()`, `select()`, `ping()`, `reconnect()`, `resetConnection()`, `createDatabase()`, `dropDatabase()`, and `getServerInfo()`.

## Pool configuration

| Field | Default | Meaning |
|-------|---------|---------|
| `host`, `user`, `password` | empty | Connection identity |
| `database` | `default` | Default database |
| `port` | `9000` | Native protocol port |
| `min_connections` | `5` | Minimum idle connections |
| `max_connections` | `20` | Maximum connections |
| `max_idle_time` | `300` | Idle retirement in seconds |
| `max_wait_time` | `30` | Acquisition wait in seconds |
| `connection_timeout` | `10` | Connect timeout in seconds |

`Application` creates the pool when `[ClickHouse]` exists.

## Result navigation

```cpp
auto result = connection->select(
    "SELECT user_id, event_time, event_type "
    "FROM events ORDER BY event_time DESC LIMIT 100");

while (result->next()) {
    const auto& block = result->row();
    const auto row = result->getCurrentRow();
    const auto userId =
        crazy::ClickHouseResult::columnValueToString(block, 0, row);
    const auto eventTime =
        crazy::ClickHouseResult::columnValueToString(block, 1, row);
    const auto eventType =
        crazy::ClickHouseResult::columnValueToString(block, 2, row);
    CRAZY_SYSTEM_INFO() << userId << ", " << eventTime
                        << ", " << eventType;
}
```

Results consist of one or more Blocks. `row()` returns the current Block and `getCurrentRow()` returns its row index.

## Batch insert

```cpp
clickhouse::Block block;
auto id = std::make_shared<clickhouse::ColumnUInt64>();
auto timestamp = std::make_shared<clickhouse::ColumnDateTime>();
auto payload = std::make_shared<clickhouse::ColumnString>();

for (int i = 0; i < 1000; ++i) {
    id->Append(static_cast<uint64_t>(i));
    timestamp->Append(std::time(nullptr));
    payload->Append("event-" + std::to_string(i));
}

block.AppendColumn("id", id);
block.AppendColumn("event_time", timestamp);
block.AppendColumn("payload", payload);
connection->insert("events", block);
```

Batch inserts suit ClickHouse's columnar model better than row-at-a-time SQL.

## Reconnect and limits

```cpp
if (!connection->ping() && !connection->reconnect()) {
    CRAZY_SYSTEM_ERROR() << "clickhouse unavailable";
}
```

The pool checks idle connections, but query timeouts and transaction semantics still depend on server and table engines. Keep large queries off Actor and HTTP request threads.

Related: [MySQL](mysql.md), [Configuration](config.md), [ThreadPool](thread-pool.md).
