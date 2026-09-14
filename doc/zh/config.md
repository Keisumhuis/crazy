# 配置管理

[返回文档目录](../README.md)

`crazy::Config` 负责加载 INI 文件和目录，并提供字符串、整数、浮点和布尔值读取接口。

## 加载与读取

```cpp
#include "crazy/config.h"

crazy::Config::LoadConfigFile("./config/app.ini");
crazy::Config::LoadConfigPath("./conf.d", ".ini");

const std::string host =
    crazy::Config::GetString("server", "host", "127.0.0.1");
const int64_t port =
    crazy::Config::GetInteger("server", "port", 8080);
const bool debug =
    crazy::Config::GetBoolean("server", "debug", false);
const double ratio =
    crazy::Config::GetDouble("server", "ratio", 1.0);
```

## INI 示例

```ini
thread_pool_count = 8

[server]
host = 0.0.0.0
port = 8080
debug = false
```

没有 section 的键会归入 `global`。`HasSection()` 可判断配置段是否存在，`EraseValue()` 可删除指定配置项。`Application` 构造时会自动加载当前目录下以 `.ini` 结尾的文件。

## 解析规则

| 输入 | 结果 |
|------|------|
| `key = value` 且未声明 section | 写入 `global` 段 |
| `[server]` | 后续键写入 `server` 段，直到下一个 section |
| `# comment` 或 `; comment` | 从注释符开始截断 |
| `key = "value"` | 去掉最外层双引号 |
| `key = 'value'` | 去掉最外层单引号 |
| 没有 `=` 的行 | 跳过 |
| 空 key | 跳过 |
| 重复 key | 后加载的值覆盖之前的值 |

布尔读取只把精确字符串 `true` 视为 `true`，其他值都视为 `false`。因此 `yes`、`1`、`on` 都不会被 `GetBoolean()` 识别为真。

## 目录加载顺序

`LoadConfigPath()` 会递归遍历目录。目录迭代顺序由文件系统决定，多个文件包含同一个 section/key 时不应依赖固定覆盖顺序。需要明确优先级时，应按顺序显式调用：

```cpp
crazy::Config::LoadConfigFile("./config/base.ini");
crazy::Config::LoadConfigFile("./config/production.ini");
```

后加载的 `production.ini` 会覆盖相同键。配置值保存在进程级静态 map 中，所有模块共享同一份数据。

## 安全读取

`GetInteger()` 使用 `std::stoll`，`GetDouble()` 使用 `std::stod`。非法数字可能抛出异常。对外部可编辑的配置，建议先校验原始字符串：

```cpp
int64_t readPort() {
    const std::string raw =
        crazy::Config::GetString("server", "port", "8080");

    try {
        const int64_t port = std::stoll(raw);
        if (port > 0 && port <= 65535) {
            return port;
        }
    } catch (const std::exception&) {
        CRAZY_SYSTEM_ERROR() << "invalid server.port: " << raw;
    }

    return 8080;
}
```

## 配置合并示例

```ini
[global]
thread_pool_count = 8

[service]
address = "0.0.0.0"
port = 9090
heartbeat_interval = 5000

[MySQL]
host = "127.0.0.1"
port = 3306
user = "app"
password = "secret"
min_connections = 2
max_connections = 10
```

配置读取没有热更新监听机制。修改文件后需要重启应用，或在业务层重新调用加载接口并处理既有对象的状态迁移。

相关文档：[Application](application.md)、[日志系统](logger.md)、[MySQL](mysql.md)、[ClickHouse](clickhouse.md)。
