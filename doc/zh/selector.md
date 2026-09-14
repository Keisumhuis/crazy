# Selector 与定时任务

[返回文档目录](../README.md)

`Selector` 封装 epoll（Linux）和 wepoll（Windows），用于 fd 读写事件、循环定时器和每日固定时间任务。

## 注册事件

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

## 动态修改

```cpp
selector.unregisterEvent(fd, crazy::SelectorEventType::read);
selector.cancelEvent(fd);
selector.unregisterTimer("tick");
selector.unregisterTimePointTask("daily");
selector.wakeup();
```

事件回调和定时器回调在 `select()` 所在线程执行。跨线程注册或取消事件时，应先投递到该线程，`ActorInterface` 提供的 `registerEventOnThread()` 和 `cancelEventOnThread()` 已封装这一约束。

## 事件模型

`registerEvent(fd, type, callback)` 会把 fd 加入 epoll。同一个 fd 可以同时注册 read 和 write，框架内部按位合并事件类型：

```cpp
loop.registerEvent(
    fd, crazy::SelectorEventType::read,
    [fd] { readSocket(fd); });

loop.registerEvent(
    fd, crazy::SelectorEventType::write,
    [fd] { flushSocket(fd); });
```

重复注册同一类型会覆盖该类型原回调。取消一种类型不会影响另一种类型；`cancelEvent(fd)` 会同时删除该 fd 的全部事件。

## 定时器语义

| 接口 | 时间参数 | 触发方式 |
|------|----------|----------|
| `registerTimer(name, ms, callback)` | 毫秒 | 回调完成后重新计算下一次时间 |
| `unregisterTimer(name)` | 无 | 按名称删除循环任务 |
| `registerTimePointTask(key, h, m, s, callback)` | 时、分、秒 | 每天固定时间触发 |
| `unregisterTimePointTask(key)` | 无 | 按 key 删除每日任务 |

循环定时器不是在独立线程执行，也不保证严格固定频率。如果回调耗时超过间隔，下一次触发时间会从本次回调结束后重新计算。

## 完整事件循环

```cpp
#include "crazy/net/selector.h"

class TcpLoop final : public crazy::Selector {
public:
    void setup(int32_t listenFd) {
        registerEvent(listenFd, crazy::SelectorEventType::read,
            [this] { acceptClients(); });

        registerTimer("metrics", 10000, [this] {
            reportMetrics();
        });

        registerTimePointTask("rollover", 0, 0, 0, [this] {
            rotateLogs();
        });
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

如果不调用 `wakeup()`，已经阻塞在 `epoll_wait()` 的线程可能无法立即看到 `running_` 的变化。所有会改变事件集合或停止状态的外部线程都应通过 `wakeup()` 唤醒循环。

## 与其他模块的关系

`ActorInterface` 本身继承 `Selector`，Actor 的 `run()` 循环就是事件循环。`ServiceActor`、`ClientActor` 和 `TelnetServiceActor` 在此之上管理监听、Socket 会话与心跳。HTTP 服务也使用独立会话事件循环。

相关文档：[Actor](actor.md)、[Socket](socket.md)、[ServiceActor](service-actor.md)、[ClientActor](client-actor.md)。
