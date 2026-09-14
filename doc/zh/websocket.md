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
| `close()` / `isConnected()` | 查询和释放客户端连接 |

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
