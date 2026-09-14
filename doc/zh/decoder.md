# Decoder

[返回文档目录](../README.md)

`crazy::Decoder` 从 `Buffer` 增量解析框架二进制帧。它能在任意分包位置停止，并在收到完整报文后触发回调。

## 状态机

| 状态 | 等待内容 | 完成后的动作 |
|------|----------|--------------|
| `begin` | 8 字节长度 | 校验长度并创建消息 |
| `length` | 长度校验 | 转移到命令读取 |
| `command` | 8 字节命令 | 设置 `cmd`，计算正文长度 |
| `body` | 完整正文 | 设置 `data` 并触发完成回调 |

当数据不足时，Decoder 保留当前状态并等待下一次 `parse()`。长度小于命令字段或超过最大值时调用异常回调。

## API

| 接口 | 说明 |
|------|------|
| `setMaxMessageLength(length)` | 设置允许的最大帧长度 |
| `getMaxMessageLength()` | 读取当前上限 |
| `parse()` | 尽可能连续解析缓冲区中的完整帧 |
| `registerParseFinishCallback()` | 注册完整消息回调 |
| `registerParseExceptionCallback()` | 注册协议错误回调 |
| `reset()` | 丢弃当前解析状态 |

默认最大报文长度为 10 MiB。

## 完整示例

```cpp
#include "crazy/net/decoder.h"

crazy::Buffer receiveBuffer;
receiveBuffer.ensureWritableCount(4096);

const int32_t received =
    socket.recv(
        receiveBuffer.writeBegin(),
        receiveBuffer.writableCount());

if (received > 0) {
    receiveBuffer.written(static_cast<uint32_t>(received));

    crazy::Decoder decoder(receiveBuffer);
    decoder.setMaxMessageLength(4 * 1024 * 1024);
    decoder.registerParseFinishCallback(
        [](crazy::MessageBase::ptr message) {
            CRAZY_SYSTEM_INFO()
                << message->getCmd()
                << ": " << message->getData();
        });
    decoder.registerParseExceptionCallback([] {
        CRAZY_SYSTEM_ERROR() << "invalid binary frame";
    });
    decoder.parse();
}
```

真实使用中应复用 Decoder，而不是每次 recv 都创建新对象。上面的写法用于展示一次完整调用路径。

## 异常处理

长度异常触发后，通常应关闭连接：

```cpp
decoder.registerParseExceptionCallback([socket] {
    CRAZY_SYSTEM_ERROR() << "protocol error";
    socket->close();
});
```

如果数据源可能暂时发送非法片段，不应吞掉异常继续解析，因为先前消费的长度和命令无法恢复。

## 安全边界

- 先把 `maxMessageLength` 设置为业务允许的上限。
- 长度为 0 或小于命令字段时直接视为异常。
- 不要把网络传入的长度直接用于无上限容器分配。
- 在进入 Decoder 前处理 Buffer 的容量和背压。
- 业务正文仍需按 `cmd` 做 schema 校验。

相关文档：[Encoder](encoder.md)、[Session](session.md)、[Buffer](buffer.md)、[二进制协议](protocol.md)。
