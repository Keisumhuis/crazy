# DateTime

[返回文档目录](../README.md)

`crazy::DateTime` 提供秒级日期时间构造、解析、格式化、比较和日历计算。时区转换见 [TimeZone](time-zone.md)。

## 功能说明

DateTime 同时保存年、月、日、时、分、秒、时间戳和时区。业务代码可以按字段计算，也可以转换为 Unix 时间戳用于存储和比较。

## API

| 接口 | 说明 |
|------|------|
| `now()` | 当前本地日期时间 |
| `fromString()` | 按格式解析 |
| `toString()` | 按格式输出 |
| `timestamp()` | 返回 `time_t` |
| `weekDay()` / `yearDay()` | 星期和一年中的第几天 |
| `isLeapYear()` | 判断闰年 |
| `daysInMonth()` | 当前月份天数 |
| `addYears/Months/Days/Hours/Minutes/Seconds()` | 原地调整 |
| 比较运算符 | 按时间先后比较 |

## 解析和格式化

```cpp
#include "crazy/date_time.h"

crazy::DateTime value =
    crazy::DateTime::fromString(
        "2026-09-14 12:30:45",
        "%Y-%m-%d %H:%M:%S");

CRAZY_SYSTEM_INFO() << value.toString();
CRAZY_SYSTEM_INFO()
    << value.toString("%Y/%m/%d");
```

解析格式必须与输入一致。格式错误或字段越界的行为取决于底层时间函数，应在外部输入边界先做格式校验。

## 日期计算

```cpp
crazy::DateTime start =
    crazy::DateTime::fromString("2026-01-31 23:30:00");

crazy::DateTime end = start;
end.addMonths(1).addDays(1).addHours(2);

CRAZY_SYSTEM_INFO() << start.toString();
CRAZY_SYSTEM_INFO() << end.toString();

if (end > start) {
    CRAZY_SYSTEM_INFO() << "end is later";
}
```

`addMonths()` 会调用规范化逻辑处理不同月份天数。财务账期和自然月边界应在业务层明确定义。

## 日历字段

```cpp
const uint32_t year = start.year();
const uint32_t month = start.month();
const uint32_t day = start.day();
const crazy::WeekDay weekday = start.weekDay();
const uint32_t dayOfYear = start.yearDay();

const bool leap = crazy::DateTime::isLeapYear(year);
const uint32_t monthDays = start.daysInMonth();
```

## 时间戳和比较

```cpp
const time_t timestamp = start.timestamp();
CRAZY_SYSTEM_INFO() << timestamp;

if (start != end && start <= end) {
    CRAZY_SYSTEM_INFO() << "ordered";
}
```

时间戳适合跨时区持久化和排序。日期字符串适合展示，但不应直接用于判断先后。

## 精度和边界

- 精度为秒，不包含毫秒和纳秒。
- 时区规则由传入的 `TimeZone` 决定。
- 不提供任意格式的严格解析错误码。
- 长时间跨度计算应验证业务上限。

相关文档：[TimeZone](time-zone.md)、[通用工具](utilities.md)、[KeyValuePair](key-value-pair.md)、[Selector 与定时任务](selector.md)。
