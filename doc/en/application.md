# Application

[Back to documentation index](../README.en.md)

`crazy::Application` is the process runtime. It owns Actor registration, the thread pool, message routing, the local command socket, file locking, and database connection pools.

## Start an application

```cpp
#include "crazy.h"

class Worker final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

protected:
    void handleCommandLineMessageBase(
        crazy::MessageBase::ptr request,
        crazy::MessageBase::ptr response) override {
        response->setData("worker received: " + request->getData());
    }
};

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<Worker>("worker");

    app.addRouteTable("worker", "application", 1001);
    app.enqueueRunnable([] {
        CRAZY_SYSTEM_INFO() << "background task";
    });

    app.exec();
}
```

## Common APIs

| API | Purpose |
|-----|---------|
| `registerActor<T>(name)` | Construct and register an Actor |
| `registerActor(ptr)` | Register an existing Actor |
| `addRouteTable(from, to, cmd)` | Register a message route |
| `enqueueRunnable(fn)` | Submit a task to the internal thread pool |
| `stopService(code)` | Stop services and worker threads |
| `restartService()` | Exit with code 1 after one second so the daemon can restart it |

## Command line

`Application::exec()` supports `-s` and `-d`. Send a command to an Actor:

```bash
./your_app -s worker@ping
```

The format is `actor@command`. Built-in commands include `restart`, `shutdown`, `time`, and `info`.

The constructor loads `.ini` files from the current directory and creates `<executable>.lock`. `[global]` supports `thread_pool_count`, and a Boolean value named after an Actor can control whether that Actor is registered.

## Lifecycle

`Application` starts in this order:

1. The constructor initializes networking and the local command socket, then loads INI files.
2. `registerActor()` fills the Actor registry unless `[global]` disables the name.
3. `exec()` handles daemon mode and command-line client mode.
4. Normal service mode acquires `<program>.lock` and creates configured database pools.
5. The framework creates the thread pool, initializes Actors, and registers command routes.
6. `startServer()` provides an extension point for HTTP and other subclasses.
7. `ActorInterface::run()` enters the application event loop.

## Complete example

```cpp
#include "crazy.h"

class JobActor final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

    std::map<std::string, std::string> helps() override {
        return {{"count", "read processed job count"}};
    }

protected:
    void init() override {
        registerTimer("report", 5000, [this] {
            CRAZY_SYSTEM_INFO() << "processed=" << processed_;
        });
    }

    void handleCommandLineMessageBase(
        crazy::MessageBase::ptr,
        crazy::MessageBase::ptr response) override {
        response->setData(std::to_string(processed_));
    }

    void handleMessageBase(crazy::MessageBase::ptr message) override {
        if (message->getCmd() == 1001) {
            ++processed_;
            CRAZY_SYSTEM_INFO() << "job: " << message->getData();
        }
    }

private:
    uint64_t processed_ = 0;
};

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<JobActor>("job");
    app.addRouteTable("application", "job", 1001);
    app.exec();
}
```

## Built-in commands

| Command | Behavior |
|---------|----------|
| `app@shutdown` | Calls `stopService(0)` |
| `app@restart` | Calls `stopService(1)` after one second |
| `app@time` | Returns the current system time |
| `app@info` | Returns framework and version information |

`stopService()` calls `exit()`. Do not invoke it in a library unit test that must keep running.

## Configuration and routes

```ini
[global]
thread_pool_count = 8
job = true
metrics = false
```

The `metrics` Actor is not registered in this deployment. Routes match both source and command:

```cpp
app.addRouteTable("gateway", "user", 1001);
app.addRouteTable("gateway", "audit", 1001);
```

One message can target multiple Actors. Log source, command, and route failures when messages do not arrive.

Related: [Actor](actor.md), [MessageBase](message.md), [Daemon](daemon.md), [Configuration](config.md).
