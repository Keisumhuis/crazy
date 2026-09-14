# Encoder

[返回文档目录](../README.md)

`crazy::Encoder` 把 `MessageBase` 序列化为框架内部二进制帧，并追加到调用方提供的 `Buffer`。

## 帧格式

| 顺序 | 大小 | 内容 |
|------|------|------|
| 1 | 8 字节 | `cmd + data` 的总长度 |
| 2 | 8 字节 | `cmd` |
| 3 | `size - 8` 字节 | `data` |

当前实现直接复制 `uint64_t` 的本机字节序。相同架构和字节序的节点之间使用没有问题；跨语言或跨端序通信应定义独立协议。

## API

| 接口 | 说明 |
|------|------|
| `Encoder(Buffer&)` | 绑定输出缓冲区 |
| `stringify(MessageBase::ptr)` | 追加一条完整消息 |
| `reset()` | 预留重置入口，当前实现为空 |

Encoder 不拥有 Buffer，也不管理 Socket。调用方应在发送完成后决定是否重置或复用缓冲区。

## 完整编码示例

```cpp
#include "crazy/net/encoder.h"

crazy::Buffer sendBuffer;
crazy::Encoder encoder(sendBuffer);

auto request = std::make_shared<crazy::MessageBase>();
request->setCmd(1001);
request->setData("hello");
encoder.stringify(request);

const char* data = sendBuffer.readBegin();
const uint32_t length = sendBuffer.readableCount();

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

## 批量编码

```cpp
crazy::Buffer buffer;
crazy::Encoder encoder(buffer);

for (const auto& message : messages) {
    encoder.stringify(message);
}
```

Decoder 可以连续解析多条消息，因此批量编码适合一次网络发送。但仍要考虑发送缓冲区上限，避免无限累积。

## 注意事项

- `data` 可以包含任意字节，Encoder 不会进行 JSON 或 UTF-8 校验。
- `cmd` 和长度使用本机字节序。
- Buffer 扩容后，先取得的可读指针可能失效。
- 编码层不提供压缩和加密。
- 消息长度必须与 Decoder 的最大报文长度约束一致。

相关文档：[Decoder](decoder.md)、[Session](session.md)、[MessageBase](message.md)、[Buffer](buffer.md)。
