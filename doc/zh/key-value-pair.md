# KeyValuePair

[返回文档目录](../README.md)

`crazy::KeyValuePair` 用统一的动态类型保存整数、浮点、字符串和 `DateTime`，并可导出为文本、CSV、JSON 或 map。

## 构造与读取

```cpp
#include "crazy/key_value_pair.h"

std::vector<crazy::KeyValuePair> row = {
    {"name", std::string("alice")},
    {"age", int32_t(18)},
    {"score", 95.5},
};

for (const auto& item : row) {
    CRAZY_SYSTEM_INFO() << item.key() << "=" << item.value();
}

auto age = row[1].get<int32_t>();
if (age.has_value()) {
    CRAZY_SYSTEM_INFO() << *age;
}
```

## 导出

```cpp
const std::string keyValueText = crazy::dataformat(row);
const std::string csv = crazy::datacsv(row);
const std::string json = crazy::toJson(row, false);
auto values = crazy::toMap(row);
```

支持的值类型是 `uint8_t` 到 `uint64_t`、`int8_t` 到 `int64_t`、`double`、`std::string` 和 `crazy::DateTime`。读取时如果实际类型不匹配，`get<T>()` 返回空 `optional`。

## 类型系统

`KeyValuePair` 内部使用 `std::variant`，每个实例只保存一种值。`getValueType()` 返回 `ValueType` 枚举，`isType<T>()` 用于类型判断：

```cpp
crazy::KeyValuePair value("age", int32_t(18));

CRAZY_SYSTEM_INFO() << value.key();
CRAZY_SYSTEM_INFO() << value.value();
CRAZY_SYSTEM_INFO() << static_cast<int>(value.getValueType());

if (value.isType<int32_t>()) {
    auto number = value.getInteger<int32_t>();
    CRAZY_SYSTEM_INFO() << *number;
}
```

`value()` 会把所有类型转换为字符串，适合展示；需要参与计算时使用 `get<T>()` 获取原始类型。

## 日期字段

```cpp
crazy::KeyValuePair created(
    "created_at",
    crazy::DateTime::fromString("2026-09-14 10:00:00"));

if (auto date = created.getDateTime()) {
    CRAZY_SYSTEM_INFO() << date->toString();
}
```

`DateTime` 的字符串输出使用默认格式。跨时区展示时应在写入前统一时间基准。

## 表格导出

```cpp
std::vector<std::vector<crazy::KeyValuePair>> rows = {
    {{"name", std::string("alice")}, {"age", int32_t(18)}},
    {{"name", std::string("bob")}, {"age", int32_t(20)}},
};

const std::string csv = crazy::datacsv(rows);
const std::string json = crazy::toJson(rows.front(), true);
auto map = crazy::toMap(rows.front());
```

`datacsv()` 只输出值并保持列顺序。如果每一行的列顺序不同，结果不会自动对齐。`toJson()` 当前把值作为字符串输出，不负责日期或数字的 JSON 类型推断。

## 使用边界

- `dataformat()` 适合键值文本，不适合转义后再解析。
- `toJson()` 对特殊字符的转义能力有限，复杂 JSON 应使用 `json` 模块。
- 重复 key 在 `toMap()` 中后值覆盖前值。
- 空值与空字符串的语义没有额外区分。
- 需要复杂 schema 时应直接定义结构体并使用 `REFLECTION`。

相关文档：[DateTime](date-time.md)、[TimeZone](time-zone.md)、[JSON 序列化](json.md)、[REFLECTION](reflection.md)。
