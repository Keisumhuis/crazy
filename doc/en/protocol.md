# Binary Protocol

[Back to documentation index](../README.en.md)

`crazy::protocol` provides tagged binary serialization for internal service communication and persistence.

## Manual read and write

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

## Custom type

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

Supported values include primitives, enums, C arrays, STL containers, and reflected types. Always validate externally supplied lengths and business fields before allocating memory.

## Processing model

`Serialize` writes values in stream order with type tags. `Deserialize` reads them in the exact same order. The writer owns a `Buffer`; pass its copy into the reader.

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

A read failure should invalidate the entire object. Do not use partially decoded data.

## Nested custom type

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
```

## Message payload

```cpp
auto message = std::make_shared<crazy::MessageBase>();
message->setCmd(2001);
message->setData(
    crazy::protocol::Converter::Serializable(snapshot));
```

The receiver selects one schema for command 2001. Avoid reusing a command for multiple incompatible schemas.

## Binary compatibility

- Add an explicit version or magic header to persisted protocols.
- Do not treat compiler struct layout as a stable wire format.
- Bound collection and string sizes before allocation.
- Keep producer and consumer schema versions aligned.
- Prefer append-only fields or a versioned message envelope for evolution.

Related: [REFLECTION](reflection.md), [MessageBase](message.md), [Encoder](encoder.md), [Decoder](decoder.md), [Session](session.md).
