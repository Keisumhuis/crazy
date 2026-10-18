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
| `address` | `0.0.0.0` | 绑定地址；填 IPv4 字面量走 IPv4，填 IPv6 字面量（`::1`、`::`、`[::1]`）走 IPv6 |
| `port` | `8901` | 监听端口 |
| `heartbeat_interval` | `5000` | 心跳检查间隔，毫秒 |

`address` 缺省 `0.0.0.0` 表示「仅 IPv4」，行为与旧版本一致。写成 IPv6 字面量即可监听 IPv6：

```ini
[service]
address = "::1"   ; 仅 IPv6 回环
port = 9090
```

```ini
[service]
address = "::"    ; 仅 IPv6 全地址（不隐式双栈）
port = 9090
```

注意：`ServiceActor` 的配置层**不暴露双栈开关**，配置 `::` 得到的是「仅 IPv6」。需要「同一端口同时接受 IPv4 与 IPv6」的调用方应绕过配置、直接调用 `Socket::listen(port, "::", true)`，详见 [Socket 的 IPv6 与双栈](socket.md)。

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

`ServiceActor` 只提供基础连接管理，不包含连接数配额、认证和限流。TLS 的实现位于 **Socket 层**（`crazy::TlsSocket`，见 [Socket](socket.md) 与 [HTTP 服务端](http-server.md) 的 HTTPS 小节），因此 `Session` / `Connection` / `ServiceActor` / `TelnetServiceActor` 将来可以在**不改业务逻辑**的前提下复用同一抽象；但**本次并未给它们接线**，这些服务当前的连接仍是明文。生产服务应在网络层或业务层补充上述能力，并监控会话数、消息队列和慢客户端发送缓冲区。

相关文档：[ClientActor](client-actor.md)、[Session](session.md)、[Socket](socket.md)、[配置管理](config.md)。
