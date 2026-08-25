# crazy

A lightweight C++17 foundational framework library with Actor-based concurrency, networking, logging, configuration, serialization, database connection pools, memory mapping, locks, and common utility wrappers.

- **Language standard**: C++17
- **Build system**: CMake 3.15+
- **Current version**: 1.0.2
- **Documentation**: [English Manual](doc/README.en.md) | [中文](README.md)

## Quick Links

| Topic | Link |
|------|------|
| Feature overview | [doc/README.en.md#feature-overview](doc/README.en.md#feature-overview) |
| Build | [doc/README.en.md#build](doc/README.en.md#build) |
| Project integration | [doc/README.en.md#project-integration](doc/README.en.md#project-integration) |
| Application and Actor | [doc/README.en.md#application-and-actor](doc/README.en.md#application-and-actor) |
| Send CLI commands to Actors | [doc/README.en.md#send-cli-commands-to-actors](doc/README.en.md#send-cli-commands-to-actors) |
| Networking | [doc/README.en.md#networking](doc/README.en.md#networking) |
| Logging and configuration | [Logging](doc/README.en.md#logging) / [Configuration](doc/README.en.md#configuration) |
| Encryption | [doc/README.en.md#encryption](doc/README.en.md#encryption) |
| JSON, binary protocol, reflection | [JSON](doc/README.en.md#json-serialization) / [Binary Protocol and Reflection](doc/README.en.md#binary-protocol-and-reflection) |
| MySQL and ClickHouse | [MySQL](doc/README.en.md#mysql) / [ClickHouse](doc/README.en.md#clickhouse) |
| mmap, Buffer, locks, utilities | [Memory Mapping](doc/README.en.md#memory-mapping) / [Utilities](doc/README.en.md#utilities) |
| Test programs | [doc/README.en.md#test-programs](doc/README.en.md#test-programs) |

## Minimal Example

```cpp
#include "crazy.h"

class Worker : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

protected:
    void handleCommandLineMessgaBase(
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

After starting the service, send a command from another terminal:

```bash
./your_app -s worker@ping
```

See the [English Manual](doc/README.en.md) for complete module usage.
