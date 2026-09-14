# Logging

[Back to documentation index](../README.en.md)

The logging module provides trace, debug, info, warn, error, and fatal levels, multiple loggers, console/file appenders, and custom formatting.

## Logging macros

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

## Custom logger

```cpp
auto logger = LOGGER(api);
logger->setLevel(crazy::LoggerLevel::debug);
logger->setFormatter("%d %c %p %f:%l %m");
logger->addAppender(std::make_shared<crazy::LoggerConsoleAppenderImpl>());
logger->addAppender(
    std::make_shared<crazy::LoggerFileAppenderImpl>("./logs"));
```

Format fields include date, category, level, function, line, and message. A macro lookup creates the named logger on first use and reuses it afterwards.

## Levels and filtering

Levels run from trace through fatal. Both the logger and each appender have a level threshold; an event must pass both filters.

```cpp
auto logger = LOGGER(business);
logger->setLevel(crazy::LoggerLevel::warn);

auto console = std::make_shared<crazy::LoggerConsoleAppenderImpl>();
console->setLevel(crazy::LoggerLevel::error);
logger->addAppender(console);
```

In this example, `warn` passes the logger but not the error-only appender.

## Format components

| Token | Value |
|-------|-------|
| `%d` | Date and milliseconds |
| `%c` | Logger category |
| `%p` | Priority |
| `%f` | Function |
| `%l` | Line |
| `%m` | Message |

```cpp
logger->setFormatter("%d %c %p %f:%l %m");
```

## File appender and rotation

`LoggerFileAppenderImpl` creates per-level files such as `business_info.log` and `business_error.log`. When a file reaches `maxLogFileSize`, it is renamed with a timestamp and a new active file is created.

```cpp
auto fileAppender =
    std::make_shared<crazy::LoggerFileAppenderImpl>("./logs");
fileAppender->setMaxSize(10 * 1024 * 1024);
fileAppender->setLevel(crazy::LoggerLevel::debug);
logger->addAppender(fileAppender);
```

Use this with external log rotation or retention policies for long-running deployments. Never log passwords or complete authentication tokens.

Related: [Configuration](config.md), [Application](application.md), [Testing](testing.md).
