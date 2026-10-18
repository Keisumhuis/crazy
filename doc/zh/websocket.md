# WebSocket

[返回文档目录](../README.md)

WebSocket 服务端由 `HttpApplication` 管理，客户端由 `WebSocketClient` 提供。

## 服务端

```cpp
#include "crazy/http_application.h"

int main(int argc, char** argv) {
    crazy::HttpApplication app(argc, argv);
    app.listen(8080);

    app.registerWebSocketConnectCallback([](crazy::HttpSession::ptr session) {
        session->sendText("welcome");
    });

    app.registerWebSocketMessageCallback(
        [](crazy::HttpSession::ptr session,
           const std::string& data,
           crazy::HttpSession::WebSocketOpCode opcode) {
            if (opcode == crazy::HttpSession::WebSocketOpCode::text) {
                session->sendText("echo: " + data);
            } else {
                session->sendBinary(data);
            }
        });

    app.registerWebSocketCloseCallback([] {
        CRAZY_SYSTEM_INFO() << "websocket closed";
    });

    app.exec();
}
```

## 客户端

```cpp
#include "crazy/net/http/websocket_client.h"

crazy::WebSocketClient ws;
ws.registerMessageCallback(
    [](const std::string& data, crazy::WebSocketClient::OpCode opcode) {
        if (opcode == crazy::WebSocketClient::OpCode::text) {
            CRAZY_SYSTEM_INFO() << data;
        }
    });

if (ws.connect("ws://127.0.0.1:8080/chat")) {
    ws.sendText("hello");
    ws.run();
}
```

`connect()` 和 `run()` 都按阻塞方式工作。客户端帧会自动加 mask，服务端会处理 text、binary、ping、pong 和 close 帧。

### WSS（WebSocket over TLS）

`wss://` **真的走 TLS**：与 `HttpClient` 使用同一套 TLS 实现与同一节 `[HTTP]`/`[http]` 配置
（`tls_ca_file` / `tls_verify_peer` / `tls_min_version`，见 [HTTP 客户端](http-client.md)）。

- 握手或证书校验失败时 `connect()` 返回 `false`，**绝不退化成明文**；
- 构建期用 `-DCRAZY_ENABLE_TLS=OFF` 显式关闭 TLS 支持时（默认是 `ON`），`wss://` 同样返回 `false`，且不会建立任何 TCP 连接。

> **编译期默认支持 ≠ 运行期默认启用。** 只有 `wss://` 才真正走 TLS；`ws://` 依旧是明文。

服务端侧不需要任何额外代码：TLS 在 Socket 层完成，WebSocket 升级与帧解析只看到解密后的字节流
（详见 [HTTP 服务端](http-server.md) 的 HTTPS 小节）。

## 握手过程

服务端在 HTTP 请求中发现 Upgrade 头后校验 WebSocket Key，并返回 `101 Switching Protocols` 和 `Sec-WebSocket-Accept`。`HttpSession` 随后切换到 WebSocket 帧解析模式。客户端通过 `HttpRequest::createWebSocketRequest()` 生成升级请求，并验证响应中的 Accept 值。

## 服务端回调

| 回调 | 参数 | 用途 |
|------|------|------|
| `registerWebSocketConnectCallback()` | `HttpSession::ptr` | 连接建立后发送欢迎信息或绑定会话状态 |
| `registerWebSocketMessageCallback()` | Session、data、OpCode | 处理文本、二进制和控制帧 |
| `registerWebSocketCloseCallback()` | 无 | 全局记录关闭事件或清理共享状态 |

消息回调参数中的 `data` 是已经拼装后的消息内容。分片帧由解析器缓存并合并，业务通常只需要检查 `OpCode::text` 和 `OpCode::binary`。

## 服务端完整示例

```cpp
void configureWebSocket(crazy::HttpApplication& app) {
    app.registerWebSocketConnectCallback(
        [](crazy::HttpSession::ptr session) {
            session->sendText(R"({"type":"connected"})");
        });

    app.registerWebSocketMessageCallback(
        [](crazy::HttpSession::ptr session,
           const std::string& data,
           crazy::HttpSession::WebSocketOpCode opcode) {
            if (opcode == crazy::HttpSession::WebSocketOpCode::text) {
                session->sendText(data);
            } else if (
                opcode == crazy::HttpSession::WebSocketOpCode::binary) {
                session->sendBinary(data);
            }
        });
}
```

## 客户端 API

| 接口 | 说明 |
|------|------|
| `connect(uri)` | 建立 TCP 连接并完成 HTTP Upgrade 握手 |
| `sendText(data)` | 发送 UTF-8 文本帧 |
| `sendBinary(data)` | 发送二进制帧 |
| `sendClose(code)` | 发送关闭帧 |
| `run()` | 阻塞读取帧并触发消息回调 |
| `connectAsync(uri, error)` | 建连并发出握手请求后立即返回（事件驱动） |
| `onReadable()` / `onWritable()` | selector 驱动的收发入口 |
| `registerSelectorCallbacks()` | 绑定事件注册/注销（通常指向 actor 的 `registerEventOnThread`） |
| `close()` / `isConnected()` | 查询和释放客户端连接 |

## 事件驱动（注册到 selector / actor）

`connectAsync()` 建连并发出握手请求后立即返回，握手响应与之后所有帧都由 selector 驱动的
`onReadable()` 处理，回调在 selector 线程触发；有积压数据时客户端自己挂上写事件。

```cpp
class ChatActor : public crazy::ActorInterface {
public:
    ChatActor() : ActorInterface("chat") {}

    void init() override {
        ws_.registerMessageCallback([](const std::string& data, crazy::WebSocketClient::OpCode opcode) {
            CRAZY_SYSTEM_INFO() << data;
        });
        ws_.registerCloseCallback([] { CRAZY_SYSTEM_INFO() << "ws closed"; });
        ws_.registerSelectorCallbacks(
            [this](int32_t fd, crazy::SelectorEventType type, std::function<void()> callback) {
                registerEventOnThread(fd, type, std::move(callback));
            },
            [this](int32_t fd, crazy::SelectorEventType type) {
                unregisterEventOnThread(fd, type);
            });
        std::string error;
        if (!ws_.connectAsync("wss://example.com/chat", error)) {
            CRAZY_SYSTEM_ERROR() << "connect failed: " << error;
        }
    }

private:
    crazy::WebSocketClient ws_;
};
```

- 回调都在 selector 线程；从别的线程发消息请走 `ActorInterface::enqueueFunction()`；
- 发送自动进缓冲，写不出去时等写事件；`sendClose()` 等缓冲发完再关闭；
- 心跳用 `Selector::registerTimer()` 定时 `sendText()` 即可；
- 不用 actor 时把这两个回调直接绑到 `crazy::Selector::registerEvent` / `unregisterEvent`（见 `tests/test_websocket_client_async.cc`）。

## 客户端重连策略

`WebSocketClient` 不自动重连。需要长期连接时，应在调用方维护循环：

```cpp
void runForever(crazy::WebSocketClient& ws) {
    while (true) {
        if (!ws.connect("ws://127.0.0.1:8080/chat")) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        ws.run();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

生产环境应增加指数退避、连接超时、认证 Token 和断线状态上报。大消息会整体保存在内存中，不适合直接传输超大文件。

相关文档：[HTTP 服务端](http-server.md)、[Session](session.md)、[Buffer](buffer.md)。
