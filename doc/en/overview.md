# Feature Overview

[Back to documentation index](../README.en.md)

`crazy` is a C++17 static library for server-side concurrency, networking, configuration, serialization, storage, and common infrastructure. Every module can be included directly, or through the aggregate header `crazy.h`.

## Modules

| Area | Features | Main headers |
|------|----------|--------------|
| Application | Application, Actor, Message, Daemon | `application.h`, `actor_interface.h`, `message_base.h`, `daemon.h` |
| Networking | Socket, Selector, Session, ServiceActor, HTTP, WebSocket, SMTP | `net/*.h`, `http_application.h` |
| Data | JSON, reflection, binary protocol, MySQL, ClickHouse | `json.h`, `reflection.h`, `protocol.h` |
| Infrastructure | Logging, config, encryption, Buffer, mmap, thread pool, locks, time, path tools | `logger.h`, `config.h`, and others |

## Minimal example

```cpp
#include "crazy.h"

class EchoActor final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

protected:
    void handleCommandLineMessageBase(
        crazy::MessageBase::ptr request,
        crazy::MessageBase::ptr response) override {
        response->setData("echo: " + request->getData());
    }
};

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<EchoActor>("echo");
    app.exec();
}
```

Send a command from another terminal:

```bash
./your_app -s echo@ping
```

## Related pages

- [Build](build.md)
- [Project integration](integration.md)
- [Application](application.md)
- [Actor](actor.md)
- [Testing](testing.md)

## Design model

The framework follows a simple ownership rule: one Actor runs on one thread and owns the mutable state for one responsibility. Actors cooperate through messages, routing rules, and asynchronous functions rather than directly sharing writable C++ objects. Network readiness is handled by `Selector`, while blocking or CPU-heavy work can be moved to the thread pool.

A typical request passes through these stages:

1. `Socket` or an HTTP session receives bytes.
2. `Decoder` or an HTTP parser assembles a complete request.
3. The session creates a `MessageBase` or invokes an HTTP handler.
4. `Application` routes the message by source and command.
5. The target Actor processes state on its own thread and creates a response.
6. `Encoder` or the HTTP session writes the response to the client.

The benefit is lock-free state inside an Actor. The tradeoff is that blocking one Actor handler also delays its messages and timers, so database calls, file scans, and long computations should move to the thread pool and post results back to the Actor.

## Choosing a module

| Use case | Recommended entry point |
|----------|-------------------------|
| Long-running service with CLI operations | `Application`, `Actor`, `LocalSocket` |
| Custom binary long connections | `ServiceActor`, `Session`, `protocol` |
| HTTP API or static files | `HttpApplication` |
| Browser push and real-time interaction | `HttpApplication` WebSocket callbacks |
| Timer-driven business state | `ActorInterface` Selector timers |
| Batch jobs or blocking calls | `ThreadPool` or `ActorInterface::enqueueFunction()` |
| Lightweight IPC or tests | `Socket`, `LocalSocket`, `Buffer` |

## Runtime files

An application may create these files relative to the working directory:

- `<program>.lock` for single-instance control.
- `<program>.daemon.lock` for the daemon supervisor.
- `<program>.command` as the local command socket.
- Log directories and per-level log files.

The deployment directory must be writable. If the executable lives on a read-only volume, start the process from a writable working directory or adjust the deployment layout.

## Extending the minimal example

```cpp
int main(int argc, char** argv) {
    crazy::Application app(argc, argv);

    auto worker = std::make_shared<EchoActor>("echo");
    app.registerActor(worker);
    app.addRouteTable("gateway", "echo", 1001);

    app.enqueueRunnable([worker] {
        auto message = std::make_shared<crazy::MessageBase>();
        message->setSource("bootstrap");
        message->setCmd(1001);
        message->setData("ready");
        worker->enqueueMessage(message);
    });

    app.exec();
}
```

This example shows registration, route declaration, and cross-thread message delivery. The feature pages expand each of those concepts.

Related: [Build](build.md), [Project integration](integration.md), [Application](application.md), [Actor](actor.md), [Testing](testing.md).
