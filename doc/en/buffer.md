# Buffer

[Back to documentation index](../README.en.md)

`crazy::Buffer` is a dynamic byte buffer with separate read and write cursors for network I/O, protocol parsing, and binary assembly.

## Common operations

```cpp
#include "crazy/buffer.h"

crazy::Buffer buffer;
buffer.append("hello", 5);
buffer.append('!');

const uint32_t readable = buffer.readableCount();
std::string text(buffer.readBegin(), readable);
buffer.readed(readable);

buffer.shrink();
```

## Manual writes

```cpp
buffer.ensureWritableCount(1024);
const int32_t received =
    socket->recv(buffer.writeBegin(), buffer.writableCount());
if (received > 0) {
    buffer.written(static_cast<uint32_t>(received));
}
```

`writableCount()` reports space at the write cursor. `ensureWritableCount()` grows the allocation or compacts consumed data. `reset()` clears data and storage, while `shrink()` preserves unread data and reduces capacity.

## Cursor model

```text
[ consumed ][ readable ][ writable ]
            ^          ^
        readBegin   writeBegin
```

`readed(n)` and `written(n)` move the corresponding cursors. Callers must update cursors after recv or memcpy, otherwise later length calculations are wrong.

## Growth and pointer lifetime

```cpp
buffer.ensureWritableCount(payload.size());
std::memcpy(buffer.writeBegin(), payload.data(), payload.size());
buffer.written(static_cast<uint32_t>(payload.size()));
```

`ensureWritableCount()` may compact consumed data or grow the vector. Any `readBegin()` or `writeBegin()` pointer obtained before that call may become invalid.

## Framed read example

```cpp
bool readAvailable(crazy::Socket& socket, crazy::Buffer& buffer) {
    buffer.ensureWritableCount(4096);
    const int32_t received =
        socket.recv(buffer.writeBegin(), buffer.writableCount());
    if (received <= 0) {
        return false;
    }
    buffer.written(static_cast<uint32_t>(received));
    return true;
}

void consume(crazy::Buffer& buffer) {
    while (buffer.readableCount() >= 4) {
        uint32_t length = 0;
        std::memcpy(&length, buffer.readBegin(), sizeof(length));
        if (buffer.readableCount() < length + sizeof(length)) {
            break;
        }

        buffer.readed(sizeof(length));
        const char* payload = buffer.readBegin();
        processFrame(payload, length);
        buffer.readed(length);
    }
}
```

When data is incomplete, leave the buffer unchanged. `Decoder` implements this state machine for the framework message format.

Related: [Encoder](encoder.md), [Decoder](decoder.md), [Session](session.md), [Socket](socket.md), [MmapInterface](mmap-interface.md), [MmapVector](mmap-vector.md).
