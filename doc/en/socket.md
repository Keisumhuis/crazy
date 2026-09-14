# TCP Socket

[Back to documentation index](../README.en.md)

`crazy::Socket` wraps an IPv4 TCP socket for synchronous clients, simple servers, and custom event loops. This page covers TCP only; local IPC is documented in [LocalSocket](local-socket.md).

## API

| API | Result | Purpose |
|-----|--------|---------|
| `listen(port, address)` | `bool` | Bind and listen |
| `connect(address, port)` | `bool` | Establish TCP |
| `accept()` | `Socket::ptr` | Accept a client |
| `send(data, length, flags)` | `int32_t` | Write bytes, possibly partial |
| `recv(data, length, flags)` | `int32_t` | Read bytes, 0 means EOF |
| `active()` | `bool` | Local descriptor is valid |
| `close()` | `void` | Release descriptor |
| `getOption()` / `setOption()` | `bool` | Socket options |
| `remoteAddress()` / `localAddress()` | address strings | Endpoint details |

## Server and client

```cpp
#include "crazy/net/socket.h"

int main() {
    crazy::Socket server;
    if (!server.listen(8080, "0.0.0.0")) {
        return 1;
    }

    auto client = server.accept();
    char buffer[1024] = {};
    const int32_t received =
        client->recv(buffer, sizeof(buffer));
    if (received > 0) {
        client->send(buffer, static_cast<size_t>(received));
    }
}
```

```cpp
crazy::Socket socket;
if (socket.connect("127.0.0.1", 8080)) {
    const std::string request = "ping";
    socket.send(request.data(), request.size());
}
```

## Handle partial writes

`send()` can write fewer bytes than requested:

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

`Session` and `Connection` manage output buffering for framed protocols.

## Blocking and event-driven use

The direct examples are useful for tools and tests. Production servers should register read and write callbacks with `Selector`:

```cpp
selector.registerEvent(
    client->socket(),
    crazy::SelectorEventType::read,
    [client] { handleRead(client); });
```

Blocking `accept()` or `recv()` must not occupy a production Actor thread.

## Lifecycle and options

Create, listen/connect, exchange data, detect closure from return values or heartbeat policy, and close. `active()` only says that the local descriptor is valid.

```cpp
int enabled = 1;
socket.setOption(SOL_SOCKET, SO_REUSEADDR, enabled);
```

Prefer portable option wrappers over platform-specific flags in business code.

Related: [LocalSocket](local-socket.md), [Selector and timers](selector.md), [Encoder](encoder.md), [Decoder](decoder.md), [Session](session.md).
