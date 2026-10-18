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

### WSS (WebSocket over TLS)

`wss://` **really uses TLS**: it shares the TLS implementation and the same `[HTTP]` / `[http]` section with
`HttpClient` (`tls_ca_file` / `tls_verify_peer` / `tls_min_version` — see [HTTP client](http-client.md)).

- if the handshake or certificate verification fails, `connect()` returns `false` and never falls back to plaintext;
- in a build that explicitly turned TLS off with `-DCRAZY_ENABLE_TLS=OFF` (it is `ON` by default), `wss://` also returns `false` and no TCP connection is even attempted.

> **Compiled-in by default does not mean enabled at run time.** Only `wss://` really uses TLS; `ws://` stays plaintext.

The server needs no extra code: TLS terminates at the Socket layer, so the WebSocket upgrade and framing only
see the decrypted byte stream (see the HTTPS section of [HTTP server](http-server.md)).

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
| `connectAsync(uri, error)` | Connect and send the Upgrade request, then return (event driven) |
| `onReadable()` / `onWritable()` | Selector driven I/O entry points |
| `registerSelectorCallbacks()` | Bind event register/unregister (usually to the actor's `registerEventOnThread`) |
| `close()` / `isConnected()` | Connection state |

## Event driven (register into a selector / actor)

`connectAsync()` returns right after the TCP connection (and the `wss` TLS handshake) is established and the
Upgrade request is written. The handshake response and every later frame are handled by the selector driven
`onReadable()`; callbacks fire on the selector thread, and write events are registered by the client itself
whenever data is queued.

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

- Callbacks run on the selector thread; to send from another thread use `ActorInterface::enqueueFunction()`.
- Sends are buffered, and `sendClose()` waits for the buffer to drain before closing.
- Heartbeats are just a `Selector::registerTimer()` that calls `sendText()`.
- Without an actor, bind the same two callbacks to `crazy::Selector::registerEvent` / `unregisterEvent` (see `tests/test_websocket_client_async.cc`).

`WebSocketClient` does not reconnect automatically. Implement exponential backoff and authentication outside `run()` for long-lived clients. Large messages are buffered entirely in memory.

Related: [HTTP server](http-server.md), [Session](session.md), [Buffer](buffer.md).
