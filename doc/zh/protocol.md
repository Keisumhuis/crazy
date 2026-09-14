# 二进制协议

[返回文档目录](../README.md)

`crazy::protocol` 提供带类型标记的二进制序列化和反序列化，适合内部服务通信和持久化。

## 手动读写

```cpp
#include "crazy/protocol.h"

crazy::protocol::Serialize writer;
writer << int32_t(7) << std::string("alice") << true;

crazy::protocol::Deserialize reader;
reader.setBuffer(writer.getBuffer());

int32_t id = 0;
std::string name;
bool enabled = false;
reader >> id >> name >> enabled;
```

## 自定义类型

```cpp
struct Item {
    uint64_t id = 0;
    std::string name;
    REFLECTION(id, name);
};

Item item{1, "book"};
const std::string payload =
    crazy::protocol::Converter::Serializable(item);
Item copy =
    crazy::protocol::Converter::Deserializable<Item>(payload);
```

支持基础类型、枚举、C 数组、STL 容器和反射类型。接收外部数据前仍应设置消息长度上限并校验命令与业务字段，避免把不可信长度直接用于分配。

## 处理流程

`Serialize` 使用流式 `operator<<` 写入内部 `Buffer`，`Deserialize` 使用 `operator>>` 按相同顺序读取。类型标记用于在读取时区分基础标量、字符串、容器和反射对象。

```cpp
crazy::protocol::Serialize writer;
writer << uint8_t(1)
       << int32_t(-7)
       << std::string("alice")
       << std::vector<int>{1, 2, 3};

crazy::protocol::Deserialize reader;
reader.setBuffer(writer.getBuffer());

uint8_t version = 0;
int32_t id = 0;
std::string name;
std::vector<int> values;
reader >> version >> id >> name >> values;
```

写入顺序和读取顺序必须一致。读取错误不应继续使用部分初始化的对象。

## 自定义容器

```cpp
struct Metric {
    std::string name;
    double value = 0.0;
    REFLECTION(name, value);
};

struct Snapshot {
    uint64_t timestamp = 0;
    std::map<std::string, Metric> values;
    REFLECTION(timestamp, values);
};

Snapshot snapshot{
    crazy::GetCurrentMS(),
    {{"cpu", {"cpu.load", 0.42}}},
};

const std::string binary =
    crazy::protocol::Converter::Serializable(snapshot);
Snapshot copy =
    crazy::protocol::Converter::Deserializable<Snapshot>(binary);
```

## 存储到 MessageBase

```cpp
auto message = std::make_shared<crazy::MessageBase>();
message->setCmd(2001);
message->setData(
    crazy::protocol::Converter::Serializable(snapshot));
```

接收端按 `cmd` 选择固定 schema：

```cpp
if (message->getCmd() == 2001) {
    auto snapshot =
        crazy::protocol::Converter::Deserializable<Snapshot>(
            message->getData());
}
```

## 兼容性与安全

- 二进制协议没有字段名，schema 变更必须由版本号管理。
- 不同编译器、平台和字节序下的原始布局不应被视为稳定。
- 集合大小和字符串长度应在反序列化前受最大限制约束。
- 不要对不可信输入无限扩容。
- 生产协议建议保留固定 magic、版本和长度字段。

相关文档：[REFLECTION](reflection.md)、[MessageBase](message.md)、[Encoder](encoder.md)、[Decoder](decoder.md)、[Session](session.md)。
