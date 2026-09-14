# ClientActor

[Back to documentation index](../README.en.md)

`crazy::ClientActor` manages one outbound server connection, including connect, disconnect, message delivery, and heartbeat.

## Configuration

```ini
[client]
address = "127.0.0.1"
port = 9090
heartbeat_interval = 5000
```

| Key | Default | Meaning |
|-----|---------|---------|
| `address` | `127.0.0.1` | Server address |
| `port` | `8901` | Server port |
| `heartbeat_interval` | `5000` | Heartbeat interval in milliseconds |

## Start the client

```cpp
#include "crazy.h"

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<crazy::ClientActor>("client");
    app.exec();
}
```

## Business subclass

```cpp
class GatewayClient final : public crazy::ClientActor {
public:
    using crazy::ClientActor::ClientActor;

protected:
    void handleMessageBase(crazy::MessageBase::ptr message) override {
        if (message->getCmd() == 2001) {
            CRAZY_SYSTEM_INFO() << message->getData();
            return;
        }
        crazy::ClientActor::handleMessageBase(message);
    }
};
```

## Connection state

```text
disconnected -> connecting -> connected -> disconnecting -> disconnected
```

The current Actor does not implement automatic exponential-backoff reconnects. Add a bounded retry policy for long-lived clients and track failures, last heartbeat, and pending work.

One `ClientActor` represents one endpoint configuration. Register multiple Actors with distinct names and INI sections when connecting to multiple servers.

## Deployment guidance

- Make address and port configurable.
- Distinguish initial connection failure from a later disconnect.
- Monitor reconnect and heartbeat failures.
- Avoid unbounded reconnect storms.

Related: [ServiceActor](service-actor.md), [Session](session.md), [Socket](socket.md), [Configuration](config.md).
