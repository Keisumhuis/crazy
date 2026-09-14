# ClickHouse

[返回文档目录](../README.md)

ClickHouse 模块提供连接、查询、Block 插入、数据库操作、事务接口和连接池。

## 查询

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

## 插入 Block

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

## 连接池

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

连接封装还提供 `exec_fmt()`、`select()`、`ping()`、`reconnect()`、`resetConnection()`、`createDatabase()`、`dropDatabase()` 和 `getServerInfo()`。

## 连接池配置

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `host` | 空 | ClickHouse 主机 |
| `user` / `password` | 空 | 登录凭据 |
| `database` | `default` | 默认数据库 |
| `port` | `9000` | 原生协议端口 |
| `min_connections` | `5` | 最小空闲连接数 |
| `max_connections` | `20` | 最大连接数 |
| `max_idle_time` | `300` | 空闲回收时间，秒 |
| `max_wait_time` | `30` | 获取连接等待时间，秒 |
| `connection_timeout` | `10` | 建连超时，秒 |

`Application` 的配置段名是 `[ClickHouse]`。配置存在时框架会创建连接池并启动后台监控；未配置时 `getClickHouseConnectionPool()` 会抛出逻辑错误。

## 结果集结构

ClickHouse 查询返回由多个 Block 组成的结果集。`next()` 移动当前行，`row()` 返回当前 Block，`getCurrentRow()` 返回 Block 内的行号，`field_name(index)` 获取列名。

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

Block 和列类型保持 ClickHouse 原生类型。字符串转换适合日志和展示，不应作为所有数值计算的唯一接口。

## 批量插入

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

批量插入比逐行 SQL 更符合 ClickHouse 的列式存储模型。列顺序、名称和类型必须与目标表兼容。

## 健康检查与重连

```cpp
if (!connection->ping()) {
    if (!connection->reconnect()) {
        CRAZY_SYSTEM_ERROR() << "clickhouse unavailable";
    }
}

auto info = connection->getServerInfo();
```

连接池会检查空闲连接有效性。`resetConnection()` 适合清理连接级状态；`reconnect()` 会重新建立底层客户端。

## 限制与建议

- ClickHouse 的“事务接口”与 OLTP 语义不完全等价，应依据服务端版本和表引擎确认。
- 不要把大结果集全部转换成字符串后长期保存。
- 高吞吐写入应使用合适的批量大小和异步表引擎。
- 查询参数不要直接拼接用户输入。
- 长查询不要占用 Actor 或 HTTP 请求线程。

相关文档：[MySQL](mysql.md)、[配置管理](config.md)、[ThreadPool](thread-pool.md)。
