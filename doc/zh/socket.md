# TCP Socket

[返回文档目录](../README.md)

`crazy::Socket` 封装 TCP 套接字，**默认为仅 IPv4**，也可显式监听/连接 IPv6 与双栈；可用于同步客户端、简单服务端和自定义事件循环。本页只描述 TCP Socket，本机通信见 [LocalSocket](local-socket.md)。

## 功能说明

`Socket` 继承 `SocketInterface`，负责创建、绑定、监听、连接、收发和关闭描述符。它不负责消息分帧、自动重连、心跳或并发调度。

## API

| 接口 | 返回值 | 说明 |
|------|--------|------|
| `listen(port, address, dualStack)` | `bool` | 绑定并监听；`address` 默认 `"0.0.0.0"`，`dualStack` 默认 `false`（仅 IPv4 / 仅 IPv6） |
| `connect(address, port)` | `bool` | 建立 TCP 连接，支持 IPv4/IPv6 字面量 |
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

## IPv6 与双栈

`Socket` **默认为「仅 IPv4」**，与既有行为完全一致；IPv6 与双栈都必须显式开启。地址族由传入的地址字符串自动判定：

| `listen()` 的 `address` | 地址族 | 行为 |
|---|---|---|
| 省略、`"0.0.0.0"`、`"127.0.0.1"` | `AF_INET` | 仅 IPv4（默认） |
| `"::1"`、`"::"`、`"[::1]"`，`dualStack = false` | `AF_INET6` | 仅 IPv6（不隐式双栈） |
| `"::"`，`dualStack = true` | `AF_INET6` | 双栈：同一端口同时接受 IPv4 与 IPv6 连接 |

```cpp
// 仅 IPv6 回环
crazy::Socket onlyV6;
onlyV6.listen(8080, "::1");

// 双栈：同一端口同时接受 127.0.0.1 与 ::1
crazy::Socket both;
both.listen(8080, "::", true);
```

`connect(address, port)` 同样支持 IPv6 字面量（`"::1"`、`"[::1]"`）。**非字面量主机名继续只产出 IPv4 候选**：`connect("localhost")` 的地址族不会因为本次改动而变化，IPv6 域名解析另立需求。

**绑定地址只接受标准字面量**：`listen(port, address)` 的 `address` 必须是标准点分十进制 IPv4 字面量（四段十进制，如 `"127.0.0.1"`）或标准 IPv6 字面量（如 `"::1"`、`"[::1]"`）。**非标准写法一律失败并返回 `false`**，包括 `"127.1"` 这类缩写、八进制/十六进制写法（如 `"0177.0.0.1"`、`"0x7f.0.0.1"`）以及主机名（如 `"localhost"`）。这些写法过去由 `inet_addr` 顺带接受，现在不再被接受——行为是**收紧**而不是放宽，迁移时请把绑定地址写成标准字面量。

地址字符串格式：

| 场景 | `localAddress()` / `remoteAddress()` |
|---|---|
| IPv4 | `tcp://127.0.0.1:45678` |
| IPv6 | `tcp://[::1]:45678`（方括号形式，`:port` 仍在最后） |
| 双栈下由 IPv4 客户端连入 | `tcp://127.0.0.1:45678`（IPv4-mapped 对端已归一化，不出现 `::ffff:`） |

### IPV6_V6ONLY 与双栈语义

双栈不是「靠平台默认」就能成立的行为，必须在 `bind()` **之前**显式设置套接字选项 `IPV6_V6ONLY`：

- Windows（Vista 及以后）新建的 `AF_INET6` 套接字，`IPV6_V6ONLY` 默认为 **1（仅 IPv6）**；不显式置 0 时 IPv4 客户端会得到 `ConnectionRefused`。
- Linux 的默认值由 `/proc/sys/net/ipv6/bindv6only` 决定，该文件默认为 `0`（双栈）；运维把它改成 `1` 后，「不设置」的行为就会随环境漂移。
- `bind()` 之后再修改该选项无效，因此必须在绑定之前设置。

因此 `Socket::listen()` 在 IPv6 地址下**始终显式设置**该选项：`dualStack = false` 置 `1`（仅 IPv6），`dualStack = true` 置 `0`（双栈）。**设置失败时 `listen()` 返回 `false`**，不会记日志后继续、也不会静默退化为平台默认行为。

## TLS（`TlsSocket`）

TLS 以 `Socket` 子类的形式落在这一层：`crazy::TlsSocket : public Socket` 覆写了
`send`/`recv`/`close`/`pending`/`wantRead`/`wantWrite`，因此 `Session`、`Connection`、`HttpSession`
等上层只需要继续用 `Socket` 接口，不需要感知 TLS。`crazy::TlsContext` 负责加载 PEM 证书/私钥与校验策略。

构建期 TLS 支持**默认开启**（`CRAZY_ENABLE_TLS` 默认 `ON`，使用仓库内 `src/third_party/openssl`），
可用 `-DCRAZY_ENABLE_TLS=OFF` 显式关闭；**运行期**还需要在 `[HTTP]` 节配 `tls_enable = true` 与证书
才真正启用 HTTPS（详见 [HTTP 服务端](http-server.md) 的 HTTPS 小节）。
HTTPS/WSS 的配置与用法见 [HTTP 服务端](http-server.md) 与 [HTTP 客户端](http-client.md)。

```cpp
#include "crazy/net/tls.h"

std::string error;
crazy::TlsOptions options;
options.certFile = "server_cert.pem";
options.keyFile = "server_key.pem";
options.minVersion = "1.2";
auto context = crazy::TlsContext::createServer(options, error);
if (!context) {
    CRAZY_SYSTEM_ERROR() << error;   // 含具体文件路径；后续由调用方决定（HttpApplication 会降级明文并告警，见 http-server.md）
}

// 监听 socket 的 accept() 直接产出 TlsSocket（Socket::createAcceptedSocket 钩子）
crazy::TlsServerSocket listener(context);
listener.listen(8443, "0.0.0.0");
auto tls = std::dynamic_pointer_cast<crazy::TlsSocket>(listener.accept());
if (tls && tls->handshake(error)) {
    // tls->negotiatedVersion() / negotiatedCipher() 可读出协商结果
}
```

契约要点：

| 行为 | 语义 |
|------|------|
| `TlsSocket::recv()` | `> 0` 明文字节数；`0` 收到 `close_notify`（正常关闭）；`< 0` 错误（含连接被截断） |
| `TlsSocket::send()` | **要么全部收下、要么返回错误**，不把 `SSL_write` 的部分写入暴露成字节偏移语义 |
| `close()` | 先发送 `close_notify` 再关闭；关闭前有界排空未读数据，避免 RST 让对端看到「被截断」 |
| `wantRead()` / `wantWrite()` | TLS 的 `WANT_READ`/`WANT_WRITE`；`recv`/`send` 同时把它表现为 `errno == EAGAIN`，上层按「稍后再来」处理 |
| `pending()` | TLS 层已解密但尚未读走的字节数，供上层排空 TLS 缓冲（明文恒为 0） |

`handshake()` 默认有 **5 秒超时**（握手期间设置 `SO_RCVTIMEO`/`SO_SNDTIMEO`，握手结束立即恢复），
用于收敛「只连不发」客户端造成的握手期阻塞。业务 socket 本身仍是阻塞模型，改成非阻塞 IO 属另一独立需求。

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
