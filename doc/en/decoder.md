# Decoder

[Back to documentation index](../README.en.md)

`crazy::Decoder` incrementally parses framework frames from a `Buffer`. It can stop at any partial boundary and calls completion or exception callbacks when appropriate.

## State machine

| State | Waiting for | Action |
|-------|-------------|--------|
| `begin` | 8-byte size | Validate and allocate a message |
| `length` | Size validation | Move to command parsing |
| `command` | 8-byte command | Set `cmd` and body length |
| `body` | Complete body | Set `data` and invoke completion |

Insufficient data leaves the parser state unchanged until the next `parse()`.

## API

| API | Purpose |
|-----|---------|
| `setMaxMessageLength()` | Set the maximum frame size |
| `getMaxMessageLength()` | Read the limit |
| `parse()` | Parse every complete buffered frame |
| `registerParseFinishCallback()` | Receive complete messages |
| `registerParseExceptionCallback()` | Observe invalid framing |
| `reset()` | Discard parser state |

The default maximum is 10 MiB.

## Complete parse

```cpp
#include "crazy/net/decoder.h"

crazy::Buffer receiveBuffer;
receiveBuffer.ensureWritableCount(4096);

const int32_t received =
    socket.recv(receiveBuffer.writeBegin(),
                receiveBuffer.writableCount());

if (received > 0) {
    receiveBuffer.written(static_cast<uint32_t>(received));

    crazy::Decoder decoder(receiveBuffer);
    decoder.setMaxMessageLength(4 * 1024 * 1024);
    decoder.registerParseFinishCallback(
        [](crazy::MessageBase::ptr message) {
            CRAZY_SYSTEM_INFO() << message->getCmd()
                                << ": " << message->getData();
        });
    decoder.registerParseExceptionCallback([] {
        CRAZY_SYSTEM_ERROR() << "invalid binary frame";
    });
    decoder.parse();
}
```

Production code should reuse one decoder with the connection rather than constructing one per receive.

## Error handling and security

An invalid size or command consumes framing state that cannot be reconstructed. Close the connection after a protocol exception. Set `maxMessageLength` before accepting external traffic and validate application fields after decoding.

Related: [Encoder](encoder.md), [Session](session.md), [Buffer](buffer.md), [Binary protocol](protocol.md).
