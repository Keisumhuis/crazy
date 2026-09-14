# JSON Serialization

[Back to documentation index](../README.en.md)

`crazy::json` supports primitives, standard containers, `std::optional`, and reflected custom types.

## Write JSON

```cpp
#include "crazy/json.h"

crazy::json::Serialise writer;
writer.add_from("name", std::string("alice"));
writer.add_from("age", 25);
writer.add_from("tags", std::vector<std::string>{"c++", "server"});

const std::string text = writer.getString();
```

## Custom type

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

Supported containers include C arrays, `std::array`, `vector`, `deque`, `list`, `set`, `unordered_set`, `map`, `unordered_map`, and `stack`.

## Type mapping

| C++ value | JSON representation |
|-----------|---------------------|
| Numeric and Boolean | Number or Boolean |
| `std::string` | String |
| `std::optional<T>` | Value or `null` |
| Sequence containers | Array |
| Set containers | Array |
| Map containers | Object |
| Reflected type | Object of field names and values |

String map keys become JSON keys; integer keys are converted to strings.

## Nested business object

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

## Manual writer

```cpp
crazy::json::Serialise writer;
writer.add_from("items", std::vector<int>{1, 2, 3});
writer.add_from("enabled", true);
writer.add_from("timeout", 12.5);
const std::string payload = writer.getString();
```

## Compatibility and safety

- Validate type mismatches and required fields before use.
- `std::optional` helps preserve compatibility with older documents.
- Renaming reflected fields breaks JSON keys.
- Large arrays are loaded into memory as a whole.
- Use strings or integer minor units for exact monetary values.

Related: [REFLECTION](reflection.md), [Binary protocol](protocol.md), [HTTP server](http-server.md).
