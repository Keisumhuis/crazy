# TimeZone

[Back to documentation index](../README.en.md)

`crazy::TimeZone` represents a fixed UTC offset for converting local time and UTC timestamps.

## API

| API | Purpose |
|-----|---------|
| `TimeZone(offset, name)` | Construct a fixed offset |
| `UTC()` / `Local()` | Standard zones |
| `getName()` / `setName()` | Zone name |
| `getOffset()` / `setOffset()` | Offset in seconds |
| `toUTC()` / `toLocal()` | Convert a time value |
| `isValid()` | Validate the zone |

The valid range is UTC-12 through UTC+14. Constructors and `setOffset()` do not reject values outside that range, so call `isValid()` after supplying a custom offset.

## Basic conversion

```cpp
#include "crazy/time_zone.h"

crazy::TimeZone utc = crazy::TimeZone::UTC();
crazy::TimeZone shanghai(8 * 60 * 60, "UTC+8");

const std::time_t now = std::time(nullptr);
const std::time_t utcNow = shanghai.toUTC(now);
const std::time_t localNow = utc.toLocal(utcNow);
```

## DateTime integration

```cpp
crazy::DateTime local(
    std::time(nullptr),
    crazy::TimeZone::Local());

crazy::DateTime utc(
    local.timestamp(),
    crazy::TimeZone::UTC());
```

## Daylight-saving limits

A fixed offset cannot model regions that change offset during the year. Use an IANA time-zone database for daylight-saving rules. Persist UTC and use local zones only for calculation or display.

## Local snapshot

`TimeZone::Local()` calculates a fixed offset and name when constructed. It does not track later system-zone changes. Recreate the object after changing system time-zone settings.

```cpp
crazy::TimeZone custom(16 * 60 * 60, "invalid");
if (!custom.isValid()) {
    CRAZY_SYSTEM_ERROR() << "time zone offset is invalid";
}
```

Related: [DateTime](date-time.md), [General utilities](utilities.md), [Selector and timers](selector.md).
