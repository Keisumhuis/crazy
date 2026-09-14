# Configuration

[Back to documentation index](../README.en.md)

`crazy::Config` loads INI files and directories, with typed accessors for strings, integers, doubles, and Booleans.

## Load and read values

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

## INI example

```ini
thread_pool_count = 8

[server]
host = 0.0.0.0
port = 8080
debug = false
```

Keys outside a section belong to `global`. Use `HasSection()` to test a section and `EraseValue()` to remove a key. `Application` loads `.ini` files from the current directory during construction.

## Parsing rules

| Input | Result |
|-------|--------|
| `key = value` before a section | Stored under `global` |
| `[server]` | Changes the current section |
| `#` or `;` comment | Truncates the line |
| Quoted value | Removes one matching outer quote pair |
| Line without `=` | Ignored |
| Empty key | Ignored |
| Duplicate key | Last loaded value wins |

`GetBoolean()` treats only the exact string `true` as true. Values such as `yes`, `on`, and `1` are false.

## Load order

Directory traversal order is filesystem-dependent. Do not rely on it for overrides. Load files explicitly in priority order:

```cpp
crazy::Config::LoadConfigFile("./config/base.ini");
crazy::Config::LoadConfigFile("./config/production.ini");
```

The process keeps one global configuration map. There is no file watcher; restart the service or reload values explicitly after changes.

## Validated numeric access

`GetInteger()` and `GetDouble()` may throw on invalid input. Validate externally editable config:

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

## Combined example

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

Related: [Application](application.md), [Logging](logger.md), [MySQL](mysql.md), [ClickHouse](clickhouse.md).
