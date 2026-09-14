# MessageBase

[Back to documentation index](../README.en.md)

`crazy::MessageBase` is the shared message object used by Actors, Sessions, Encoder, and Decoder.

## Standard message

```cpp
#include "crazy.h"

auto request = std::make_shared<crazy::MessageBase>();
request->setSource("gateway");
request->setSessionId(42);
request->setCmd(1001);
request->setComment("query user");
request->setData("user_id=7");

auto response = request->createResponse();
response->setData("user_name=alice");
```

`createResponse()` preserves routing information and associates the response with the request.

## Typed internal message

```cpp
struct Task {
    uint64_t id = 0;
    std::string name;
};

auto message = std::make_shared<crazy::InternalMessage<Task>>();
message->setCmd(2001);
message->setValue(Task{7, "build"});

const Task& task = message->getValue();
```

`InternalMessage<T>` is intended for in-process calls. It does not serialize `T` for the network. Put encoded data in `data`, or use the JSON/protocol modules for cross-process messages.

## Fields

| Field | Type | Purpose |
|-------|------|---------|
| `source` | `std::string` | Sender or route origin |
| `sessionId` | `uint64_t` | Network session identity, 0 for non-network messages |
| `cmd` | `uint64_t` | Route and business command |
| `comment` | `std::string` | Diagnostic or business note |
| `data` | `std::string` | Text or encoded binary payload |

`createResponse()` copies routing information and creates a response suitable for `sendMessage()`.

## Complete request and response

```cpp
class UserActor final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

protected:
    void handleMessageBase(crazy::MessageBase::ptr request) override {
        if (request->getCmd() != 1001) {
            return;
        }

        const uint64_t userId = std::stoull(request->getData());
        auto response = request->createResponse();

        response->setComment("find user");
        response->setData(
            userId == 7
                ? R"({"id":7,"name":"alice"})"
                : R"({"error":"not found"})");
        sendMessage(response);
    }
};
```

## Binary payload

```cpp
struct LoginRequest {
    uint64_t userId = 0;
    std::string token;
    REFLECTION(userId, token);
};

LoginRequest value{7, "secret"};

auto message = std::make_shared<crazy::MessageBase>();
message->setCmd(2001);
message->setData(
    crazy::protocol::Converter::Serializable(value));
```

The receiver selects the decoder by `cmd`. Never assume that `data` is UTF-8 text.

## Typed internal messages

`InternalMessage<T>` is best for in-process callbacks where both sides agree on the C++ type. A message passed as `MessageBase::ptr` loses the compile-time type, so confirm the actual type or enforce a strict command-to-type convention. Avoid passing strongly typed internal messages across dynamic library boundaries.

Related: [Actor](actor.md), [Binary protocol](protocol.md), [JSON](json.md), [Encoder](encoder.md), [Decoder](decoder.md), [Session](session.md).
