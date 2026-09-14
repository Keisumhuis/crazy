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
| `address` | `0.0.0.0` | Bind address |
| `port` | `8901` | Listen port |
| `heartbeat_interval` | `5000` | Heartbeat check interval in milliseconds |

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

The class does not provide TLS, authentication, rate limiting, or connection quotas. Add those at the network or business layer for production.

Related: [ClientActor](client-actor.md), [Session](session.md), [Socket](socket.md), [Configuration](config.md).
