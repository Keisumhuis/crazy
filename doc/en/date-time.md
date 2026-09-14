# DateTime

[Back to documentation index](../README.en.md)

`crazy::DateTime` provides second-resolution date construction, parsing, formatting, comparison, and calendar arithmetic. Time-zone conversion is documented in [TimeZone](time-zone.md).

## API

| API | Purpose |
|-----|---------|
| `now()` | Current local time |
| `fromString()` | Parse using a format |
| `toString()` | Format |
| `timestamp()` | Return `time_t` |
| `weekDay()` / `yearDay()` | Calendar fields |
| `isLeapYear()` / `daysInMonth()` | Calendar calculations |
| `addYears/Months/Days/Hours/Minutes/Seconds()` | In-place arithmetic |
| Comparison operators | Chronological comparison |

## Parse and format

```cpp
#include "crazy/date_time.h"

crazy::DateTime value =
    crazy::DateTime::fromString(
        "2026-09-14 12:30:45",
        "%Y-%m-%d %H:%M:%S");

CRAZY_SYSTEM_INFO() << value.toString();
CRAZY_SYSTEM_INFO() << value.toString("%Y/%m/%d");
```

The format must match the source string. Validate untrusted input before parsing.

## Calendar arithmetic

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

Month arithmetic normalizes month lengths. Define explicit business rules for billing periods and natural-month boundaries.

## Timestamps and comparison

```cpp
const time_t timestamp = start.timestamp();
const crazy::WeekDay weekday = start.weekDay();
const uint32_t dayOfYear = start.yearDay();
const bool leap = crazy::DateTime::isLeapYear(start.year());

if (start != end && start <= end) {
    CRAZY_SYSTEM_INFO() << "ordered";
}
```

Timestamps are better for persistence and cross-zone ordering. Formatted local strings are for display.

## Limits

- Second resolution only.
- Time-zone behavior depends on the supplied `TimeZone`.
- There is no detailed parse-error code.
- Validate large business date ranges before arithmetic.

Related: [TimeZone](time-zone.md), [General utilities](utilities.md), [KeyValuePair](key-value-pair.md), [Selector and timers](selector.md).
