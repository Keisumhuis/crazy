# crazy

轻量级 C++17 基础框架库，提供 Actor 并发模型、网络通信、日志、配置、序列化、数据库连接池、内存映射、锁与常用工具封装。

- **语言标准**：C++17
- **构建系统**：CMake 3.15+
- **当前版本**：1.0.2
- **文档**：[中文完整文档](doc/README.md) | [English](README.en.md)

## 快速入口

| 内容 | 跳转 |
|------|------|
| 功能总览 | [doc/README.md#功能总览](doc/README.md#功能总览) |
| 编译构建 | [doc/README.md#编译构建](doc/README.md#编译构建) |
| 接入项目 | [doc/README.md#接入项目](doc/README.md#接入项目) |
| Application 与 Actor | [doc/README.md#application-与-actor](doc/README.md#application-与-actor) |
| 命令行向 Actor 发消息 | [doc/README.md#命令行向-actor-发消息](doc/README.md#命令行向-actor-发消息) |
| 网络通信 | [doc/README.md#网络通信](doc/README.md#网络通信) |
| HTTP 与 WebSocket | [doc/README.md#http-与-websocket](doc/README.md#http-与-websocket) |
| 日志与配置 | [doc/README.md#日志系统](doc/README.md#日志系统) / [配置管理](doc/README.md#配置管理) |
| 加密模块 | [doc/README.md#加密模块](doc/README.md#加密模块) |
| JSON、二进制协议、反射 | [doc/README.md#json-序列化](doc/README.md#json-序列化) / [二进制协议与反射](doc/README.md#二进制协议与反射) |
| MySQL 与 ClickHouse | [doc/README.md#mysql](doc/README.md#mysql) / [ClickHouse](doc/README.md#clickhouse) |
| mmap、Buffer、锁、工具类 | [doc/README.md#内存映射](doc/README.md#内存映射) / [基础工具](doc/README.md#基础工具) |
| 测试程序 | [doc/README.md#测试程序](doc/README.md#测试程序) |

## 最小示例

```cpp
#include "crazy.h"

class Worker : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

protected:
    void handleCommandLineMessageBase(
        crazy::MessageBase::ptr request,
        crazy::MessageBase::ptr response) override {
        response->setData("ok: " + request->getData());
    }
};

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<Worker>("worker");
    app.exec();
}
```

运行服务后，可在另一个终端发送命令：

```bash
./your_app -s worker@ping
```

更多模块和使用方法见 [中文完整文档](doc/README.md)。
