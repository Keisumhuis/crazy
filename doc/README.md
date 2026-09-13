# crazy 中文文档

`crazy` 是一个 C++17 静态库，面向常用服务端基础能力：Actor 并发模型、TCP/本地 Socket、日志、INI 配置、JSON 和二进制协议、MySQL/ClickHouse 连接池、mmap、锁、时间、URI、UUID 与路径工具等。

> English: [README.en.md](README.en.md)

## 功能总览

| 模块 | 主要能力 | 主要头文件 |
|------|----------|------------|
| Application | 应用入口、Actor 注册、命令行控制、路由、后台任务、数据库连接池入口 | `application.h` |
| Actor | 独立线程、消息队列、异步任务队列、命令行消息处理、普通消息处理 | `actor_interface.h`, `message_base.h` |
| Daemon | `-d` 守护模式参数解析、supervisor/worker 角色、worker 拉起 | `daemon.h` |
| 网络通信 | IPv4 Socket、本地 Socket、Acceptor/Connection、Session、Encoder/Decoder、Selector、定时器、ServiceActor/ClientActor | `net/*.h` |
| HTTP | HTTP/1.1 服务端路由、延迟响应、静态文件目录、HTTP 同步客户端、multipart/form-data 上传、WebSocket 服务端/客户端 | `http_application.h`, `net/http/*.h` |
| SMTP | 明文 SMTP 客户端、AUTH PLAIN/LOGIN 认证、纯文本邮件发送 | `net/smtp/*.h` |
| 日志 | trace/debug/info/warn/error/fatal，多 Logger，控制台/文件 Appender，自定义格式 | `logger.h` |
| 配置 | INI 文件/目录加载，字符串、整数、浮点、布尔读取，默认值，section 检查 | `config.h` |
| 加密 | Base64 编码/解码、MD5 哈希 | `encryption/base64.h`, `encryption/md5.h` |
| JSON | 基础类型、STL 容器、optional、自定义类型序列化/反序列化 | `json.h`, `reflection.h` |
| 二进制协议 | 基础类型、枚举、数组、STL 容器、自定义类型二进制编解码 | `protocol.h`, `reflection.h` |
| 数据库 | MySQL/ClickHouse 连接、查询、格式化 SQL、事务、连接池、健康检查 | `mysql/*.h`, `clickhouse/*.h` |
| 内存与文件 | Buffer、跨平台 mmap、持久化 `MmapVector`、文件锁 | `buffer.h`, `mmap/*.h`, `file_lock.h` |
| 并发工具 | 线程池、原子锁、条件互斥锁、MVCC 双版本读写包装 | `thread_pool.h`, `atomic_lock.h`, `cond_mutex.h`, `mvcc_lock_wrapper.h` |
| 基础工具 | DateTime、TimeZone、URI、UUID、端序转换、字符串/路径/线程名工具、单例、不可拷贝基类、命令行 parser | `date_time.h`, `time_zone.h`, `uri.h`, `uuid.h`, `byte_order.h`, `utils.h`, `singleton.h`, `command_line.h` |
| 第三方组件 | RapidJSON、GSL、MySQL client、ClickHouse client 头文件/预编译库 | `src/crazy/rapidjson`, `src/crazy/gsl`, `src/third_party` |

## 编译构建

Windows:

```bat
build_crazy_framework.bat
```

Linux:

```bash
chmod +x build_crazy_framework.sh
./build_crazy_framework.sh
```

手动构建：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

CMake 会生成静态库 `crazy`、编译 `tests/` 下的测试程序。Windows 脚本按 x86/x64、Debug/Release 构建；Linux 脚本按 x86/x64、Debug/Release 构建。

## 接入项目

项目可以直接包含聚合头文件：

```cpp
#include "crazy.h"
```

CMake 中链接库目标：

```cmake
add_subdirectory(path/to/crazy)
target_link_libraries(your_target PRIVATE crazy)
target_include_directories(your_target PRIVATE path/to/crazy/src)
```

如需数据库模块，确保 `src/third_party/MySQL` 和 `src/third_party/clickhouse` 中对应平台/架构的库存在。CMake 会按系统和架构自动选择库文件。

## Application 与 Actor

`Application` 是运行入口，继承自 `ActorInterface`，负责注册 Actor、启动线程、命令行服务、消息路由、线程池任务和数据库连接池。

```cpp
class Worker : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

    std::map<std::string, std::string> helps() override {
        return {{"ping", "返回 pong"}};
    }

protected:
    void handleCommandLineMessageBase(
        crazy::MessageBase::ptr request,
        crazy::MessageBase::ptr response) override {
        if (request->getData() == "ping") {
            response->setData("pong");
        }
    }

    void handleMessageBase(crazy::MessageBase::ptr message) override {
        if (message->getCmd() == 100) {
            auto response = message->createResponse();
            response->setData("handled");
            sendMessage(response);
        }
    }
};

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<Worker>("worker");
    app.enqueueRunnable([] {
        CRAZY_SYSTEM_INFO() << "background task";
    });
    app.exec();
}
```

常用接口：

- `registerActor<T>("name")`：注册 Actor。
- `addRouteTable(from, to, cmd)`：把来自 `from` 且命令为 `cmd` 的消息路由给 `to`。
- `enqueueRunnable(fn)`：提交后台任务到内部线程池。
- `getMySQLConnectionPool()` / `getClickHouseConnectionPool()`：获取连接池。
- `stopService()`：停止服务。

## 命令行向 Actor 发消息

服务进程运行后，可以用同一个可执行文件向指定 Actor 发送命令：

```bash
./your_app -s worker@ping
```

格式为：

```text
actor_name@command
```

Actor 需要重写 `helps()` 和 `handleCommandLineMessageBase()`。内置命令可通过：

```bash
./your_app --help
```

查看。

## MessageBase

`MessageBase` 是 Actor、网络 Session 和协议编解码共享的消息对象。

```cpp
auto msg = std::make_shared<crazy::MessageBase>();
msg->setSource("client");
msg->setSessionId(1);
msg->setCmd(100);
msg->setComment("demo");
msg->setData("payload");

auto rsp = msg->createResponse();
rsp->setData("ok");
```

需要携带强类型内部数据时使用 `InternalMessage<T>`：

```cpp
auto msg = std::make_shared<crazy::InternalMessage<int>>();
msg->setCmd(200);
msg->setValue(42);
```

## Daemon

`Daemon` 提供守护进程参数解析和 supervisor/worker 运行框架。

```cpp
int main(int argc, char** argv) {
    auto daemonArgs = crazy::Daemon::ParseArguments(argc, argv);
    if (daemonArgs.enabled && daemonArgs.role == crazy::DaemonRole::supervisor) {
        return crazy::Daemon::RunSupervisor(daemonArgs.childArgs);
    }

    crazy::Application app(argc, argv);
    app.exec();
}
```

角色：

- `DaemonRole::none`：普通进程。
- `DaemonRole::supervisor`：守护进程，负责检查并拉起 worker。
- `DaemonRole::worker`：业务进程。

## 网络通信

### Socket

```cpp
crazy::Socket server;
server.listen(8080, "0.0.0.0");
auto client = server.accept();

char buf[1024] = {};
int n = client->recv(buf, sizeof(buf));
client->send(buf, n);
```

```cpp
crazy::Socket sock;
sock.connect("127.0.0.1", 8080);
sock.send("ping", 4);
```

`SocketInterface` 还提供 `active()`、`close()`、`getOption()`、`setOption()`、`socket()` 等基础操作。

### LocalSocket

本地 Socket 用于同机进程间通信，Application 的命令行控制也基于它。

```cpp
crazy::LocalSocket server;
server.listen("demo.cmd");
auto client = server.accept();
```

```cpp
crazy::LocalSocket client;
client.connect("demo.cmd");
client.send("hello", 5);
```

### Selector、定时器与时间点任务

`Selector` 封装 epoll/wepoll，支持读写事件、唤醒、循环定时器和每日固定时间任务。

```cpp
class Loop : public crazy::Selector {
public:
    void start(int fd) {
        registerEvent(fd, crazy::SelectorEventType::read, [] {
            CRAZY_SYSTEM_INFO() << "readable";
        });
        registerTimer("tick", 1000, [] {
            CRAZY_SYSTEM_INFO() << "every second";
        });
        registerTimePointTask("midnight", 0, 0, 0, [] {
            CRAZY_SYSTEM_INFO() << "daily task";
        });
        select();
    }
};
```

### ServiceActor 与 ClientActor

`ServiceActor` 管理服务端监听、Session、心跳和消息收发；`ClientActor` 管理客户端连接、断线和心跳。配置来自 INI，例如：

```ini
[service]
address = "0.0.0.0"
port = 9090
heartbeat_interval = 5000

[client]
address = "127.0.0.1"
port = 9090
heartbeat_interval = 5000
```

注册方式：

```cpp
crazy::Application app(argc, argv);
app.registerActor<crazy::ServiceActor>("service");
app.registerActor<crazy::ClientActor>("client");
app.exec();
```

### Encoder、Decoder 与 Session

`Encoder` 把 `MessageBase` 写入 `Buffer`；`Decoder` 从 `Buffer` 解析完整消息，并支持最大报文长度和完成/异常回调。`Session` 在 Socket 之上组合了收发缓冲区、编码器、解码器、断开回调和消息回调。

```cpp
crazy::Buffer buffer;
crazy::Encoder encoder(buffer);
encoder.stringify(msg);

crazy::Decoder decoder(buffer);
decoder.setMaxMessageLength(1024 * 1024);
decoder.registerParseFinishCallback([](crazy::MessageBase::ptr message) {
    CRAZY_SYSTEM_INFO() << message->getData();
});
decoder.parse();
```

### HTTP 与 WebSocket

`HttpApplication` 在 `Application` 基础上提供 HTTP/1.1 服务和 WebSocket 服务。

#### HTTP 服务端

```cpp
#include "crazy/http_application.h"

int main(int argc, char** argv) {
    crazy::HttpApplication app(argc, argv);
    app.listen(8080, "0.0.0.0");

    // 注册 GET 路由（自由函数或 lambda）
    app.registerHttpHandler<crazy::GET>("/", [](crazy::HttpRequest& request, crazy::HttpResponse& response) {
        response.setBody("hello crazy");
    });

    // 注册多方法路由
    app.registerHttpHandler<crazy::GET, crazy::POST>(
        "/user", [](crazy::HttpRequest& request, crazy::HttpResponse& response) {
            response.setBody("user");
        });

    // 注册成员函数路由并绑定对象
    // app.registerHttpHandler<crazy::GET>("/path", &Controller::method, &controller);

    // 注册静态文件目录，默认不显示目录列表
    app.registerStaticDirectory("/static", "./static");

    // 显式开启类似 Nginx autoindex 的 HTML 目录列表
    app.registerStaticDirectory("/browse", "./static", true);

    app.exec();
}
```

处理器签名固定为 `void(HttpRequest&, HttpResponse&)`，通过 `request` 读取方法、URI、请求头和请求体，通过 `response` 设置状态码、响应头和响应体。

默认情况下，处理器退出后会立即发送响应。如果业务需要在异步任务完成后再回复，可以调用 `response.defer()` 获取响应对象，完成业务后设置响应内容并调用 `send()`：

```cpp
app.registerHttpHandler<crazy::GET>("/defer",
    [&app](crazy::HttpRequest&, crazy::HttpResponse& response) {
        auto deferredResponse = response.defer();
        app.enqueueRunnable([deferredResponse]() {
            deferredResponse->setBody("deferred response");
            deferredResponse->send();
        });
    });
```

`send()` 可以从后台任务中调用，框架会把实际 socket 写操作投递回 HTTP 会话所属线程。同一个响应重复调用 `send()` 只会发送一次。

静态目录只处理 `GET` 请求，业务路由优先于静态文件路由。请求路径会按 URL 的目录层级映射到注册目录下的文件，并根据文件扩展名设置常见 `Content-Type`。目录列表默认关闭，目录请求返回 `404`；注册时将第三个参数设为 `true` 后，目录请求会生成 HTML 文件列表，非尾斜杠目录会重定向到带 `/` 的路径。文件不存在返回 `404`。为了防止路径穿越，URL 路径中包含 `.` 或 `..` 段的请求会返回 `403`，符号链接解析后越过注册根目录的路径也会被拒绝。

```cpp
// /static/index.html       -> ./static/index.html
// /static/nested/data.json -> ./static/nested/data.json
app.registerStaticDirectory("/static", "./static");

// GET /browse/ 会返回 HTML 目录列表
app.registerStaticDirectory("/browse", "./static", true);
```

#### HTTP 客户端

`HttpClient` 是同步阻塞客户端，支持常用方法、自定义请求头和 multipart/form-data 上传。

```cpp
#include "crazy/net/http/http_client.h"

crazy::HttpClient client;

// 设置默认请求头
client.setHeader("Authorization", "Bearer token");

// GET 请求
client.get("http://127.0.0.1:8080/hello", [](crazy::HttpResponse::ptr response) {
    CRAZY_SYSTEM_INFO() << response->body();
});

// POST 请求（带 body）
client.post("http://127.0.0.1:8080/submit", "body content",
    [](crazy::HttpResponse::ptr response) {
        CRAZY_SYSTEM_INFO() << response->body();
    });

// 通用请求（DELETE / HEAD / OPTIONS / PATCH 等）
client.request(crazy::HttpMethod::DELETE, "http://127.0.0.1:8080/user/1",
    "", [](crazy::HttpResponse::ptr response) {});

// multipart/form-data 上传
std::vector<crazy::FormDataField> fields = {{"name", "alice"}};
std::vector<crazy::FormDataFile> files = {
    {"file", "a.txt", "text/plain", "file content"},
};
client.postForm("http://127.0.0.1:8080/upload", fields, files,
    [](crazy::HttpResponse::ptr response) {});
```

#### WebSocket 服务端

```cpp
crazy::HttpApplication app(argc, argv);
app.listen(8080, "0.0.0.0");

// 连接建立回调
app.registerWebSocketConnectCallback([](crazy::HttpSession::ptr session) {
    session->sendText("welcome");
});

// 消息回调（收到消息后回显）
app.registerWebSocketMessageCallback(
    [](crazy::HttpSession::ptr session, const std::string& data,
       crazy::HttpSession::WebSocketOpCode opcode) {
        if (opcode == crazy::HttpSession::WebSocketOpCode::text) {
            session->sendText(data);
        }
    });

// 关闭回调
app.registerWebSocketCloseCallback([]() {
    CRAZY_SYSTEM_INFO() << "closed";
});

app.exec();
```

#### WebSocket 客户端

```cpp
#include "crazy/net/http/websocket_client.h"

crazy::WebSocketClient ws;

ws.registerConnectCallback([]() {
    CRAZY_SYSTEM_INFO() << "connected";
});
ws.registerMessageCallback([](const std::string& data, crazy::WebSocketClient::OpCode opcode) {
    CRAZY_SYSTEM_INFO() << "received: " << data;
});
ws.registerCloseCallback([]() {
    CRAZY_SYSTEM_INFO() << "closed";
});

if (ws.connect("ws://127.0.0.1:8080/chat")) {
    ws.sendText("hello");
    ws.run();  // 阻塞接收循环，直到连接关闭
}
```

### SMTP 客户端

`SmtpClient` 提供明文 SMTP 会话、AUTH PLAIN / AUTH LOGIN 认证和纯文本邮件发送。

```cpp
#include "crazy/net/smtp/smtp_client.h"

crazy::SmtpClient client;
client.setClientName("localhost");

// 连接服务器（默认端口 25）并完成 greeting/hello
if (!client.connect("smtp.example.com")) {
    CRAZY_SYSTEM_ERROR() << "connect failed";
    return;
}

// 认证（默认 AUTH LOGIN，可选 AUTH PLAIN）
client.login("user@example.com", "<password>", crazy::SmtpClient::AuthType::login);

// 发送纯文本邮件
client.sendMail("from@example.com",
                {"to@example.com"},
                "subject",
                "body",
                {"cc@example.com"},
                {"bcc@example.com"});

client.close();
```

也可以通过 `SmtpClient::MailMessage` 结构体发送带附加头部的邮件，
`sendMail(const MailMessage&)` 与 `buildMailData()` 支持自定义邮件内容。

## 日志系统

支持 `trace`、`debug`、`info`、`warn`、`error`、`fatal` 六级日志。

```cpp
CRAZY_SYSTEM_TRACE() << "trace";
CRAZY_SYSTEM_DEBUG() << "debug";
CRAZY_SYSTEM_INFO()  << "info";
CRAZY_SYSTEM_WARN()  << "warn";
CRAZY_SYSTEM_ERROR() << "error";
CRAZY_SYSTEM_FATAL() << "fatal";

CRAZY_ROOT_INFO() << "root logger";
CRAZY_LOG(custom, crazy::LoggerLevel::info) << "custom logger";
```

自定义 Logger：

```cpp
auto logger = LOGGER(api);
logger->setLevel(crazy::LoggerLevel::debug);
logger->setFormatter("%d %c %p %f:%l %m");
logger->addAppender(std::make_shared<crazy::LoggerConsoleAppenderImpl>());
logger->addAppender(std::make_shared<crazy::LoggerFileAppenderImpl>("./logs"));
```

格式组件包括日期、分类、级别、函数、行号和消息。

## 配置管理

支持加载单个 INI 文件或目录下所有指定后缀文件。

```cpp
crazy::Config::LoadConfigFile("./config.ini");
crazy::Config::LoadConfigPath("./config");

auto host = crazy::Config::GetString("server", "host", "127.0.0.1");
auto port = crazy::Config::GetInteger("server", "port", 8080);
auto debug = crazy::Config::GetBoolean("server", "debug", false);
auto ratio = crazy::Config::GetDouble("server", "ratio", 1.0);

if (crazy::Config::HasSection("server")) {
    crazy::Config::EraseValue("server", "debug");
}
```

未写 section 的配置归入 `global`。

## 加密模块

`Base64` 提供字符串编码/解码，`MD5` 提供字符串哈希。

```cpp
auto encoded = crazy::Base64::encryption("hello world");
auto decoded = crazy::Base64::decryption(encoded);

auto digest = crazy::MD5::encryption("hello");
CRAZY_SYSTEM_INFO() << encoded << ", " << decoded << ", " << digest;
```

## JSON 序列化

`json::Serialise` / `json::Deserialise` 支持基础类型、字符串、数组、`std::array`、`vector`、`deque`、`list`、`set`、`unordered_set`、`map`、`unordered_map`、`stack`、`optional` 和带反射方法的自定义类型。

```cpp
crazy::json::Serialise ser;
ser.add_from("name", std::string("kesium"));
ser.add_from("age", 25);
ser.add_from("tags", std::vector<std::string>{"c++", "server"});
std::string text = ser.getString();
```

自定义结构体配合 `REFLECTION`：

```cpp
struct User {
    std::string name;
    int32_t age = 0;
    std::vector<std::string> tags;
    REFLECTION(name, age, tags);
};

User user{"tom", 18, {"dev"}};
std::string json = crazy::json::Converter::Serializable(user);
User copy = crazy::json::Converter::Deserializable<User>(json);
```

## 二进制协议与反射

`protocol::Serialize` / `protocol::Deserialize` 使用类型标记写入二进制数据，支持基础类型、枚举、C 数组、`std::array`、`deque`、`list`、`map`、`set`、`stack`、`unordered_map`、`unordered_set`、`vector` 和自定义类型。

```cpp
struct Item {
    uint64_t id = 0;
    std::string name;
    REFLECTION(id, name);
};

Item item{1, "book"};
std::string bin = crazy::protocol::Converter::Serializable(item);
Item copy = crazy::protocol::Converter::Deserializable<Item>(bin);
```

也可以手动写入：

```cpp
crazy::protocol::Serialize ser;
ser << int32_t(1) << std::string("hello");

crazy::protocol::Deserialize de;
de.setBuffer(ser.getBuffer());
int32_t id;
std::string text;
de >> id >> text;
```

`REFLECTION(...)` 最多支持 60 个字段，会生成 `to_protocol`、`from_protocol`、`to_json`、`from_json`。

## MySQL

### 直接连接

```cpp
auto conn = std::make_shared<crazy::MySQLConnection>();
if (!conn->connect("127.0.0.1", "root", "<password>", "test", 3306)) {
    CRAZY_SYSTEM_ERROR() << conn->get_error_message();
    return;
}

auto result = conn->exec("SELECT id, name FROM users");
while (auto row = result->row()) {
    CRAZY_SYSTEM_INFO() << row[0] << ", " << row[1];
}
```

### 格式化 SQL、事务和元信息

```cpp
auto result = conn->exec_fmt("SELECT * FROM users WHERE id = %d", 10);

conn->beginTransaction();
conn->exec("UPDATE users SET score = score + 1 WHERE id = 10");
conn->commit();

CRAZY_SYSTEM_INFO() << conn->server_info();
CRAZY_SYSTEM_INFO() << conn->affected_rows();
```

### 预处理语句

```cpp
auto stmt = conn->create_statement();
stmt->prepare("SELECT name FROM users WHERE id = ?");
// 使用 MYSQL_BIND 绑定输入/输出参数：
// stmt->bind_input_param(inputBinds);
// stmt->bind_output_param(outputBinds);
stmt->execute();
stmt->store_result();
```

### 连接池

```cpp
crazy::MySQLConnectionPoolConfig cfg;
cfg.host = "127.0.0.1";
cfg.user = "root";
cfg.password = "<password>";
cfg.database = "test";
cfg.min_connections = 2;
cfg.max_connections = 10;

auto pool = std::make_shared<crazy::MySQLConnectionPool>(cfg);
pool->start();

auto conn = pool->getConnection(3000);
conn->exec("SELECT 1");
CRAZY_SYSTEM_INFO() << pool->getStats();
pool->stop();
```

连接池会维护空闲连接、超时等待、连接有效性检查、自动归还和收缩。

## ClickHouse

### 直接连接与查询

```cpp
auto conn = std::make_shared<crazy::ClickHouseConnection>();
if (conn->connect("127.0.0.1", 9000, "default", "", "default")) {
    auto result = conn->exec("SELECT 1 AS value");
    while (result->next()) {
        const auto& block = result->row();
        auto row = result->getCurrentRow();
        CRAZY_SYSTEM_INFO()
            << crazy::ClickHouseResult::columnValueToString(block, 0, row);
    }
}
```

### 插入 Block

```cpp
clickhouse::Block block;
auto id = std::make_shared<clickhouse::ColumnUInt32>();
auto name = std::make_shared<clickhouse::ColumnString>();
id->Append(1);
name->Append("Alice");
block.AppendColumn("id", id);
block.AppendColumn("name", name);

conn->insert("test_table", block);
```

### 连接池

```cpp
crazy::ClickHouseConnectionPoolConfig cfg;
cfg.host = "127.0.0.1";
cfg.user = "default";
cfg.password = "";
cfg.database = "default";
cfg.min_connections = 1;
cfg.max_connections = 5;

auto pool = std::make_shared<crazy::ClickHouseConnectionPool>(cfg);
pool->start();
auto conn = pool->getConnection(3000);
conn->exec("SELECT 1");
pool->stop();
```

ClickHouse 封装还提供 `exec_fmt()`、`select()`、`ping()`、`reconnect()`、`resetConnection()`、`createDatabase()`、`dropDatabase()`、`getServerInfo()` 和异常类型。

## 内存映射

### Buffer

`Buffer` 是读写指针分离的动态字节缓冲区，支持自动扩容、前移复用、收缩和重置。

```cpp
crazy::Buffer buf;
buf.append("hello", 5);
buf.append('!');

auto readable = buf.readableCount();
std::string text(buf.readBegin(), readable);
buf.readed(readable);
buf.shrink();
```

### MmapInterface

```cpp
crazy::MmapInterface mmap("./data.bin", 1024 * 1024);
std::memcpy(mmap.data(), "hello", 5);
mmap.sync();
mmap.close();
```

### MmapVector

`MmapVector<T>` 只支持 trivially copyable 类型，数据持久化在 mmap 文件里。`operator[]` 写越界时会自动扩容并更新 size；`at()` 越界返回 `nullptr`。

```cpp
crazy::MmapVector<int32_t> vec("./vec.bin", 2);
vec[0] = 10;
vec[3] = 40;

if (auto p = vec.at(3)) {
    CRAZY_SYSTEM_INFO() << *p;
}
```

## 并发与锁

### ThreadPool

```cpp
crazy::ThreadPool pool(4);
pool.start();
pool.enqueueRunnable([] {
    CRAZY_SYSTEM_INFO() << "task";
});
pool.enqueueRunnable([] {}, 0);
pool.stop();
```

### AtomicLock 与 CondMutex

```cpp
crazy::AtomicLock spin;
{
    crazy::AtomicLockGuard guard(spin);
}

crazy::CondMutex mutex;
{
    crazy::CondMutexGuard guard(mutex);
}
mutex.signal();
mutex.signalAll();
```

### FileLock

```cpp
crazy::FileLock lock("./app.lock");
if (lock.tryLock()) {
    // 当前进程持有锁
    lock.unlock();
}
```

### MVCCLockWrapper

双版本读写包装：读操作无锁，写操作通过事务提交切换版本。

```cpp
crazy::MVCCLockWrapper<std::string> value("v1");

{
    auto tx = value.beginWrite();
    tx.set("v2");
    tx.commit();
}

int64_t version = 0;
auto current = value.readWithVersion(version);
bool changed = value.isUpdated(version);
```

## 基础工具

### DateTime 与 TimeZone

```cpp
auto now = crazy::DateTime::now();
auto dt = crazy::DateTime::fromString("2026-01-01 08:00:00");
dt.addDays(1).addHours(2);

CRAZY_SYSTEM_INFO() << dt.toString();
CRAZY_SYSTEM_INFO() << dt.year() << "-" << dt.month() << "-" << dt.day();
CRAZY_SYSTEM_INFO() << crazy::DateTime::isLeapYear(2024);

crazy::TimeZone utc = crazy::TimeZone::UTC();
crazy::TimeZone local = crazy::TimeZone::Local();
auto utcTime = local.toUTC(std::time(nullptr));
```

### URI

```cpp
crazy::Uri uri("https://user:pass@example.com:8443/api?q=1#top");
uri.addQueryParam("page", "2");

CRAZY_SYSTEM_INFO() << uri.getScheme();
CRAZY_SYSTEM_INFO() << uri.getHost();
CRAZY_SYSTEM_INFO() << uri.getUserName();
CRAZY_SYSTEM_INFO() << uri.getPassword();
CRAZY_SYSTEM_INFO() << uri.toString();

auto resource = uri.resource();
auto next = uri.resolve(crazy::Uri("../next"));
```

### UUID、时间戳、字符串、路径、线程

```cpp
auto id = crazy::CreateUUID();
auto ms = crazy::GetCurrentMS();

auto parts = crazy::StringUtil::Split("a,b,c", ",");
auto text = crazy::StringUtil::Trim("  hello ");
auto upper = crazy::StringUtil::ToUpper("abc");

auto exe = crazy::PathUtil::GetExecutablePath();
auto dir = crazy::PathUtil::GetExecutableDirectory();
crazy::PathUtil::CreateDirectories("./logs/app");

crazy::ThreadUtil::SetThreadName("worker");
auto name = crazy::ThreadUtil::GetThreadName();
```

也可以直接使用 `uuid.h` 中的 `uuids::uuid_system_generator`、`uuid_random_generator`、`uuid_name_generator` 和 `uuids::to_string()`。

### 端序转换

```cpp
uint32_t value = 0x12345678;
auto big = crazy::SwapToBigOrder(value);
auto little = crazy::SwapToLittleOrder(value);
```

### KeyValuePair

```cpp
std::vector<crazy::KeyValuePair> row = {
    {"name", std::string("tom")},
    {"age", 18},
};
std::string csv = crazy::datacsv(row);
```

### Singleton、Noncopyable、command_line

- `Singleton<T>::Instance()`：按类型提供单例实例。
- `Noncopyable`：禁止拷贝的基类。
- `cmdline::parser`：第三方风格命令行解析器，`Application` 内部已使用，也可以单独用于自定义命令。

## 测试程序

CMake 当前会生成以下测试/示例目标：

- `test_actor`
- `test_actor_client`
- `test_clickhouse`
- `test_config`
- `test_date_time`
- `test_encryption`
- `test_http`
- `test_http_session`
- `test_http_application`
- `test_http_client`
- `test_http_client_e2e`
- `test_json`
- `test_key_value_pair`
- `test_localsocket`
- `test_lock`
- `test_logger`
- `test_mmap`
- `test_mvcc_lock_wrapper`
- `test_mysql`
- `test_protocol`
- `test_smtp_client`
- `test_socket`
- `test_websocket`
- `test_websocket_client`
- `test_websocket_client_e2e`
- `test_websocket_server`

运行示例：

```bash
cmake --build build
./build/test_config
./build/test_json
```

部分数据库和网络测试依赖本机或配置中的外部服务。
