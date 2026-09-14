# MmapVector

[Back to documentation index](../README.en.md)

`crazy::MmapVector<T>` builds a persistent contiguous array on top of `MmapInterface`. The data can be reopened by another process.

## Type requirement

`T` must be trivially copyable:

```cpp
static_assert(std::is_trivially_copyable_v<T>);
```

Do not store `std::string`, `std::vector`, or other owning types directly. Use fixed-size POD records or offset tables.

## File layout

| Region | Content |
|--------|---------|
| Header | `uint64_t size`, `uint64_t capacity` |
| Data | Contiguous `T` elements |

## Basic use

```cpp
#include "crazy/mmap/mmap_vector.h"

crazy::MmapVector<uint64_t> values("./values.bin", 4);
values[0] = 10;
values[1] = 20;
values[3] = 40;

for (uint64_t value : values) {
    CRAZY_SYSTEM_INFO() << value;
}

if (auto* value = values.at(2)) {
    CRAZY_SYSTEM_INFO() << *value;
}
```

Writing index 3 advances `size()` to 4. Index 2 is included in the logical range but was not explicitly initialized.

## Growth and sharing

Out-of-range writes may remap the file and invalidate all pointers. Multi-process writers require an external lock and a protocol for readers to reopen after growth. Validate header size and file length before trusting an external file.

For a new non-empty file, the header starts with size zero and the requested capacity. Existing file headers retain their stored capacity.

## Safe access

```cpp
void append(crazy::MmapVector<uint64_t>& values, uint64_t value) {
    const uint64_t index = values.size();
    values[index] = value;
}

std::optional<uint64_t> find(
    crazy::MmapVector<uint64_t>& values,
    uint64_t index) {
    if (auto* value = values.at(index)) {
        return *value;
    }
    return std::nullopt;
}
```

Use current `size()` for append semantics and `at()` for checked reads. A damaged header must not be trusted because overflow in size calculations can invalidate the mapping.

Related: [MmapInterface](mmap-interface.md), [FileLock](file-lock.md), [Binary protocol](protocol.md).
