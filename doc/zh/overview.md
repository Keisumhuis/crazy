# 功能总览

[返回文档目录](../README.md)

`crazy` 是一个 C++17 静态库，聚合了服务端程序常用的并发、网络、配置、序列化和存储能力。各功能按职责拆分，也可以直接包含聚合头文件 `crazy.h`。

## 模块划分

| 分类 | 功能 | 主要头文件 |
|------|------|------------|
| 应用 | Application、Actor、Message、Daemon | `application.h`, `actor_interface.h`, `message_base.h`, `daemon.h` |
| 网络 | Socket、Selector、Session、ServiceActor、HTTP、WebSocket、SMTP | `net/*.h`, `http_application.h` |
| 数据处理 | JSON、反射、二进制协议、MySQL、ClickHouse | `json.h`, `reflection.h`, `protocol.h` |
| 基础设施 | 日志、配置、加密、Buffer、mmap、线程池、锁、时间与路径工具 | `logger.h`, `config.h` 等 |

## 最小程序

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

启动程序后，在另一个终端发送命令：

```bash
./your_app -s echo@ping
```

## 相关文档

- [编译与构建](build.md)
- [接入现有项目](integration.md)
- [Application](application.md)
- [Actor](actor.md)
- [测试](testing.md)

## 设计原则

框架按“一个 Actor 管一个线程，一个线程管一组状态”的方式组织业务代码。Actor 之间的普通 C++ 对象不共享写权限，而是通过消息、路由表和异步任务协作。网络事件由 `Selector` 驱动，耗时工作通过线程池或 Actor 异步任务队列执行。

一次典型请求会经过以下阶段：

1. `Socket` 或 HTTP Session 接收到字节流。
2. `Decoder` 或 HTTP Parser 拼装出完整请求。
3. Session 把请求转换成 `MessageBase` 或交给 HTTP Handler。
4. Application 的路由表把消息投递给目标 Actor。
5. Actor 在独占线程中处理业务状态并创建响应。
6. Encoder 或 HTTP 会话把响应写回客户端。

这种结构的主要收益是 Actor 内部状态不需要额外加锁，缺点是任意 Actor 处理函数阻塞都会拖慢该 Actor 的消息、定时器和网络事件。因此数据库查询、文件扫描和长计算更适合交给线程池，并按需把结果投递回 Actor。

## 模块选择

| 场景 | 建议入口 |
|------|----------|
| 单体后台服务、命令行运维 | `Application` + `Actor` + `LocalSocket` |
| 自定义二进制长连接 | `ServiceActor` + `Session` + `protocol` |
| HTTP API 或静态文件 | `HttpApplication` |
| 浏览器实时推送 | `HttpApplication` + WebSocket |
| 定时推进业务状态 | `ActorInterface` 中的 Selector 定时器 |
| 批量任务或阻塞调用 | `ThreadPool` 或 `ActorInterface::enqueueFunction()` |
| 测试或轻量进程通信 | `Socket`、`LocalSocket`、`Buffer` |

## 运行时文件

应用启动时可能在可执行文件所在工作目录创建以下文件：

- `<程序名>.lock`：应用单实例锁。
- `<程序名>.daemon.lock`：守护进程 supervisor 单实例锁。
- `<程序名>.command`：本地命令行 Socket。
- 日志目录及按级别拆分的日志文件。

部署时应保证程序对这些路径有读写权限。如果切换到只读目录，应让工作目录指向可写目录，或调整配置与启动脚本。

## 从最小示例继续

最小程序只演示命令入口。真实服务通常还会注册路由、初始化数据库连接池和加载配置：

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

这段代码展示了三种协作方式：注册入口、声明路由、跨线程投递消息。后续每个功能页都会从这些基础概念继续展开。
