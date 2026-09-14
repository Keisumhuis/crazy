# Encoder

[Back to documentation index](../README.en.md)

`crazy::Encoder` serializes a `MessageBase` into the framework's internal binary frame and appends it to a caller-owned `Buffer`.

## Frame format

| Offset | Size | Content |
|--------|------|---------|
| 0 | 8 bytes | Total size of `cmd + data` |
| 8 | 8 bytes | Command |
| 16 | `size - 8` | Message data |

The current implementation uses native byte order. Use a versioned protocol with explicit endian conversion for heterogeneous systems.

## API

| API | Purpose |
|-----|---------|
| `Encoder(Buffer&)` | Bind an output buffer |
| `stringify(MessageBase::ptr)` | Append one message |
| `reset()` | Reserved reset hook, currently empty |

The encoder does not own the buffer or socket. The caller decides when to send, consume, or reset buffered bytes.

## Complete encode

```cpp
#include "crazy/net/encoder.h"

crazy::Buffer sendBuffer;
crazy::Encoder encoder(sendBuffer);

auto request = std::make_shared<crazy::MessageBase>();
request->setCmd(1001);
request->setData("hello");
encoder.stringify(request);

const char* data = sendBuffer.readBegin();
uint32_t length = sendBuffer.readableCount();

while (length > 0) {
    const int32_t sent =
        socket.send(data, static_cast<size_t>(length));
    if (sent <= 0) {
        break;
    }
    data += sent;
    length -= static_cast<uint32_t>(sent);
}
```

## Batch encoding

```cpp
crazy::Buffer buffer;
crazy::Encoder encoder(buffer);

for (const auto& message : messages) {
    encoder.stringify(message);
}
```

The decoder can parse multiple frames from one buffer. Bound the output buffer to avoid unbounded accumulation.

## Constraints

- `data` is an arbitrary byte string; no UTF-8 validation is performed.
- The length and command use native byte order.
- Buffer growth can invalidate previously captured pointers.
- Encoding does not compress or encrypt.
- Frame size must fit the receiver's decoder limit.

Related: [Decoder](decoder.md), [Session](session.md), [MessageBase](message.md), [Buffer](buffer.md).
