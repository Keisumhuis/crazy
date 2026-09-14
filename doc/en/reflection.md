# REFLECTION

[Back to documentation index](../README.en.md)

`REFLECTION(...)` generates field traversal code used by JSON and binary serialization. It supports up to 60 fields.

## Declare reflected fields

```cpp
#include "crazy/reflection.h"

struct User {
    uint64_t id = 0;
    std::string name;
    std::vector<std::string> roles;

    REFLECTION(id, name, roles);
};
```

The macro generates `to_protocol`, `from_protocol`, `to_json`, and `from_json`, so the same type can be used with both formats:

```cpp
User user{7, "alice", {"admin"}};

const std::string json = crazy::json::Converter::Serializable(user);
const std::string binary = crazy::protocol::Converter::Serializable(user);
```

Place the macro in a public section of the type. JSON keys come from field names, so renaming a reflected field can break compatibility with stored data.

## Generated interfaces

| Interface | Consumer | Purpose |
|-----------|----------|---------|
| `to_json` | `json::Converter` | Write fields into an object |
| `from_json` | `json::Converter` | Read fields from an object |
| `to_protocol` | `protocol::Converter` | Write fields in declaration order |
| `from_protocol` | `protocol::Converter` | Read fields in declaration order |

Field order is critical for binary compatibility but not for JSON field lookup.

## Nested reflected types

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
```

Every nested user-defined type must also be reflected.

## Schema evolution

Adding an optional JSON field can preserve compatibility with old documents. Binary protocol has no field names or version negotiation; adding, deleting, or reordering fields changes the wire format. Include an explicit schema version in persisted binary data.

## Limits

- Up to 60 fields.
- Fields must be publicly accessible to the generated code.
- Pointers and complex ownership are not directly serializable.
- Polymorphic base types do not automatically record a derived type.
- Reflected names and order should be treated as part of the data contract.

Related: [JSON](json.md), [Binary protocol](protocol.md), [MessageBase](message.md).
