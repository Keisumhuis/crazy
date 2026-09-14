# LocalSocket

[Back to documentation index](../README.en.md)

`crazy::LocalSocket` provides inter-process communication on one host. Application's local command service uses the same abstraction.

## Use cases

- Local administration commands.
- Process-to-process control protected by filesystem permissions.
- IPC that should not consume a network port.
- A shared API on Windows and Linux.

It does not provide authentication, message framing, or cross-host transport. Use `Encoder`/`Decoder` or `Session` for framing.

## API

| API | Purpose |
|-----|---------|
| `listen(address)` | Listen on a local path |
| `connect(address)` | Connect to a local socket |
| `accept()` | Accept one client |
| `send()` / `recv()` | Inherited byte transport |
| `active()` / `close()` | State and cleanup |

## Server and client

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

## Application command socket

`Application` listens on `<executable>.command`. Running the program with `-s` uses the client side of the same path.

```bash
./your_app -s application@time
```

Use distinct executable names or directories for parallel instances so command paths do not collide.

## Path and security

Unix socket paths have platform-specific length limits. A crashed process may leave a path behind; verify whether another process is using it before cleanup. Restrict filesystem permissions and bound command payloads. Never treat every local caller as trusted.

Related: [Socket](socket.md), [Application](application.md), [Encoder](encoder.md), [Decoder](decoder.md).
