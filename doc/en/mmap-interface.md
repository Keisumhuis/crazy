# MmapInterface

[Back to documentation index](../README.en.md)

`crazy::MmapInterface` provides cross-platform memory-mapped files with open, remap, and close operations.

## API

| API | Purpose |
|-----|---------|
| `open(filepath, size)` | Open or create and map |
| `close()` | Unmap and close |
| `data()` | Address of mapped memory |
| `size()` | Mapping size |
| `filepath()` | Source path |
| `remmap(newSize)` | Re-map at a new size |

The class is noncopyable. A default-constructed object requires `open()` before access.

## Basic use

```cpp
#include "crazy/mmap/mmap.h"

{
    crazy::MmapInterface mapping;
    mapping.open("./data.bin", 4096);

    auto* bytes = static_cast<char*>(mapping.data());
    std::memcpy(bytes, "crazy", 5);

    CRAZY_SYSTEM_INFO() << mapping.filepath()
                        << ", size=" << mapping.size();
}
```

## Remapping

```cpp
crazy::MmapInterface mapping("./data.bin", 1024);
mapping.remmap(1024 * 1024);

// Call data() again; all previous pointers are invalid.
void* newAddress = mapping.data();
```

`open()` keeps the actual size when an existing file is larger than the requested size. It extends smaller files before mapping. A zero request against an empty file leaves no mapping.

## Errors and lifetime

File open, mapping, extension, and remap failures throw `std::runtime_error`. Calling `remmap()` before `open()` throws `std::logic_error`, and a zero new size throws `std::invalid_argument`.

```cpp
try {
    crazy::MmapInterface mapping("./data.bin", 4096);
    mapping.remmap(8192);
} catch (const std::exception& error) {
    CRAZY_SYSTEM_ERROR() << error.what();
}
```

Destruction and `close()` attempt to synchronize before unmapping. Critical data still needs an explicit recovery protocol.

## File format guidance

Map a versioned, fixed-width header instead of a raw C++ struct:

```text
magic(4) | version(2) | header_size(2) | payload_size(8)
```

Validate magic, version, and length before accessing external files. OS flush timing is not a transactional durability guarantee; add checksums or double-buffer commit logic when required.

Related: [MmapVector](mmap-vector.md), [FileLock](file-lock.md), [Buffer](buffer.md).
