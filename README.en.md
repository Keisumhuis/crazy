# crazy

A lightweight C++ foundational framework library, providing common modules such as Actor model, network communication, logging, configuration, encryption, MySQL, and more.

- **Language Standard**: C++17  |  **Build**: CMake 3.15+  |  **Version**: 1.0.2

> [中文文档](README.md)

## Feature Modules

| Module | Description |
|--------|-------------|
| Actor Model | Message-driven concurrency model, independent threads + message queues, supports sending messages to specific Actors via command line |
| Network Communication | TCP server/client, Socket wrapper, Selector multiplexing, local sockets |
| Logging System | Multi-level logging (trace~fatal), supports custom formatters and output targets |
| Configuration Management | INI format config file parsing, supports multi-type reading |
| Encryption Module | Base64 encoding/decoding, MD5 hashing |
| MySQL | Connection pool, prepared statements, exception system |
| Memory Mapping | Cross-platform mmap file mapping and mmap_vector container |
| JSON | Serialization/deserialization, supports STL containers and custom types |
| Thread Utilities | Thread pool, atomic lock, conditional mutex, file lock, MVCC lock |
| Basic Utilities | DateTime, UUID, URI, Buffer, Reflection, Singleton, KeyValuePair, command-line parsing |

## Actor Model

Each Actor runs in an independent thread and communicates via message queues, avoiding shared-state concurrency issues.

**Core Classes**: `ActorInterface`, `Application`

**Lifecycle**: `init()` → `start()` → `run()` → `stop()`

**Message Routing**: `addRouteTable(from, to, cmd)` registers routing rules, messages are automatically delivered by source + cmd.

## Sending Commands to a Specific Actor Thread via CLI

Start the service process `./exe`, then use `./exe -s "actor_name@command"` to send commands to a specific Actor thread and get a response. The command format is `actor_name@command`, with `@` separating the Actor name and the command content.

Custom Actor command example:

```cpp
class MyActor : public crazy::ActorInterface {
    using crazy::ActorInterface::ActorInterface;
    std::map<std::string, std::string> helps() override {
        return {{"query", "query data"}, {"clear", "clear cache"}};
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

Command-line interaction:

```bash
# Start the service (one terminal)
$ ./exe

# Send a command (another terminal)
$ ./exe -s my_actor@query
# → result: ok

$ ./exe -s my_actor@clear
# → cleared

$ ./exe -s my_actor@print_message_queue_size
# → current message queue size: 0

# List all available commands
$ ./exe --help
```

## Module Examples

### Logging System

```cpp
#include "crazy.h"

// System logger (6 levels)
CRAZY_SYSTEM_TRACE() << "trace message";
CRAZY_SYSTEM_DEBUG() << "debug value=" << 42;
CRAZY_SYSTEM_INFO()  << "server started";
CRAZY_SYSTEM_WARN()  << "low memory";
CRAZY_SYSTEM_ERROR() << "connection failed";
CRAZY_SYSTEM_FATAL() << "critical error";

// Custom logger
CRAZY_LOG(my_logger, crazy::LoggerLevel::info) << "custom log";
```

### Configuration Management

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

### Encryption Module

```cpp
auto enc = crazy::Base64::encryption("hello world");  // aGVsbG8gd29ybGQ=
auto dec = crazy::Base64::decryption(enc);             // hello world

auto md5 = crazy::MD5::encryption("hello");  // 5d41402abc4b2a76b9719d911017c592
```

### MySQL Connection Pool

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

### JSON Serialization

```cpp
crazy::json::Serialise ser;
ser.add_from("name", std::string("kesium"));
ser.add_from("age", 25);
ser.add_from("active", true);
std::string json = ser.getString();
// {"name":"kesium","age":25,"active":true}
```

### DateTime

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

### Memory Mapping

```cpp
crazy::MmapInterface mmap("./data.bin", 1024 * 1024);  // 1MB
void* ptr = mmap.data();
memcpy(ptr, "hello", 5);
mmap.sync();  // flush to disk

// mmap_vector usage is similar to std::vector
crazy::mmap_vector<int32_t> vec("./vec.bin", 100);
vec[0] = 42;
vec.push_back(100);
```

### Buffer

```cpp
crazy::Buffer buf;
buf.append("hello", 5);
buf.append(" world", 6);
// 11 bytes readable
char data[12] = {};
memcpy(data, buf.readBegin(), buf.readableCount());
buf.readed(buf.readableCount());
```

### Thread Pool

```cpp
auto pool = std::make_shared<crazy::ThreadPool>(4);
pool->start();
pool->enqueueRunnable([]() {
    CRAZY_SYSTEM_INFO() << "task running in thread pool";
});
```

### KeyValuePair

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

### Reflection

The `REFLECTION(...)` macro automatically generates `to_protocol` / `from_protocol` and `to_json` / `from_json` methods for structs, enabling one-click serialization to binary protocol and JSON format without writing serialization code manually. Supports up to 60 member fields.

```cpp
struct UserInfo {
    std::string name;
    int32_t age;
    double score;
    std::vector<std::string> tags;
    REFLECTION(name, age, score, tags);  // One line for serialization
};

UserInfo user{"kesium", 25, 99.5, {"c++", "gamedev"}};

// Serialize to binary protocol
std::string bin = crazy::protocol::Converter::Serializable(user);

// Deserialize
UserInfo user2 = crazy::protocol::Converter::Deserializable<UserInfo>(bin);

// Serialize to JSON
crazy::json::Serialise jsonSer;
user.to_json(jsonSer);
std::string json = jsonSer.getString();
// {"name":"kesium","age":25,"score":99.5,"tags":["c++","gamedev"]}
```

### Socket Networking

```cpp
// Server
crazy::Socket server;
server.listen(8080);
auto client = server.accept();
char buf[1024];
int n = client->recv(buf, sizeof(buf));
client->send("HTTP/1.1 200 OK\r\n\r\nhello", 25);

// Client
crazy::Socket sock;
sock.connect("127.0.0.1", 8080);
sock.send("ping", 4);
```

## Build

```bash
# Windows
build_crazy_framework.bat

# Linux
chmod +x build_crazy_framework.sh && ./build_crazy_framework.sh

# Manual
mkdir build && cd build && cmake .. && cmake --build .
```