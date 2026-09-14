# ClientActor

[返回文档目录](../README.md)

`crazy::ClientActor` 是管理单个服务端连接的客户端 Actor，负责建连、断线、消息接收和心跳。

## 配置

```ini
[client]
address = "127.0.0.1"
port = 9090
heartbeat_interval = 5000
```

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `address` | `127.0.0.1` | 服务端地址 |
| `port` | `8901` | 服务端端口 |
| `heartbeat_interval` | `5000` | 心跳间隔，毫秒 |

## 启动客户端

```cpp
#include "crazy.h"

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<crazy::ClientActor>("client");
    app.exec();
}
```

## 处理服务端消息

```cpp
class GatewayClient final : public crazy::ClientActor {
public:
    using crazy::ClientActor::ClientActor;

protected:
    void onRecvMessage(crazy::MessageBase::ptr message) {
        CRAZY_SYSTEM_INFO()
            << "server cmd=" << message->getCmd()
            << ", data=" << message->getData();
    }

    void onConnected() {
        CRAZY_SYSTEM_INFO() << "connected";
    }

    void onDisconnected() {
        CRAZY_SYSTEM_WARN() << "disconnected";
    }
};
```

`onRecvMessage()`、`onConnected()` 和 `onDisconnected()` 当前是基类的受保护实现，不是虚接口。业务扩展时应先确认当前版本头文件的扩展点；如果无法重写，可以在 `handleMessageBase()` 中处理消息，并使用连接状态消息完成生命周期切换。

## 连接和心跳

`ClientActor` 创建 `Connection` 并使用 Selector 管理网络事件。心跳消息由定时器触发，断线后连接状态会更新，但当前 Actor 不自动指数退避重连。需要长期稳定连接时，应实现有上限的重连策略并记录失败原因。

## 客户端状态机

```text
disconnected -> connecting -> connected -> disconnecting -> disconnected
```

业务状态最好只在此状态机允许的阶段发送消息。未连接时调用发送接口应被拒绝或排队，而不是丢弃后无记录。

## 多连接场景

一个 `ClientActor` 对应一组配置和一个连接。需要连接多个服务端时，应注册多个名称和配置段，或者实现自己的连接管理器。连接名称同样决定配置段名。

## 部署建议

- 服务端地址和端口必须可通过配置覆盖。
- 区分首次连接失败和运行中断线。
- 监控重连次数、最后成功心跳和待发送消息数量。
- 长期断线时采用退避策略，避免形成连接风暴。

相关文档：[ServiceActor](service-actor.md)、[Session](session.md)、[Socket](socket.md)、[配置管理](config.md)。
