# JSON 序列化

[返回文档目录](../README.md)

`crazy::json` 支持基础类型、标准容器、`std::optional` 和带 `REFLECTION` 的自定义类型。

## 写入 JSON

```cpp
#include "crazy/json.h"

crazy::json::Serialise writer;
writer.add_from("name", std::string("alice"));
writer.add_from("age", 25);
writer.add_from("tags", std::vector<std::string>{"c++", "server"});

const std::string text = writer.getString();
```

## 自定义类型

```cpp
struct User {
    std::string name;
    int32_t age = 0;
    std::vector<std::string> tags;
    REFLECTION(name, age, tags);
};

User user{"tom", 18, {"dev"}};
const std::string text = crazy::json::Converter::Serializable(user);
User copy = crazy::json::Converter::Deserializable<User>(text);
```

支持的基础能力包括标量、字符串、C 数组、`std::array`、`vector`、`deque`、`list`、`set`、`unordered_set`、`map`、`unordered_map`、`stack` 和 `optional`。

## 支持的数据形态

| C++ 类型 | JSON 形态 |
|----------|-----------|
| 整数、浮点、布尔 | 对应数字或布尔值 |
| `std::string` | 字符串 |
| `std::optional<T>` | 有值时写 `T`，空值时写 `null` |
| 顺序容器 | 数组 |
| `set` / `unordered_set` | 数组，顺序取决于容器 |
| `map` / `unordered_map` | 对象 |
| 反射类型 | 字段名到字段值的对象 |

`map` 的字符串键会直接作为 JSON key；整数键会转换为字符串。序列化关联容器时应明确是否允许相同 JSON key 的冲突。

## 完整业务对象

```cpp
struct Address {
    std::string city;
    std::string street;
    REFLECTION(city, street);
};

struct Profile {
    uint64_t id = 0;
    std::string name;
    std::optional<std::string> email;
    std::vector<Address> addresses;
    std::map<std::string, std::string> attributes;
    REFLECTION(id, name, email, addresses, attributes);
};

Profile profile{
    7,
    "alice",
    "alice@example.com",
    {{"Shanghai", "West Road"}},
    {{"role", "admin"}},
};

const std::string json =
    crazy::json::Converter::Serializable(profile);
Profile restored =
    crazy::json::Converter::Deserializable<Profile>(json);
```

## 手动构造列表

```cpp
crazy::json::Serialise writer;
writer.add_from("items", std::vector<int>{1, 2, 3});
writer.add_from("enabled", true);
writer.add_from("timeout", 12.5);
const std::string payload = writer.getString();
```

## 错误与兼容性

- 反序列化类型不匹配时，应先在业务层验证数据，不要直接信任输入。
- 可选字段加入 `std::optional` 可以兼容旧数据中的缺失值。
- 删除或重命名反射字段会破坏旧数据兼容性。
- 大数组会整体载入内存，不适合直接处理超大文件。
- JSON 数字精度受底层解析器限制，金额等精确值建议使用字符串或整数分。

相关文档：[REFLECTION](reflection.md)、[二进制协议](protocol.md)、[HTTP 服务端](http-server.md)。
