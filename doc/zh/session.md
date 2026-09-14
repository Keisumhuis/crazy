# Session

[返回文档目录](../README.md)

`crazy::Session` 表示一条已建立的网络连接。它组合 Socket、收发缓冲区、Encoder、Decoder、心跳时间和业务回调。

## 功能边界

Session 负责：

- 从 Socket 读取数据并写入接收缓冲区。
- 调用 Decoder 拆出完整消息。
- 缓存待发送消息并在可写时继续发送。
- 调用连接、消息和断开回调。
- 记录最后心跳时间。

Session 不创建线程，也不执行重连。它依赖外部 `Selector` 注册读事件和动态写事件。

## API

| 接口 | 说明 |
|------|------|
| `getSessionId()` | 获取唯一会话标识 |
| `socket()` | 获取底层 socket 描述符 |
| `close()` | 关闭会话 |
| `onReadEvent()` / `onWriteEvent()` | Selector 事件入口 |
| `sendMessage()` | 编码并排队发送消息 |
| `lastHeartbeatTimestamp()` | 获取最后心跳时间 |
| `remoteAddress()` | 获取客户端地址 |

## 回调

| 回调 | 触发条件 |
|------|----------|
| `registerMessageCallback()` | Decoder 得到完整消息 |
| `registerDisconnectCallback()` | 连接关闭 |
| `registerSelectEventRegisterCallback()` | Session 需要注册写事件 |
| `registerSelectEventUnregisterCallback()` | 写缓冲清空或会话关闭 |

## 完整接入示例

```cpp
#include "crazy/net/session.h"

class SessionServer {
public:
    void accept(crazy::Socket::ptr socket) {
        auto session = std::make_shared<crazy::Session>(socket);

        session->registerSelectEventRegisterCallback(
            [this](int32_t fd, crazy::SelectorEventType type,
                   std::function<void()> callback) {
                selector_.registerEvent(fd, type, std::move(callback));
            });

        session->registerSelectEventUnregisterCallback(
            [this](int32_t fd, crazy::SelectorEventType type) {
                selector_.unregisterEvent(fd, type);
            });

        session->registerMessageCallback(
            [session](crazy::MessageBase::ptr message) {
                auto response = message->createResponse();
                response->setData("echo: " + message->getData());
                session->sendMessage(response);
            });

        session->registerDisconnectCallback([this, session] {
            sessions_.erase(session->getSessionId());
        });

        sessions_[session->getSessionId()] = session;
        selector_.registerEvent(
            socket->socket(),
            crazy::SelectorEventType::read,
            [session] { session->onReadEvent(); });
    }

private:
    crazy::Selector selector_;
    std::map<uint64_t, crazy::Session::ptr> sessions_;
};
```

## 发送和背压

`sendMessage()` 先编码到发送缓冲区，然后直接尝试写 Socket；未写完时注册可写事件继续发送。慢客户端会使缓冲区增长。

生产环境应增加发送队列上限：

```cpp
if (sendBuffer.readableCount() > 8 * 1024 * 1024) {
    session->close();
}
```

上面的检查应放在可访问发送统计的业务代码或扩展接口中。当前 Session 没有公开发送缓冲区大小，实际项目可通过连接级限流或消息速率配额控制。

## 心跳

`lastHeartbeatTimestamp()` 由收到消息时更新。ServiceActor 等上层组件使用该值关闭超时会话。长时间阻塞会话线程会导致心跳事件和消息处理同时延迟。

## 生命周期和并发

- Session 的读写回调应在所属事件线程执行。
- 不要在多个线程同时操作同一个 Session。
- 断开回调中不要继续持有无效 Session 的业务引用。
- Session 从容器移除前应关闭 Socket 并取消事件。

相关文档：[Encoder](encoder.md)、[Decoder](decoder.md)、[ServiceActor](service-actor.md)、[ClientActor](client-actor.md)、[Buffer](buffer.md)。
