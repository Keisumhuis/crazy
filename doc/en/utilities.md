# General Utilities

[Back to documentation index](../README.en.md)

The utilities module provides timestamps, UUIDs, string/path/thread helpers, endian conversion, singleton and noncopyable bases, and command-line parsing.

## Timestamps and UUID

```cpp
#include "crazy/utils.h"

const uint64_t seconds = crazy::GetCurrentSS();
const uint64_t milliseconds = crazy::GetCurrentMS();
const uint64_t microseconds = crazy::GetCurrentUS();
const uint64_t nanoseconds = crazy::GetCurrentNS();
const std::string id = crazy::CreateUUID();
```

## Strings and paths

```cpp
const auto parts = crazy::StringUtil::Split("a,b,c", ",");
const std::string text = crazy::StringUtil::Trim("  hello  ");
const std::string upper = crazy::StringUtil::ToUpper("crazy");

const std::string exe = crazy::PathUtil::GetExecutablePath();
const std::string dir = crazy::PathUtil::GetExecutableDirectory();
const std::string file = crazy::PathUtil::JoinPath(dir, "app.ini");
crazy::PathUtil::CreateDirectories("./logs/app");
```

## Thread names and endian conversion

```cpp
#include "crazy/byte_order.h"

crazy::ThreadUtil::SetThreadName("worker");
const std::string threadName = crazy::ThreadUtil::GetThreadName();

uint32_t value = 0x12345678;
auto networkOrder = crazy::SwapToBigOrder(value);
auto hostOrder = crazy::SwapToLittleOrder(networkOrder);
```

`Singleton<T>::Instance()` provides typed singleton access and `Noncopyable` disables copying. The `cmdline::parser` in `command_line.h` can also be used independently.

## Timestamp APIs

| API | Unit |
|-----|------|
| `GetCurrentSS()` | Seconds |
| `GetCurrentMS()` | Milliseconds |
| `GetCurrentUS()` | Microseconds |
| `GetCurrentNS()` | Nanoseconds |

Use `std::chrono::steady_clock` for elapsed-time measurements. These helpers represent absolute Unix time and are better suited to logs and protocol fields.

## String and path helpers

```cpp
const auto values =
    crazy::StringUtil::Split("a--b--c");
const std::string normalized =
    crazy::StringUtil::ToLower(
        crazy::StringUtil::Trim("  HELLO  "));

const std::string base =
    crazy::PathUtil::GetExecutableDirectory();
const std::string config =
    crazy::PathUtil::JoinPath(base, "config/app.ini");

if (!crazy::PathUtil::PathExists(config)) {
    CRAZY_SYSTEM_ERROR() << "missing config: " << config;
}
```

Recursive deletion must be constrained to an explicit allowed root. Path joins do not sanitize traversal segments.

## UUID helpers

```cpp
const std::string simple = crazy::CreateUUID();

uuids::uuid_system_generator systemGenerator;
const auto systemUuid = systemGenerator();
const std::string text = uuids::to_string(systemUuid);
```

The UUID header also provides random and name generators. Do not use a UUID as an authentication secret unless the chosen generator has suitable unpredictability.

## Command-line parser

```cpp
cmdline::parser parser;
parser.add<std::string>("host", 'h', "server host", false, "127.0.0.1");
parser.add<int>("port", 'p', "server port", false, 8080);
parser.add("verbose", 'v', "enable verbose logging");
parser.parse_check(argc, argv);

const std::string host = parser.get<std::string>("host");
const int port = parser.get<int>("port");
```

`Application` uses the same parser after daemon arguments have been removed.

## Singleton and Noncopyable

Static singleton storage makes test isolation harder. Prefer explicit dependency ownership by `Application` or Actors when possible. `Noncopyable` is a small base utility for classes that own handles or synchronization primitives.

Related: [DateTime](date-time.md), [TimeZone](time-zone.md), [URI](uri.md), [KeyValuePair](key-value-pair.md), [Daemon](daemon.md).
