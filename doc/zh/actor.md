# Actor

[返回文档目录](../README.md)

`crazy::ActorInterface` 为每个 Actor 提供独立线程、消息队列、异步任务队列、Selector 事件循环和命令行消息处理。

## 自定义 Actor

```cpp
#include "crazy.h"

class CounterActor final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

    std::map<std::string, std::string> helps() override {
        return {{"get", "读取计数"}, {"inc", "增加计数"}};
    }

protected:
    void handleCommandLineMessageBase(
        crazy::MessageBase::ptr request,
        crazy::MessageBase::ptr response) override {
        if (request->getData() == "inc") {
            ++count_;
        }
        response->setData("count=" + std::to_string(count_));
    }

    void handleMessageBase(crazy::MessageBase::ptr message) override {
        if (message->getCmd() == 100) {
            auto response = message->createResponse();
            response->setData("handled=" + std::to_string(++count_));
            sendMessage(response);
        }
    }

private:
    uint64_t count_ = 0;
};

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<CounterActor>("counter");
    app.exec();
}
```

## 异步任务与事件

Actor 成员函数只应在 Actor 线程中执行。需要跨线程调度时使用 `enqueueFunction()`；需要注册 fd 事件时使用 `registerEventOnThread()`。

```cpp
enqueueFunction([this] {
    CRAZY_SYSTEM_INFO() << "running on actor thread";
});
```

`messageQueueSize()` 和 `asyncTaskQueueSize()` 可用于观察待处理任务数量。消息处理函数不要长时间阻塞，否则该 Actor 的其他消息和定时器都会延迟。

## 线程模型

每个 `ActorInterface` 在 `start()` 后创建独立线程。线程循环依次处理事件、普通消息、命令行消息和异步函数。Actor 的成员状态因此可以被视为单线程状态，不需要为每个字段增加互斥锁。

推荐把所有状态修改都约束在 Actor 线程内：

```cpp
class SessionCounter final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

    void sessionOpened() {
        enqueueFunction([this] {
            ++online_;
        });
    }

    uint64_t online() {
        // 仅用于演示；真实接口应通过消息把结果返回调用方。
        return onlineSnapshot_.load();
    }

private:
    std::atomic<uint64_t> onlineSnapshot_{0};
    uint64_t online_ = 0;
};
```

如果必须在多个线程读取状态，应发布不可变快照或使用 `MVCCLockWrapper`，不要把普通成员直接暴露给外部线程。

## 接口分组

| 接口 | 调用时机 | 说明 |
|------|----------|------|
| `init()` | Actor 线程启动前 | 初始化定时器、资源或初始消息 |
| `start()` | Application 启动 Actor 时 | 创建并运行 Actor 线程 |
| `stop()` | 应用退出时 | 请求跳出运行循环 |
| `helps()` | Application 收集命令帮助时 | 返回命令及说明 |
| `handleMessageBase()` | 收到普通消息时 | 业务消息入口 |
| `handleCommandLineMessageBase()` | 收到命令行请求时 | 返回文本应答 |
| `enqueueMessage()` | 任意线程 | 投递普通消息 |
| `enqueueFunction()` | 任意线程 | 在 Actor 线程运行函数 |
| `registerAsyncTask()` | Actor 初始化阶段 | 注册需要持续调度的异步动作 |

## 定时器完整示例

```cpp
class HeartbeatActor final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

protected:
    void init() override {
        registerTimer("heartbeat", 3000, [this] {
            CRAZY_SYSTEM_INFO() << "heartbeat " << ++sequence_;
        });

        registerTimePointTask("cleanup", 3, 30, 0, [this] {
            clearExpiredSessions();
        });
    }

    void clearExpiredSessions() {
        // 在 Actor 线程中安全修改独占状态。
    }

private:
    uint64_t sequence_ = 0;
};
```

定时回调只在 Actor 线程执行，因此可以直接访问成员。回调执行时间过长会推迟下一次事件循环，超过间隔时定时器会按完成时间重新计算下一次触发。

## 路由消息

`Application::addRouteTable(from, to, cmd)` 建立路由后，消息来源和命令号必须同时匹配。Actor 自己处理响应时可调用受保护的 `sendMessage()`：

```cpp
void handleMessageBase(crazy::MessageBase::ptr message) override {
    auto response = message->createResponse();
    response->setData("handled");
    sendMessage(response);
}
```

消息来源通常来自注册名称、网络 Session 或 Application。调试时先记录 `source`、`sessionId` 和 `cmd`，再排查路由表。

## 队列积压与退出

`messageQueueSize()` 和 `asyncTaskQueueSize()` 适合做监控。队列持续增长通常意味着消息处理过慢、下游阻塞或消息产生速度超过处理能力。不要通过无限增加线程数解决顺序状态竞争，应优先拆分 Actor 或把阻塞任务移到线程池。

`stop()` 只用于停止该 Actor。多个 Actor 的退出由 `Application::stopService()` 统一协调；业务代码应保证线程池任务不会在 Actor 销毁后继续访问 Actor 指针。

相关文档：[Application](application.md)、[MessageBase](message.md)、[Selector 与定时任务](selector.md)、[ThreadPool](thread-pool.md)。
