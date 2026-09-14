# REFLECTION

[返回文档目录](../README.md)

`REFLECTION(...)` 为结构体生成 JSON 和二进制协议所需的字段遍历代码。当前最多支持 60 个字段。

## 声明反射字段

```cpp
#include "crazy/reflection.h"

struct User {
    uint64_t id = 0;
    std::string name;
    std::vector<std::string> roles;

    REFLECTION(id, name, roles);
};
```

宏会生成 `to_protocol`、`from_protocol`、`to_json` 和 `from_json`，因此同一结构体可以同时用于：

```cpp
User user{7, "alice", {"admin"}};

const std::string json = crazy::json::Converter::Serializable(user);
const std::string binary = crazy::protocol::Converter::Serializable(user);
```

字段宏必须写在结构体或类的公开区域。字段名称需要保持稳定，因为它会作为 JSON key 使用；修改字段名可能破坏旧数据的兼容性。

## 宏展开能力

| 生成接口 | 使用方 | 说明 |
|----------|--------|------|
| `to_json` | `json::Converter` | 把字段写入 JSON 对象 |
| `from_json` | `json::Converter` | 从 JSON 对象读取字段 |
| `to_protocol` | `protocol::Converter` | 依次写入二进制字段 |
| `from_protocol` | `protocol::Converter` | 按相同顺序读取字段 |

字段顺序在二进制协议中十分重要。JSON 可以按名称读取，但二进制读取必须与写入顺序严格一致。

## 嵌套结构

```cpp
struct Address {
    std::string city;
    std::string country;
    REFLECTION(city, country);
};

struct User {
    uint64_t id = 0;
    std::string name;
    Address address;
    std::vector<std::string> tags;
    REFLECTION(id, name, address, tags);
};

User user{
    7,
    "alice",
    {"Shanghai", "CN"},
    {"admin", "developer"},
};

const std::string json =
    crazy::json::Converter::Serializable(user);
User decoded =
    crazy::json::Converter::Deserializable<User>(json);
```

`Address` 也必须声明 `REFLECTION`，否则外层结构无法遍历其字段。

## 版本演进

```cpp
struct UserV1 {
    uint64_t id = 0;
    std::string name;
    REFLECTION(id, name);
};

struct UserV2 {
    uint64_t id = 0;
    std::string name;
    std::optional<std::string> email;
    REFLECTION(id, name, email);
};
```

JSON 新增可选字段通常可以兼容旧数据。二进制协议没有字段名和版本协商，新增、删除或排序字段都会改变线格式。建议为二进制持久化数据增加显式 schema 版本字段。

## 限制

- 最多支持 60 个字段。
- 字段必须能被默认构造和赋值，具体约束取决于底层序列化实现。
- 私有字段不能通过宏直接反射。
- 指针、裸引用和复杂生命周期对象不适合直接反射。
- 多态基类不会自动记录派生类型。

相关文档：[JSON 序列化](json.md)、[二进制协议](protocol.md)、[MessageBase](message.md)。
