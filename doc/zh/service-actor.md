# ServiceActor

[返回文档目录](../README.md)

`crazy::ServiceActor` 是 TCP 服务端 Actor，负责监听、接受连接、维护 Session、心跳检查和业务消息入口。

## 配置

```ini
[service]
address = "0.0.0.0"
port = 9090
heartbeat_interval = 5000
```

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `address` | `0.0.0.0` | 绑定地址 |
| `port` | `8901` | 监听端口 |
| `heartbeat_interval` | `5000` | 心跳检查间隔，毫秒 |

注册名称就是配置段名称：

```cpp
crazy::Application app(argc, argv);
app.registerActor<crazy::ServiceActor>("service");
app.exec();
```

## 生命周期

1. `initConfig()` 读取本 Actor 名称对应的配置段。
2. `initService()` 创建 `Acceptor` 并开始监听。
3. `onAccept()` 接受连接并创建 `Session`。
4. Session 收到消息后调用 `onMessage()`。
5. `handleMessageBase()` 处理业务消息。
6. 心跳定时器检查 Session 活跃时间。
7. 断开后从 `sessions_` 中移除会话。

## 自定义服务端

```cpp
class GameService final : public crazy::ServiceActor {
public:
    using crazy::ServiceActor::ServiceActor;

protected:
    void handleMessageBase(crazy::MessageBase::ptr message) override {
        if (message->getCmd() == 1001) {
            auto response = message->createResponse();
            response->setData("accepted: " + message->getData());
            sendMessage(response);
            return;
        }

        crazy::ServiceActor::handleMessageBase(message);
    }
};

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<GameService>("game");
    app.exec();
}
```

## 会话生命周期

每个连接由 `Session` 和 `sessionId` 标识。消息通过 `createResponse()` 保留 session ID，响应回到服务端后路由到正确连接。业务层若维护玩家对象，应在连接建立和断开时同步创建、销毁绑定状态。

## 心跳

`heartbeat_interval` 定义检查周期，服务端根据最后心跳时间判断连接是否超时。回调执行时间过长可能延后检查，因此不要在 ServiceActor 线程中执行阻塞数据库或文件操作。

## 扩缩容与限流

`ServiceActor` 只提供基础连接管理，不包含连接数配额、认证、限流和 TLS。生产服务应在网络层或业务层补充这些能力，并监控会话数、消息队列和慢客户端发送缓冲区。

相关文档：[ClientActor](client-actor.md)、[Session](session.md)、[Socket](socket.md)、[配置管理](config.md)。
