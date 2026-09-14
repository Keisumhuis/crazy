# 通用工具

[返回文档目录](../README.md)

通用工具模块提供时间戳、UUID、字符串、路径、线程名、端序转换、单例、不可拷贝基类、字节顺序和命令行解析。

## 时间戳与 UUID

```cpp
#include "crazy/utils.h"

const uint64_t seconds = crazy::GetCurrentSS();
const uint64_t milliseconds = crazy::GetCurrentMS();
const uint64_t microseconds = crazy::GetCurrentUS();
const uint64_t nanoseconds = crazy::GetCurrentNS();
const std::string id = crazy::CreateUUID();
```

## 字符串与路径

```cpp
const auto parts = crazy::StringUtil::Split("a,b,c", ",");
const std::string text = crazy::StringUtil::Trim("  hello  ");
const std::string upper = crazy::StringUtil::ToUpper("crazy");

const std::string exe = crazy::PathUtil::GetExecutablePath();
const std::string dir = crazy::PathUtil::GetExecutableDirectory();
const std::string file = crazy::PathUtil::JoinPath(dir, "app.ini");
crazy::PathUtil::CreateDirectories("./logs/app");
```

## 线程名与端序

```cpp
#include "crazy/byte_order.h"

crazy::ThreadUtil::SetThreadName("worker");
const std::string threadName = crazy::ThreadUtil::GetThreadName();

uint32_t value = 0x12345678;
auto networkOrder = crazy::SwapToBigOrder(value);
auto hostOrder = crazy::SwapToLittleOrder(networkOrder);
```

`Singleton<T>::Instance()` 提供按类型创建的单例，`Noncopyable` 可禁止派生类复制。`command_line.h` 中的 `cmdline::parser` 也可以单独用于自定义命令行程序。

## 时间工具

| 接口 | 单位 | 返回值 |
|------|------|--------|
| `GetCurrentSS()` | 秒 | 当前 Unix 时间戳 |
| `GetCurrentMS()` | 毫秒 | 当前时间戳 |
| `GetCurrentUS()` | 微秒 | 当前时间戳 |
| `GetCurrentNS()` | 纳秒 | 当前时间戳 |

需要单调时间测量时应使用 `std::chrono::steady_clock`，这些接口适合与外部协议和日志中的绝对时间对应。

## StringUtil 与 PathUtil

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

路径工具包含创建、删除、复制、遍历、扩展名和可用空间接口。涉及递归删除时，调用方必须先把路径限制在允许的根目录内。

## UUID 的两种入口

```cpp
const std::string simple = crazy::CreateUUID();

uuids::uuid_system_generator systemGenerator;
const auto systemUuid = systemGenerator();
const std::string text = uuids::to_string(systemUuid);
```

`uuid.h` 还包含随机生成器和命名生成器，适合需要 v4/v5 语义的场景。不要把 UUID 当密码或不可预测 Token，随机性和版本语义应按具体生成器判断。

## 命令行解析

```cpp
cmdline::parser parser;
parser.add<std::string>("host", 'h', "server host", false, "127.0.0.1");
parser.add<int>("port", 'p', "server port", false, 8080);
parser.add("verbose", 'v', "enable verbose logging");
parser.parse_check(argc, argv);

const std::string host = parser.get<std::string>("host");
const int port = parser.get<int>("port");
```

`Application` 也使用同一个解析器处理 `-s`、`-d` 和帮助选项，但业务参数会先经过 `Daemon::ParseArguments()` 过滤。

## 单例与不可拷贝

```cpp
class Registry final : public crazy::Singleton<Registry> {
public:
    void add(const std::string& name) {
        names_.push_back(name);
    }

private:
    std::vector<std::string> names_;
};

Registry::Instance().add("service");
```

单例的生命周期由静态存储决定，测试中不易重置。业务服务更推荐通过 Application、Actor 或显式依赖传递对象。

相关文档：[DateTime](date-time.md)、[TimeZone](time-zone.md)、[URI](uri.md)、[KeyValuePair](key-value-pair.md)、[Daemon](daemon.md)。
