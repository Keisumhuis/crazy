# crazy English Manual

`crazy` is a C++17 static library for common server-side infrastructure: Actor-based concurrency, TCP/local sockets, logging, INI configuration, JSON and binary protocols, MySQL/ClickHouse connection pools, mmap, locks, time, URI, UUID, and path utilities.

> 中文: [README.md](README.md)

## Feature Overview

| Module | Capabilities | Main headers |
|------|--------------|--------------|
| Application | App entry, Actor registration, CLI control, routing, background tasks, database pool access | `application.h` |
| Actor | Dedicated thread, message queue, async task queue, CLI message handling, normal message handling | `actor_interface.h`, `message_base.h` |
| Daemon | `-d` daemon argument parsing, supervisor/worker roles, worker restart loop | `daemon.h` |
| Networking | IPv4 Socket, local Socket, Acceptor/Connection, Session, Encoder/Decoder, Selector, timers, ServiceActor/ClientActor | `net/*.h` |
| Logging | trace/debug/info/warn/error/fatal, multiple loggers, console/file appenders, custom format | `logger.h` |
| Configuration | INI file/directory loading, string/integer/double/bool reads, defaults, section checks | `config.h` |
| Encryption | Base64 encode/decode, MD5 hashing | `encryption/base64.h`, `encryption/md5.h` |
| JSON | Primitive types, STL containers, optional, reflected custom types | `json.h`, `reflection.h` |
| Binary protocol | Primitive types, enums, arrays, STL containers, reflected custom types | `protocol.h`, `reflection.h` |
| Databases | MySQL/ClickHouse connections, queries, formatted SQL, transactions, pools, health checks | `mysql/*.h`, `clickhouse/*.h` |
| Memory and files | Buffer, cross-platform mmap, persistent `MmapVector`, file lock | `buffer.h`, `mmap/*.h`, `file_lock.h` |
| Concurrency | Thread pool, atomic lock, condition mutex, MVCC double-version wrapper | `thread_pool.h`, `atomic_lock.h`, `cond_mutex.h`, `mvcc_lock_wrapper.h` |
| Utilities | DateTime, TimeZone, URI, UUID, endian conversion, string/path/thread-name utilities, singleton, noncopyable base, command-line parser | `date_time.h`, `time_zone.h`, `uri.h`, `uuid.h`, `endian.h`, `utils.h`, `singleton.h`, `command_line.h` |
| Third party | RapidJSON, GSL, MySQL client, ClickHouse client headers/prebuilt libs | `src/crazy/rapidjson`, `src/crazy/gsl`, `src/third_party` |

## Build

Windows:

```bat
build_crazy_framework.bat
```

Linux:

```bash
chmod +x build_crazy_framework.sh
./build_crazy_framework.sh
```

Manual build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

CMake builds the static library target `crazy` and the example programs under `tests/`. The Windows script builds x86/x64 Debug/Release. The Linux script builds x86/x64 Debug/Release.

## Project Integration

Include the aggregate header:

```cpp
#include "crazy.h"
```

Link from CMake:

```cmake
add_subdirectory(path/to/crazy)
target_link_libraries(your_target PRIVATE crazy)
target_include_directories(your_target PRIVATE path/to/crazy/src)
```

For database modules, make sure the matching platform/architecture libraries exist under `src/third_party/MySQL` and `src/third_party/clickhouse`. CMake selects the files automatically.

## Application and Actor

`Application` is the runtime entry. It derives from `ActorInterface` and manages Actor registration, thread startup, CLI service, message routing, thread-pool tasks, and database pools.

```cpp
class Worker : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

    std::map<std::string, std::string> helps() override {
        return {{"ping", "return pong"}};
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

Common APIs:

- `registerActor<T>("name")`: register an Actor.
- `addRouteTable(from, to, cmd)`: route messages from `from` with command `cmd` to `to`.
- `enqueueRunnable(fn)`: submit a background task to the internal thread pool.
- `getMySQLConnectionPool()` / `getClickHouseConnectionPool()`: access database pools.
- `stopService()`: stop the service.

## Send CLI Commands to Actors

After the service process is running, use the same executable to send a command to a specific Actor:

```bash
./your_app -s worker@ping
```

Format:

```text
actor_name@command
```

The Actor should override `helps()` and `handleCommandLineMessageBase()`. Built-in commands are listed with:

```bash
./your_app --help
```

## MessageBase

`MessageBase` is the shared message object used by Actors, network sessions, and encoders/decoders.

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

Use `InternalMessage<T>` for strongly typed internal payloads:

```cpp
auto msg = std::make_shared<crazy::InternalMessage<int>>();
msg->setCmd(200);
msg->setValue(42);
```

## Daemon

`Daemon` provides daemon argument parsing and a supervisor/worker runtime shape.

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

Roles:

- `DaemonRole::none`: normal process.
- `DaemonRole::supervisor`: monitor process that starts/checks workers.
- `DaemonRole::worker`: business process.

## Networking

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

`SocketInterface` also provides `active()`, `close()`, `getOption()`, `setOption()`, and `socket()`.

### LocalSocket

Local sockets are used for same-machine IPC. Application CLI control uses this layer too.

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

### Selector, Timers, and Time-Point Tasks

`Selector` wraps epoll/wepoll and supports read/write events, wakeup, repeating timers, and daily fixed-time tasks.

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

### ServiceActor and ClientActor

`ServiceActor` manages server listening, sessions, heartbeat, and message receive/send. `ClientActor` manages client connection, disconnect, and heartbeat. Configuration can be loaded from INI:

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

Register them like normal Actors:

```cpp
crazy::Application app(argc, argv);
app.registerActor<crazy::ServiceActor>("service");
app.registerActor<crazy::ClientActor>("client");
app.exec();
```

### Encoder, Decoder, and Session

`Encoder` writes a `MessageBase` to `Buffer`; `Decoder` parses complete messages from `Buffer` and supports max message size and success/error callbacks. `Session` combines a socket, receive/send buffers, encoder, decoder, disconnect callback, and message callback.

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

## Logging

Six levels are supported: `trace`, `debug`, `info`, `warn`, `error`, and `fatal`.

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

Custom logger:

```cpp
auto logger = LOGGER(api);
logger->setLevel(crazy::LoggerLevel::debug);
logger->setFormatter("%d %c %p %f:%l %m");
logger->addAppender(std::make_shared<crazy::LoggerConsoleAppenderImpl>());
logger->addAppender(std::make_shared<crazy::LoggerFileAppenderImpl>("./logs"));
```

Formatter components include date, category, priority, function, line, and message.

## Configuration

Load one INI file or all files with a suffix under a directory.

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

Keys outside an explicit section are stored under `global`.

## Encryption

`Base64` provides string encoding/decoding, and `MD5` provides string hashing.

```cpp
auto encoded = crazy::Base64::encryption("hello world");
auto decoded = crazy::Base64::decryption(encoded);

auto digest = crazy::MD5::encryption("hello");
CRAZY_SYSTEM_INFO() << encoded << ", " << decoded << ", " << digest;
```

## JSON Serialization

`json::Serialise` / `json::Deserialise` support primitive types, strings, arrays, `std::array`, `vector`, `deque`, `list`, `set`, `unordered_set`, `map`, `unordered_map`, `stack`, `optional`, and reflected custom types.

```cpp
crazy::json::Serialise ser;
ser.add_from("name", std::string("kesium"));
ser.add_from("age", 25);
ser.add_from("tags", std::vector<std::string>{"c++", "server"});
std::string text = ser.getString();
```

Custom structs with `REFLECTION`:

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

## Binary Protocol and Reflection

`protocol::Serialize` / `protocol::Deserialize` write binary data with type tags. Supported types include primitives, enums, C arrays, `std::array`, `deque`, `list`, `map`, `set`, `stack`, `unordered_map`, `unordered_set`, `vector`, and reflected custom types.

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

Manual serialization is also supported:

```cpp
crazy::protocol::Serialize ser;
ser << int32_t(1) << std::string("hello");

crazy::protocol::Deserialize de;
de.setBuffer(ser.getBuffer());
int32_t id;
std::string text;
de >> id >> text;
```

`REFLECTION(...)` supports up to 60 fields and generates `to_protocol`, `from_protocol`, `to_json`, and `from_json`.

## MySQL

### Direct Connection

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

### Formatted SQL, Transactions, and Metadata

```cpp
auto result = conn->exec_fmt("SELECT * FROM users WHERE id = %d", 10);

conn->beginTransaction();
conn->exec("UPDATE users SET score = score + 1 WHERE id = 10");
conn->commit();

CRAZY_SYSTEM_INFO() << conn->server_info();
CRAZY_SYSTEM_INFO() << conn->affected_rows();
```

### Prepared Statements

```cpp
auto stmt = conn->create_statement();
stmt->prepare("SELECT name FROM users WHERE id = ?");
// Bind MYSQL_BIND arrays for input/output:
// stmt->bind_input_param(inputBinds);
// stmt->bind_output_param(outputBinds);
stmt->execute();
stmt->store_result();
```

### Connection Pool

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

The pool handles idle connections, timeout waits, validity checks, automatic return, and shrinking.

## ClickHouse

### Direct Connection and Query

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

### Insert Block

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

### Connection Pool

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

The ClickHouse wrapper also provides `exec_fmt()`, `select()`, `ping()`, `reconnect()`, `resetConnection()`, `createDatabase()`, `dropDatabase()`, `getServerInfo()`, and exception types.

## Memory Mapping

### Buffer

`Buffer` is a dynamic byte buffer with separated read/write cursors, automatic expansion, data compaction, shrinking, and reset.

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

`MmapVector<T>` only supports trivially copyable types. Data is persisted in the mmap file. `operator[]` auto-grows on out-of-range writes and updates size; `at()` returns `nullptr` when out of range.

```cpp
crazy::MmapVector<int32_t> vec("./vec.bin", 2);
vec[0] = 10;
vec[3] = 40;

if (auto p = vec.at(3)) {
    CRAZY_SYSTEM_INFO() << *p;
}
```

## Concurrency and Locks

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

### AtomicLock and CondMutex

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
    // Current process owns the lock.
    lock.unlock();
}
```

### MVCCLockWrapper

Double-version read/write wrapper: reads are lock-free; writes commit by switching versions.

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

## Utilities

### DateTime and TimeZone

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

### UUID, Timestamps, String, Path, Thread

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

You can also use `uuids::uuid_system_generator`, `uuid_random_generator`, `uuid_name_generator`, and `uuids::to_string()` from `uuid.h`.

### Endian Conversion

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

### Singleton, Noncopyable, command_line

- `Singleton<T>::Instance()`: typed singleton access.
- `Noncopyable`: base class that disables copying.
- `cmdline::parser`: command-line parser used internally by `Application` and available for custom commands.

## Test Programs

CMake currently builds these test/example targets:

- `test_actor`
- `test_actor_client`
- `test_clickhouse`
- `test_config`
- `test_date_time`
- `test_encryption`
- `test_json`
- `test_key_value_pair`
- `test_localsocket`
- `test_lock`
- `test_logger`
- `test_mmap`
- `test_mvcc_lock_wrapper`
- `test_protocol`

Run examples:

```bash
cmake --build build
./build/test_config
./build/test_json
```

Some database and networking tests require local or configured external services.
