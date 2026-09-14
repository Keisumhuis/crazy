# 日志系统

[返回文档目录](../README.md)

日志模块提供 trace、debug、info、warn、error、fatal 六个级别，并支持多 Logger、控制台/文件 Appender 和自定义格式。

## 快捷日志

```cpp
#include "crazy/logger.h"

CRAZY_SYSTEM_TRACE() << "trace";
CRAZY_SYSTEM_DEBUG() << "debug";
CRAZY_SYSTEM_INFO() << "server started";
CRAZY_SYSTEM_WARN() << "slow request";
CRAZY_SYSTEM_ERROR() << "request failed";
CRAZY_SYSTEM_FATAL() << "fatal error";

CRAZY_ROOT_INFO() << "root logger";
CRAZY_LOG(api, crazy::LoggerLevel::info) << "api request";
```

## 自定义 Logger

```cpp
auto logger = LOGGER(api);
logger->setLevel(crazy::LoggerLevel::debug);
logger->setFormatter("%d %c %p %f:%l %m");
logger->addAppender(std::make_shared<crazy::LoggerConsoleAppenderImpl>());
logger->addAppender(
    std::make_shared<crazy::LoggerFileAppenderImpl>("./logs"));
```

格式字段包括日期、分类、级别、函数、行号和消息。日志对象通过宏按名称获取，第一次获取时创建，后续调用复用同一个实例。

## 日志级别

级别从低到高为 `trace`、`debug`、`info`、`warn`、`error`、`fatal`。Logger 自身和 Appender 都有级别，事件只有同时满足两层过滤条件时才会真正写出。

```cpp
auto logger = LOGGER(business);
logger->setLevel(crazy::LoggerLevel::warn);

auto console = std::make_shared<crazy::LoggerConsoleAppenderImpl>();
console->setLevel(crazy::LoggerLevel::error);
logger->addAppender(console);
```

上例中 `warn` 事件通过 Logger，但不会通过只接收 `error` 的控制台 Appender。

## 格式组件

| 占位符 | 内容 | 示例 |
|--------|------|------|
| `%d` | 日期时间（毫秒） | `2026-09-14 12:30:45.123` |
| `%c` | Logger 分类 | `[business]` |
| `%p` | 日志级别 | `[info]` |
| `%f` | 函数名 | `handleRequest` |
| `%l` | 行号 | `42` |
| `%m` | 日志消息 | `request completed` |

```cpp
logger->setFormatter(
    "%d %c %p %f:%l %m");
```

格式化器在设置格式时解析组件，日志事件按顺序输出。未知的 `%` 组合按普通字符处理。

## 文件 Appender

`LoggerFileAppenderImpl` 按日志级别生成文件，例如 `business_info.log`、`business_error.log`。达到 `maxLogFileSize` 后，当前文件会带上时间戳后归档，并创建新的活动文件。

```cpp
auto fileAppender =
    std::make_shared<crazy::LoggerFileAppenderImpl>("./logs");
fileAppender->setMaxSize(10 * 1024 * 1024);
fileAppender->setLevel(crazy::LoggerLevel::debug);
logger->addAppender(fileAppender);
```

目录不存在时会递归创建。多个线程写同一个 Appender 时，内部互斥锁会串行化写入，但进程级并发写入仍应采用单写进程或外部日志归集。

## 运行建议

- 生产日志建议至少记录时间、级别、分类、函数和消息。
- 高频循环不要记录 trace 级别的大对象。
- 不要把密码、Token、完整请求体直接写入日志。
- 为日志目录设置磁盘配额和归档清理策略。
- `fatal` 只表示不可继续运行，日志宏本身不会自动终止进程。

相关文档：[配置管理](config.md)、[Application](application.md)、[测试](testing.md)。
