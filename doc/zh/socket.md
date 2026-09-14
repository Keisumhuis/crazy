# TCP Socket

[返回文档目录](../README.md)

`crazy::Socket` 封装 IPv4 TCP 套接字，可用于同步客户端、简单服务端和自定义事件循环。本页只描述 TCP Socket，本机通信见 [LocalSocket](local-socket.md)。

## 功能说明

`Socket` 继承 `SocketInterface`，负责创建、绑定、监听、连接、收发和关闭描述符。它不负责消息分帧、自动重连、心跳或并发调度。

## API

| 接口 | 返回值 | 说明 |
|------|--------|------|
| `listen(port, address)` | `bool` | 绑定并监听 |
| `connect(address, port)` | `bool` | 建立 TCP 连接 |
| `accept()` | `Socket::ptr` | 接受客户端并返回新 Socket |
| `send(data, length, flags)` | `int32_t` | 发送字节，可能短写 |
| `recv(data, length, flags)` | `int32_t` | 接收字节，0 表示对端关闭 |
| `active()` | `bool` | 本地句柄是否有效 |
| `close()` | `void` | 关闭并释放描述符 |
| `getOption()` / `setOption()` | `bool` | 操作底层套接字选项 |
| `remoteAddress()` / `localAddress()` | `const std::string&` | 地址信息 |

## 服务端与客户端

```cpp
#include "crazy/net/socket.h"

int main() {
    crazy::Socket server;
    if (!server.listen(8080, "0.0.0.0")) {
        CRAZY_SYSTEM_ERROR() << "listen failed";
        return 1;
    }

    auto client = server.accept();
    if (!client) {
        return 1;
    }

    char buffer[1024] = {};
    const int32_t received =
        client->recv(buffer, sizeof(buffer));
    if (received > 0) {
        client->send(buffer, static_cast<size_t>(received));
    }
}
```

客户端：

```cpp
crazy::Socket socket;
if (!socket.connect("127.0.0.1", 8080)) {
    CRAZY_SYSTEM_ERROR() << "connect failed";
    return;
}

const std::string request = "ping";
socket.send(request.data(), request.size());
```

## 处理短写

`send()` 返回本次实际发送的字节数。必须循环发送剩余数据：

```cpp
bool sendAll(crazy::Socket& socket, const std::string& data) {
    const char* current = data.data();
    size_t remaining = data.size();

    while (remaining > 0) {
        const int32_t sent =
            socket.send(current, remaining);
        if (sent <= 0) {
            return false;
        }
        current += sent;
        remaining -= static_cast<size_t>(sent);
    }
    return true;
}
```

`Session` 和 `Connection` 已内置发送缓冲，自定义协议使用这些封装可以减少短写处理。

## 阻塞与事件驱动

本页示例使用阻塞收发，适合工具和单元测试。服务端网络层应在 `Selector` 中注册可读和可写事件：

```cpp
selector.registerEvent(
    client->socket(),
    crazy::SelectorEventType::read,
    [client] { handleRead(client); });
```

阻塞 `accept()` 或 `recv()` 会占住 Actor 线程，不应直接用于高并发业务 Actor。

## 连接生命周期

1. 创建 Socket。
2. 监听或连接。
3. 收发数据。
4. 根据返回值、字节数和空闲时间判断断开。
5. 关闭并释放对象。

`active()` 只说明本地句柄有效。对端可能已经消失，最终仍要通过读写结果或应用层心跳确认连接状态。

## 套接字选项

模板版 `getOption<T>()` 和 `setOption<T>()` 可读写底层平台选项：

```cpp
int enabled = 1;
socket.setOption(SOL_SOCKET, SO_REUSEADDR, enabled);
```

选项的常量来自平台 Socket 头文件。跨平台代码应避免在业务层直接依赖平台专属选项。

相关文档：[LocalSocket](local-socket.md)、[Selector 与定时任务](selector.md)、[Encoder](encoder.md)、[Decoder](decoder.md)、[Session](session.md)。
