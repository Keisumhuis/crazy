# LocalSocket

[返回文档目录](../README.md)

`crazy::LocalSocket` 提供同机进程间通信，Application 的本地命令行服务也使用这一层。

## 适用场景

LocalSocket 不占用 TCP 端口，适合：

- 同一台主机上的管理命令。
- 权限由文件系统控制的服务间通信。
- 不希望暴露到网络的短连接控制接口。
- Windows 和 Linux 上共享同一套业务接口。

它不适合跨主机通信，也不提供自动认证、权限和消息边界。进程仍应定义长度前缀或使用 `Session` 编解码。

## API

| 接口 | 说明 |
|------|------|
| `listen(address)` | 在本地路径上监听 |
| `connect(address)` | 连接已有本地 Socket |
| `accept()` | 接受一个客户端 |
| `send()` / `recv()` | 继承自 `SocketInterface` 的收发接口 |
| `active()` / `close()` | 状态查询和释放 |

## 服务端与客户端

```cpp
#include "crazy/net/local_socket.h"

#include <thread>

int main() {
    const std::string address = "crazy-demo.socket";

    std::thread client([address] {
        crazy::LocalSocket socket;
        if (socket.connect(address)) {
            const std::string message = "ping";
            socket.send(message.data(), message.size());
        }
    });

    crazy::LocalSocket server;
    if (!server.listen(address)) {
        return 1;
    }

    auto connection = server.accept();
    char buffer[128] = {};
    const int32_t received =
        connection->recv(buffer, sizeof(buffer));
    if (received > 0) {
        CRAZY_SYSTEM_INFO()
            << std::string(buffer, static_cast<size_t>(received));
    }

    client.join();
}
```

## Application 命令 Socket

`Application` 会监听 `<可执行文件名>.command`。当程序使用 `-s` 启动时，它作为客户端连接该路径并打印响应：

```bash
./your_app -s application@time
```

命令 Socket 的路径由可执行文件名决定。同一目录下运行多个实例时，应使用不同可执行文件名或调整部署目录。

## 路径与清理

Linux 和 Windows 的 AF_UNIX 路径长度不同。部署到浅层目录可以降低路径过长风险。进程异常退出可能留下路径文件，下次启动逻辑需要先确认它是否仍被其他进程使用，不能盲目删除。

## 安全建议

- 使用文件系统权限限制可连接用户。
- 不要假设本地调用方已经可信。
- 为消息设置长度上限和命令白名单。
- 管理 Socket 与普通业务端口采用不同的权限策略。

相关文档：[Socket](socket.md)、[Application](application.md)、[Encoder](encoder.md)、[Decoder](decoder.md)。
