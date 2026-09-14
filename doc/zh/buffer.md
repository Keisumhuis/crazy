# Buffer

[返回文档目录](../README.md)

`crazy::Buffer` 是读写游标分离的动态字节缓冲区，用于网络 IO、协议解析和二进制数据拼接。

## 常用操作

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

## 手工写入

```cpp
buffer.ensureWritableCount(1024);
const int32_t received =
    socket->recv(buffer.writeBegin(), buffer.writableCount());
if (received > 0) {
    buffer.written(static_cast<uint32_t>(received));
}
```

`writableCount()` 返回尾部剩余空间；空间不足时 `ensureWritableCount()` 会扩容或先迁移已读数据。`reset()` 清空数据并释放存储，`shrink()` 在保留未读数据的同时压缩容量。

## 游标模型

Buffer 内部维护可读起点和可写起点，`readableCount()` 与 `writableCount()` 都由两者及底层容量推导：

```text
[ 已读取空间 ][ 可读数据 ][ 可写空间 ]
               ^          ^
            readBegin   writeBegin
```

`readed(n)` 向后移动读游标，`written(n)` 向后移动写游标。调用者必须在 recv 或 memcpy 后正确更新游标，否则后续读写长度会错误。

## 容量增长策略

`ensureWritableCount(count)` 会优先复用已读取空间；尾部空间不足时扩容底层 vector。扩容后旧的 `readBegin()` 和 `writeBegin()` 指针可能失效，使用指针时不要跨扩容调用保存。

```cpp
buffer.ensureWritableCount(payload.size());
std::memcpy(buffer.writeBegin(), payload.data(), payload.size());
buffer.written(static_cast<uint32_t>(payload.size()));
```

## 网络读取完整示例

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
        handle(payload, length);
        buffer.readed(length);
    }
}
```

这类“先读长度、再等正文”的逻辑必须在数据不足时保持原状态。Decoder 已实现类似状态机。

## 参数与容量

默认构造容量为 4 KiB。大报文频繁扩容会带来复制和内存峰值，可在构造时传入预估值：

```cpp
crazy::Buffer largeBuffer(1024 * 1024);
```

`shrink()` 适合在连接空闲或处理完大消息后降低内存占用，但频繁调用会引入额外复制。不要对仍被外部指针引用的数据执行扩容、reset 或 shrink。

相关文档：[Encoder](encoder.md)、[Decoder](decoder.md)、[Session](session.md)、[Socket](socket.md)、[MmapInterface](mmap-interface.md)、[MmapVector](mmap-vector.md)。
