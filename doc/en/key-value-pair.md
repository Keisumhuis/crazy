# KeyValuePair

[Back to documentation index](../README.en.md)

`crazy::KeyValuePair` stores integers, floating-point values, strings, and `DateTime` in one dynamic type and exports rows as text, CSV, JSON, or a map.

## Construct and inspect

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

## Export

```cpp
const std::string keyValueText = crazy::dataformat(row);
const std::string csv = crazy::datacsv(row);
const std::string json = crazy::toJson(row, false);
auto values = crazy::toMap(row);
```

Supported types include fixed-width unsigned/signed integers, `double`, `std::string`, and `crazy::DateTime`. A type-mismatched `get<T>()` returns an empty `optional`.

## Type inspection

The value is stored in a `std::variant`. `getValueType()` reports the active enum and `isType<T>()` tests the type.

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

`value()` converts any stored value to text for display. Use typed getters for arithmetic.

## Dates and tables

```cpp
crazy::KeyValuePair created(
    "created_at",
    crazy::DateTime::fromString("2026-09-14 10:00:00"));

if (auto date = created.getDateTime()) {
    CRAZY_SYSTEM_INFO() << date->toString();
}

std::vector<std::vector<crazy::KeyValuePair>> rows = {
    {{"name", std::string("alice")}, {"age", int32_t(18)}},
    {{"name", std::string("bob")}, {"age", int32_t(20)}},
};

const std::string csv = crazy::datacsv(rows);
const std::string json = crazy::toJson(rows.front(), true);
```

`datacsv()` preserves each row's value order. Rows with different column ordering are not reconciled. `toJson()` writes values as strings and does not infer JSON number or date types.

## Constraints

- `dataformat()` is a display format, not an escaping-safe parser.
- Duplicate keys overwrite each other in `toMap()`.
- Empty strings and missing values are not distinguished.
- Use reflected structs for complex schemas.

Related: [DateTime](date-time.md), [TimeZone](time-zone.md), [JSON](json.md), [REFLECTION](reflection.md).
