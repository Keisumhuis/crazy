# Application

[返回文档目录](../README.md)

`crazy::Application` 是进程级运行入口，负责 Actor 注册、线程池、消息路由、本地命令行服务、文件锁和数据库连接池。

## 启动应用

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

## 常用接口

| 接口 | 作用 |
|------|------|
| `registerActor<T>(name)` | 创建并注册 Actor |
| `registerActor(ptr)` | 注册已经构造的 Actor |
| `addRouteTable(from, to, cmd)` | 注册消息路由 |
| `enqueueRunnable(fn)` | 向内部线程池提交任务 |
| `stopService(code)` | 停止各服务和线程池 |
| `restartService()` | 延迟一秒后以退出码 1 结束，供守护进程拉起 |

## 命令行

`Application::exec()` 内建 `-s` 和 `-d` 参数。向 Actor 发送命令：

```bash
./your_app -s worker@ping
```

命令行格式为 `actor@command`。应用还内置 `restart`、`shutdown`、`time` 和 `info` 命令。

## 配置约定

构造 `Application` 时会加载当前目录下所有 `.ini` 文件，并创建 `<可执行文件名>.lock`。`[global]` 中可以配置 `thread_pool_count`，也可以使用 Actor 名称对应的布尔值控制是否注册该 Actor。

## 生命周期

`Application` 的启动顺序如下：

1. 构造函数初始化网络环境、本地命令 Socket，并加载当前目录的 INI 文件。
2. `registerActor()` 把 Actor 放入注册表；`[global]` 中同名值为 `false` 时不会加入。
3. `exec()` 先处理 `-d`，再处理 `-s` 客户端命令。
4. 普通服务模式获取应用文件锁，并按配置创建数据库连接池。
5. 框架创建线程池、初始化 Actor、注册命令行路由和本地 Socket。
6. `startServer()` 提供 HTTP 等子类启动扩展点。
7. `ActorInterface::run()` 进入应用自己的事件循环，直到 `stopService()` 或信号触发退出。

## 完整示例

下面的程序包含业务 Actor、消息路由、后台任务和优雅停止入口：

```cpp
#include "crazy.h"

class JobActor final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

    std::map<std::string, std::string> helps() override {
        return {{"count", "读取已处理任务数"}};
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
    app.enqueueRunnable([&app] {
        auto message = std::make_shared<crazy::MessageBase>();
        message->setSource("application");
        message->setCmd(1001);
        message->setData("bootstrap");
        app.dispatchCommandLine("job@count", 0, "boot");
    });
    app.exec();
}
```

## 内置命令与退出

| 命令 | 行为 |
|------|------|
| `app@shutdown` | 调用 `stopService(0)`，停止线程池、Actor 和连接池 |
| `app@restart` | 延迟一秒调用 `stopService(1)` |
| `app@time` | 返回当前系统时间 |
| `app@info` | 返回框架和版本信息 |

`stopService()` 会调用 `exit()`，因此不要在需要继续运行当前进程的单元测试里调用。需要可回收的测试代码时，应直接测试 Actor 或组件，而不是启动完整 `Application`。

## Actor 启用开关

```ini
[global]
thread_pool_count = 8
job = true
metrics = false
```

上例不会注册名为 `metrics` 的 Actor。配置开关适合按部署环境裁剪模块，但代码仍然会编译对应 Actor 类型。

## 路由

路由表由 `from + cmd` 定位目标列表。`addRouteTable()` 只声明规则；消息进入 Application 后，`routeMessage()` 根据消息来源和命令选择目标：

```cpp
app.addRouteTable("gateway", "user", 1001);
app.addRouteTable("gateway", "audit", 1001);
```

同一条消息可以路由到多个 Actor。缺少匹配项时消息不会被业务 Actor 处理，应使用日志和测试确认来源名称与命令号。

相关文档：[Actor](actor.md)、[MessageBase](message.md)、[Daemon](daemon.md)、[配置管理](config.md)。
