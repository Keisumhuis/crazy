# TimeZone

[返回文档目录](../README.md)

`crazy::TimeZone` 表示固定 UTC 偏移，用于本地时间和 UTC 时间戳转换。

## API

| 接口 | 说明 |
|------|------|
| `TimeZone(offset, name)` | 按秒创建偏移时区 |
| `UTC()` | 创建 UTC 时区 |
| `Local()` | 读取系统本地时区 |
| `getName()` / `setName()` | 读取或设置名称 |
| `getOffset()` / `setOffset()` | 读取或设置秒偏移 |
| `toUTC(local)` | 本地时间转 UTC |
| `toLocal(utc)` | UTC 转本地时间 |
| `isValid()` | 检查偏移是否有效 |

`isValid()` 接受 UTC-12 到 UTC+14 之间的偏移，包括两个端点。构造函数和 `setOffset()` 本身不会拒绝范围外数值，因此手写偏移后应调用 `isValid()`。

## 基本转换

```cpp
#include "crazy/time_zone.h"

crazy::TimeZone utc = crazy::TimeZone::UTC();
crazy::TimeZone shanghai(8 * 60 * 60, "UTC+8");

const std::time_t now = std::time(nullptr);
const std::time_t utcNow = shanghai.toUTC(now);
const std::time_t localNow = utc.toLocal(utcNow);

CRAZY_SYSTEM_INFO()
    << shanghai.getName() << ": "
    << localNow;
```

## 与 DateTime 配合

```cpp
crazy::DateTime local(
    std::time(nullptr),
    crazy::TimeZone::Local());

crazy::DateTime utc(
    local.timestamp(),
    crazy::TimeZone::UTC());
```

`DateTime` 使用传入的时区完成日历字段与时间戳转换。跨时区业务应明确对象当前属于哪个时区。

## 夏令时边界

固定偏移不包含夏令时切换。例如纽约一年中会使用 UTC-5 和 UTC-4，单独创建一个固定偏移对象无法自动切换。需要完整 IANA 时区规则时，应引入系统时区数据库或专门的日期时间库。

## 本地时区读取

`TimeZone::Local()` 创建时根据当前系统设置计算偏移和名称。它保存的是构造时得到的固定值，不是对系统时区变化的实时引用。程序运行期间修改系统时区后，应重新创建对象。

## 有效性检查示例

```cpp
crazy::TimeZone custom(16 * 60 * 60, "invalid");
if (!custom.isValid()) {
    CRAZY_SYSTEM_ERROR() << "time zone offset is invalid";
}
```

## 存储建议

- 数据库和协议优先存储 UTC 时间戳。
- 时区偏移和名称只用于计算或展示。
- 不要用本地日期字符串判断跨时区先后。
- 服务器时区变化不应改变已持久化的绝对时间。

相关文档：[DateTime](date-time.md)、[通用工具](utilities.md)、[Selector 与定时任务](selector.md)。
