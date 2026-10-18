# ServiceActor

[Back to documentation index](../README.en.md)

`crazy::ServiceActor` is a TCP server Actor that owns the listener, sessions, heartbeat checks, and inbound business messages.

## Configuration

```ini
[service]
address = "0.0.0.0"
port = 9090
heartbeat_interval = 5000
```

| Key | Default | Meaning |
|-----|---------|---------|
| `address` | `0.0.0.0` | Bind address; IPv4 literals stay IPv4, IPv6 literals (`::1`, `::`, `[::1]`) select IPv6 |
| `port` | `8901` | Listen port |
| `heartbeat_interval` | `5000` | Heartbeat check interval in milliseconds |

The default `address = "0.0.0.0"` means IPv4 only, exactly as before. Use an IPv6 literal to listen on IPv6:

```ini
[service]
address = "::1"   ; IPv6 loopback only
port = 9090
```

```ini
[service]
address = "::"    ; all IPv6 addresses, IPv6 only (no implicit dual stack)
port = 9090
```

Note that `ServiceActor`'s configuration does **not** expose a dual-stack switch, so `::` yields an IPv6-only listener. Callers that need one port to accept both IPv4 and IPv6 must call `Socket::listen(port, "::", true)` directly; see [IPv6 and dual-stack](socket.md).

```cpp
crazy::Application app(argc, argv);
app.registerActor<crazy::ServiceActor>("service");
app.exec();
```

## Lifecycle

1. Read the configuration section.
2. Create the acceptor and listen.
3. Accept a connection and create a `Session`.
4. Parse messages and invoke the business handler.
5. Check heartbeat timestamps on a timer.
6. Remove the session when the peer disconnects.

## Custom service

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
```

## Sessions and heartbeat

Each connection has a `sessionId`. Responses preserve it through `createResponse()`, allowing the server to send to the correct socket. Long callbacks delay both messages and heartbeat handling; move blocking work to the thread pool.

The class does not provide authentication, rate limiting, or connection quotas. TLS lives at the **Socket layer** (`crazy::TlsSocket`, see [Socket](socket.md) and the HTTPS section of [HTTP server](http-server.md)), so `Session`, `Connection`, `ServiceActor`, and `TelnetServiceActor` can reuse the same abstraction later **without touching business logic** — but they are **not wired to it in this revision**, and their connections are still plaintext today. Add those capabilities at the network or business layer for production.

Related: [ClientActor](client-actor.md), [Session](session.md), [Socket](socket.md), [Configuration](config.md).
