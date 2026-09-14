# WebSocket

[Back to documentation index](../README.en.md)

WebSocket servers are hosted by `HttpApplication`. `WebSocketClient` provides the blocking client implementation.

## Server

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

## Client

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

Both `connect()` and `run()` block. Client frames are masked automatically. The server handles text, binary, ping, pong, and close frames.

## Handshake

The server validates the Upgrade request and WebSocket key, returns `101 Switching Protocols`, and computes `Sec-WebSocket-Accept`. `HttpSession` then switches from HTTP parsing to WebSocket framing. The client creates the upgrade request and verifies the response.

## Server callbacks

| Callback | Purpose |
|----------|---------|
| `registerWebSocketConnectCallback()` | Initialize a new session |
| `registerWebSocketMessageCallback()` | Handle text, binary, and control frames |
| `registerWebSocketCloseCallback()` | Observe disconnect events |

Fragmented frames are buffered and delivered as complete messages. Most handlers only need to branch on text and binary.

## Server setup

```cpp
#include "crazy/http_application.h"

int main(int argc, char** argv) {
    crazy::HttpApplication app(argc, argv);
    app.listen(8080);

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
            }
        });

    app.exec();
}
```

## Client API

| API | Purpose |
|-----|---------|
| `connect(uri)` | Establish TCP and complete Upgrade |
| `sendText()` / `sendBinary()` | Send data frames |
| `sendClose()` | Send a close frame |
| `run()` | Blocking receive loop |
| `close()` / `isConnected()` | Connection state |

`WebSocketClient` does not reconnect automatically. Implement exponential backoff and authentication outside `run()` for long-lived clients. Large messages are buffered entirely in memory.

Related: [HTTP server](http-server.md), [Session](session.md), [Buffer](buffer.md).
