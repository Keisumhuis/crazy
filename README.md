# crazy

轻量级 C++ 基础框架库，提供 Actor 模型、网络通信、日志、配置、加密、MySQL 等常用功能模块。

- **语言标准**：C++17  |  **构建**：CMake 3.15+  |  **版本**：1.0.2

## 功能模块

| 模块 | 说明 |
|------|------|
| Actor 模型 | 消息驱动的并发模型，独立线程+消息队列，支持命令行向指定 Actor 发消息 |
| 网络通信 | TCP 服务端/客户端、Socket 封装、Selector 多路复用、本地 Socket |
| 日志系统 | 多级别日志（trace~fatal），支持自定义格式器和输出目标 |
| 配置管理 | INI 格式配置文件解析，支持多类型读取 |
| 加密模块 | Base64 编解码、MD5 哈希 |
| MySQL | 连接池、预处理语句、异常体系 |
| 内存映射 | 跨平台 mmap 文件映射与 mmap_vector 容器 |
| JSON | 序列化/反序列化，支持 STL 容器和自定义类型 |
| 线程工具 | 线程池、原子锁、条件互斥锁、文件锁、MVCC 锁 |
| 基础工具 | 日期时间、UUID、URI、Buffer、反射、单例、键值对、命令行解析 |

## Actor 模型

每个 Actor 运行在独立线程，通过消息队列通信，避免共享状态并发问题。

**核心类**：`ActorInterface`、`Application`

**生命周期**：`init()` → `start()` → `run()` → `stop()`

**消息路由**：`addRouteTable(from, to, cmd)` 注册规则，消息按 source+cmd 自动投递。

## 命令行向指定 Actor 线程发消息

先启动服务进程 `./exe`，然后通过 `./exe -s "actor_name@command"` 向指定 Actor 线程发送指令并获得响应。命令格式为 `actor_name@command`，以 `@` 分隔 Actor 名称和命令内容。

自定义 Actor 命令示例：

```cpp
class MyActor : public crazy::ActorInterface {
    using crazy::ActorInterface::ActorInterface;
    std::map<std::string, std::string> helps() override {
        return {{"query", "查询数据"}, {"clear", "清除缓存"}};
    }
    void handleCommandLineMessgaBase(
        crazy::MessageBase::ptr req, crazy::MessageBase::ptr rsp) override {
        auto cmds = crazy::StringUtil::Split(req->getData());
        if ("query" == cmds[0]) rsp->setData("result: ok");
        else if ("clear" == cmds[0]) rsp->setData("cleared");
    }
    void handleMessgaBase(crazy::MessageBase::ptr msg) override {
        if (msg->getCmd() == 999) {
            auto rsp = msg->createResponse();
            rsp->setData("reply from server");
            sendMessage(rsp);
        }
    }
};

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<crazy::ServiceActor>("service");
    app.registerActor<MyActor>("my_actor");
    app.addRouteTable("my_actor", "service", 999);
    app.exec();
}
```

命令行交互：

```bash
# 启动服务（一个终端）
$ ./exe

# 发送命令（另一个终端）
$ ./exe -s "my_actor@query"
# → result: ok

$ ./exe -s "my_actor@clear"
# → cleared

$ ./exe -s "my_actor@print_message_queue_size"
# → 当前消息队列长度：0

# 查看所有可用命令
$ ./exe --help
```

## 各模块实例代码

### 日志系统

```cpp
#include "crazy.h"

// 系统日志（6个级别）
CRAZY_SYSTEM_TRACE() << "trace message";
CRAZY_SYSTEM_DEBUG() << "debug value=" << 42;
CRAZY_SYSTEM_INFO()  << "server started";
CRAZY_SYSTEM_WARN()  << "low memory";
CRAZY_SYSTEM_ERROR() << "connection failed";
CRAZY_SYSTEM_FATAL() << "critical error";

// 自定义日志器
CRAZY_LOG(my_logger, crazy::LoggerLevel::info) << "custom log";
```

### 配置管理

```cpp
// config.ini:
// [server]
// port=8080
// host=0.0.0.0

crazy::Config::LoadConfigFile("./config.ini");
auto port = crazy::Config::GetIntager("server", "port", 80);
auto host = crazy::Config::GetString("server", "host", "127.0.0.1");
auto debug = crazy::Config::GetBoolean("server", "debug", false);
```

### 加密模块

```cpp
auto enc = crazy::Base64::encryption("hello world");  // aGVsbG8gd29ybGQ=
auto dec = crazy::Base64::decryption(enc);             // hello world

auto md5 = crazy::MD5::encryption("hello");  // 5d41402abc4b2a76b9719d911017c592
```

### MySQL 连接池

```cpp
crazy::MySQLConnectionPoolConfig cfg;
cfg.host = "127.0.0.1";
cfg.user = "root";
cfg.password = "123456";
cfg.database = "test";
cfg.max_connections = 10;

auto pool = std::make_shared<crazy::MySQLConnectionPool>(cfg);
pool->start();

auto conn = pool->getConnection();
auto result = conn->query("SELECT * FROM users WHERE id = 1");
while (result->next()) {
    CRAZY_SYSTEM_INFO() << "name: " << result->getString("name");
}
```

### JSON 序列化

```cpp
crazy::json::Serialise ser;
ser.add_from("name", std::string("kesium"));
ser.add_from("age", 25);
ser.add_from("active", true);
std::string json = ser.getString();
// {"name":"kesium","age":25,"active":true}
```

### 日期时间

```cpp
auto now = crazy::DateTime::now();
CRAZY_SYSTEM_INFO() << now.toString();              // 2026-06-28 12:00:00
CRAZY_SYSTEM_INFO() << now.year() << "-" << now.month();

auto dt = crazy::DateTime::fromString("2026-01-01 08:00:00");
dt.addDays(30).addHours(2);
bool isLeap = crazy::DateTime::isLeapYear(2026);    // false
```

### UUID

```cpp
uuids::uuid id = uuids::uuid_system_generator{}();
std::string str = uuids::to_string(id);
// 550e8400-e29b-41d4-a716-446655440000
```

### 内存映射

```cpp
crazy::MmapInterface mmap("./data.bin", 1024 * 1024);  // 1MB
void* ptr = mmap.data();
memcpy(ptr, "hello", 5);
mmap.sync();  // 刷入磁盘

// mmap_vector 用法类似 std::vector
crazy::mmap_vector<int32_t> vec("./vec.bin", 100);
vec[0] = 42;
vec.push_back(100);
```

### Buffer 缓冲区

```cpp
crazy::Buffer buf;
buf.append("hello", 5);
buf.append(" world", 6);
// 可读 11 字节
char data[12] = {};
memcpy(data, buf.readBegin(), buf.readableCount());
buf.readed(buf.readableCount());
```

### 线程池

```cpp
auto pool = std::make_shared<crazy::ThreadPool>(4);
pool->start();
pool->enqueueRunnable([]() {
    CRAZY_SYSTEM_INFO() << "task running in thread pool";
});
```

### 键值对

```cpp
crazy::KeyValuePair kv("score", 100);
kv.setValue(std::string("excellent"));
std::string val = kv.value();  // excellent

std::vector<crazy::KeyValuePair> rows = {
    {"name", std::string("kesium")},
    {"age", 25},
};
std::string csv = crazy::datacsv(rows);  // kesium,25
```

### 反射

`REFLECTION(...)` 宏自动为结构体生成 `to_protocol` / `from_protocol` 和 `to_json` / `from_json` 方法，实现一键序列化到二进制协议和 JSON 格式，无需手写序列化代码。最多支持 60 个成员字段。

```cpp
struct UserInfo {
    std::string name;
    int32_t age;
    double score;
    std::vector<std::string> tags;
    REFLECTION(name, age, score, tags);  // 一行搞定序列化
};

UserInfo user{"kesium", 25, 99.5, {"c++", "gamedev"}};

// 序列化到二进制协议
std::string bin = crazy::protocol::Converter::Serializable(user);

// 反序列化
UserInfo user2 = crazy::protocol::Converter::Deserializable<UserInfo>(bin);

// 序列化到 JSON
crazy::json::Serialise jsonSer;
user.to_json(jsonSer);
std::string json = jsonSer.getString();
// {"name":"kesium","age":25,"score":99.5,"tags":["c++","gamedev"]}
```

### Socket 网络

```cpp
// 服务端
crazy::Socket server;
server.listen(8080);
auto client = server.accept();
char buf[1024];
int n = client->recv(buf, sizeof(buf));
client->send("HTTP/1.1 200 OK\r\n\r\nhello", 25);

// 客户端
crazy::Socket sock;
sock.connect("127.0.0.1", 8080);
sock.send("ping", 4);
```

## 编译构建

```bash
# Windows
build_crazy_framework.bat

# Linux
chmod +x build_crazy_framework.sh && ./build_crazy_framework.sh

# 手动
mkdir build && cd build && cmake .. && cmake --build .
```

## 运行测试

`test_actor` `test_actor_client` `test_config` `test_logger` `test_protocol` `test_json` `test_encryption` `test_date_time` `test_lock` `test_mvvc_lock_wapper` `test_localsocket` `test_mmap` `test_key_value_pair`

## 文档

```bash
doxygen Doxyfile   # 生成到 doc/ 目录
```

## 许可证

详见 [LICENSE](LICENSE) 文件。