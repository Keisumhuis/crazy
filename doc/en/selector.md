# Selector and Timers

[Back to documentation index](../README.en.md)

`Selector` wraps epoll on Linux and wepoll on Windows. It handles fd read/write events, repeating timers, and daily fixed-time tasks.

## Register events

```cpp
#include "crazy/net/selector.h"

class EventLoop final : public crazy::Selector {
public:
    void start(int32_t fd) {
        registerEvent(fd, crazy::SelectorEventType::read, [fd] {
            CRAZY_SYSTEM_INFO() << "fd readable: " << fd;
        });

        registerTimer("tick", 1000, [] {
            CRAZY_SYSTEM_INFO() << "one second";
        });

        registerTimePointTask("daily", 0, 30, 0, [] {
            CRAZY_SYSTEM_INFO() << "daily task";
        });

        select();
    }
};
```

## Dynamic changes

```cpp
selector.unregisterEvent(fd, crazy::SelectorEventType::read);
selector.cancelEvent(fd);
selector.unregisterTimer("tick");
selector.unregisterTimePointTask("daily");
selector.wakeup();
```

Callbacks run on the thread that called `select()`. The methods ending in `OnThread` on `ActorInterface` provide a safe way to schedule event registration from another thread.

## Event semantics

Read and write callbacks can be registered independently for the same fd. Registering the same event type again replaces the old callback. Removing one event type keeps the other active.

```cpp
loop.registerEvent(fd, crazy::SelectorEventType::read,
                   [fd] { readSocket(fd); });
loop.registerEvent(fd, crazy::SelectorEventType::write,
                   [fd] { flushSocket(fd); });
```

`wakeup()` interrupts a blocked `select()` call. It is required when another thread changes the event set or asks the loop to stop.

## Timer semantics

| API | Time argument | Behavior |
|-----|---------------|----------|
| `registerTimer()` | Milliseconds | Repeating timer |
| `unregisterTimer()` | Name | Remove a repeating timer |
| `registerTimePointTask()` | Hour, minute, second | Daily local-time task |
| `unregisterTimePointTask()` | Key | Remove a daily task |

Timers run on the event-loop thread. A callback that takes longer than its interval shifts the next execution; this is not a hard real-time scheduler.

## Complete loop

```cpp
#include "crazy/net/selector.h"

class TcpLoop final : public crazy::Selector {
public:
    void setup(int32_t listenFd) {
        registerEvent(listenFd, crazy::SelectorEventType::read,
            [this] { acceptClients(); });

        registerTimer("metrics", 10000,
            [this] { reportMetrics(); });

        registerTimePointTask("rollover", 0, 0, 0,
            [this] { rotateLogs(); });
    }

    void runLoop() {
        while (running_) {
            select();
        }
    }

    void stopLoop() {
        running_ = false;
        wakeup();
    }

private:
    void acceptClients() {}
    void reportMetrics() {}
    void rotateLogs() {}

    bool running_ = true;
};
```

`ActorInterface` itself derives from `Selector`, so its run loop has the same threading rules. HTTP and service Actors build higher-level protocols on this primitive.

Related: [Actor](actor.md), [Socket](socket.md), [ServiceActor](service-actor.md), [ClientActor](client-actor.md).
