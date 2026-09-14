# Actor

[Back to documentation index](../README.en.md)

`crazy::ActorInterface` gives each Actor a dedicated thread, message queue, asynchronous task queue, Selector event loop, and command-line handler.

## Custom Actor

```cpp
#include "crazy.h"

class CounterActor final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

    std::map<std::string, std::string> helps() override {
        return {{"get", "read the counter"}, {"inc", "increment the counter"}};
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

## Async work

Actor members should run on the Actor thread. Use `enqueueFunction()` for cross-thread work and `registerEventOnThread()` for fd registration.

```cpp
enqueueFunction([this] {
    CRAZY_SYSTEM_INFO() << "running on actor thread";
});
```

`messageQueueSize()` and `asyncTaskQueueSize()` expose pending work. Do not block an Actor handler for a long time, because messages and timers on that Actor will be delayed.

## Threading model

Each Actor creates one thread. The event loop processes Selector events, ordinary messages, command-line messages, and asynchronous functions. Actor member state can therefore be single-threaded and does not need a mutex for every field.

Cross-thread state access should use snapshots, immutable values, or an explicit concurrency wrapper. Never expose ordinary Actor fields to arbitrary threads.

## API groups

| API | When it runs | Purpose |
|-----|--------------|---------|
| `init()` | Before thread startup | Initialize timers and resources |
| `start()` / `stop()` | Application lifecycle | Start or stop the Actor thread |
| `helps()` | Help collection | Describe CLI commands |
| `handleMessageBase()` | Ordinary message | Business dispatch |
| `handleCommandLineMessageBase()` | CLI request | Produce a text response |
| `enqueueMessage()` | Any thread | Queue a message |
| `enqueueFunction()` | Any thread | Run a function on the Actor thread |
| `registerAsyncTask()` | Initialization | Register recurring async work |

## Timers

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

    void clearExpiredSessions() {}

private:
    uint64_t sequence_ = 0;
};
```

Timer callbacks run on the Actor thread, so they may access Actor state directly. A callback that runs longer than its interval delays the next event-loop pass.

## Message responses

```cpp
void handleMessageBase(crazy::MessageBase::ptr message) override {
    auto response = message->createResponse();
    response->setData("handled");
    sendMessage(response);
}
```

Use `messageQueueSize()` and `asyncTaskQueueSize()` for monitoring. A continually growing queue usually means blocking work or insufficient Actor partitioning. Move blocking calls out instead of adding unbounded threads.

Related: [Application](application.md), [MessageBase](message.md), [Selector and timers](selector.md), [ThreadPool](thread-pool.md).
