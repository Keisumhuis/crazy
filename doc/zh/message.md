# MessageBase

[返回文档目录](../README.md)

`crazy::MessageBase` 是 Actor、Session、Encoder 和 Decoder 之间共享的消息对象。

## 普通消息

```cpp
#include "crazy.h"

auto request = std::make_shared<crazy::MessageBase>();
request->setSource("gateway");
request->setSessionId(42);
request->setCmd(1001);
request->setComment("query user");
request->setData("user_id=7");

auto response = request->createResponse();
response->setData("user_name=alice");
```

`createResponse()` 会保留来源、会话和命令等路由信息，并将请求和应答关联起来。

## 强类型内部消息

```cpp
struct Task {
    uint64_t id = 0;
    std::string name;
};

auto message = std::make_shared<crazy::InternalMessage<Task>>();
message->setCmd(2001);
message->setValue(Task{7, "build"});

const Task& task = message->getValue();
```

`InternalMessage<T>` 适合进程内调用，不会自动把 `T` 序列化到网络。跨进程传输时应把内容写入 `data`，或者使用 `protocol`/`json` 模块编码。

## 字段说明

| 字段 | 类型 | 用途 |
|------|------|------|
| `source` | `std::string` | 消息来源，通常是 Actor 注册名 |
| `sessionId` | `uint64_t` | 网络会话标识，0 表示非网络消息 |
| `cmd` | `uint64_t` | 路由和业务分派命令号 |
| `comment` | `std::string` | 诊断或业务备注，不参与默认路由 |
| `data` | `std::string` | 正文，可以是文本或已编码二进制 |

`createResponse()` 复制请求的关键路由字段，并根据约定生成应答命令。业务代码随后只需要设置正文。

## 完整请求应答

```cpp
class UserActor final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

protected:
    void handleMessageBase(crazy::MessageBase::ptr request) override {
        if (request->getCmd() != 1001) {
            return;
        }

        const uint64_t userId =
            std::stoull(request->getData());
        auto response = request->createResponse();

        response->setComment("find user");
        if (userId == 7) {
            response->setData(R"({"id":7,"name":"alice"})");
        } else {
            response->setData(R"({"error":"not found"})");
        }

        sendMessage(response);
    }
};
```

## 二进制正文

`data` 本身没有类型约束。需要发送二进制结构时可先编码：

```cpp
struct LoginRequest {
    uint64_t userId = 0;
    std::string token;
    REFLECTION(userId, token);
};

LoginRequest value{7, "secret"};

auto message = std::make_shared<crazy::MessageBase>();
message->setCmd(2001);
message->setData(
    crazy::protocol::Converter::Serializable(value));
```

接收方必须根据 `cmd` 选择正确解码器，并在反序列化前验证正文长度。不要直接假定网络正文一定是 UTF-8 文本。

## 强类型内部消息的边界

```cpp
using JobMessage = crazy::InternalMessage<Job>;

auto message = std::make_shared<JobMessage>();
message->setCmd(3001);
message->setValue(Job{42, "compress"});

if (message->getCmd() == 3001) {
    const Job& job = message->getValue();
    run(job);
}
```

当消息以 `MessageBase::ptr` 传递时，调用方需要确认实际类型，或仅在团队内部约定同一命令只使用一种内部类型。强类型消息更适合进程内回调，不建议跨动态库边界传递。

相关文档：[Actor](actor.md)、[二进制协议](protocol.md)、[JSON 序列化](json.md)、[Encoder](encoder.md)、[Decoder](decoder.md)、[Session](session.md)。
