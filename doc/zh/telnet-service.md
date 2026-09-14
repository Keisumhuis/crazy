# TelnetServiceActor

[返回文档目录](../README.md)

`TelnetServiceActor` 提供带密码认证的 Telnet 命令入口，可以把文本命令转发给已经注册的 Actor。

## 配置

```ini
[telnet]
address = "127.0.0.1"
port = 2323
max_sessions = 16
idle_timeout = 300000
command_timeout = 10000
password = "change-me"
```

## 启动服务

```cpp
#include "crazy.h"

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<crazy::TelnetServiceActor>("telnet");
    app.registerActor<EchoActor>("echo");
    app.exec();
}
```

连接并执行命令：

```bash
telnet 127.0.0.1 2323
echo@ping
```

超时时间单位为毫秒。`idle_timeout` 控制无操作断开时间，`command_timeout` 控制等待 Actor 应答的最长时间。生产环境应使用强密码，并通过防火墙或内网隔离限制访问范围。

## 配置项

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `address` | `0.0.0.0` | Telnet 监听地址 |
| `port` | `2323` | Telnet 监听端口 |
| `max_sessions` | `16` | 最大并发连接数 |
| `idle_timeout` | `300000` | 空闲断开时间，毫秒 |
| `command_timeout` | `10000` | 等待 Actor 应答超时，毫秒 |
| `password` | 空 | 登录密码 |

密码为空时不会进行有效保护，开放监听地址前必须配置密码。Telnet 本身是明文协议，密码和命令都可能被网络抓包读取。

## 命令执行流程

客户端连接后，服务端先读取密码，然后显示提示符。输入：

```text
counter@inc
```

`TelnetServiceActor` 会把命令拆成 Actor 名称和命令正文，通过 Application 的命令行路由发送，并等待响应或超时。可以继续输入 `help`、`actors` 等内建命令查看帮助和 Actor 列表。

## 自定义业务命令

```cpp
class CounterActor final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

    std::map<std::string, std::string> helps() override {
        return {{"inc", "增加计数"}, {"get", "读取计数"}};
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

private:
    uint64_t count_ = 0;
};

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<crazy::TelnetServiceActor>("telnet");
    app.registerActor<CounterActor>("counter");
    app.exec();
}
```

## 会话限制

达到 `max_sessions` 后新连接应被拒绝。空闲会话和等待应答超时会话会关闭并释放资源。生产部署还应增加登录失败限速、网络 ACL 和审计日志，当前模块本身不实现这些策略。

相关文档：[Actor](actor.md)、[Application](application.md)、[配置管理](config.md)。
