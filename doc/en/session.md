# Session

[Back to documentation index](../README.en.md)

`crazy::Session` represents an established network connection. It combines a socket, receive/send buffers, Encoder, Decoder, heartbeat timestamp, and callbacks.

## Responsibilities

- Read socket bytes into a receive buffer.
- Feed the decoder and emit complete messages.
- Buffer outbound messages until they can be written.
- Invoke message and disconnect callbacks.
- Track the last heartbeat timestamp.

Session does not create a thread or reconnect. A surrounding `Selector` must register read events and dynamic write events.

## API

| API | Purpose |
|-----|---------|
| `getSessionId()` | Unique session identity |
| `socket()` | Underlying descriptor |
| `close()` | Close the connection |
| `onReadEvent()` / `onWriteEvent()` | Selector entry points |
| `sendMessage()` | Encode and queue a message |
| `lastHeartbeatTimestamp()` | Last inbound activity |
| `remoteAddress()` | Peer address |

## Complete integration

```cpp
#include "crazy/net/session.h"

class SessionServer {
public:
    void accept(crazy::Socket::ptr socket) {
        auto session = std::make_shared<crazy::Session>(socket);

        session->registerSelectEventRegisterCallback(
            [this](int32_t fd, crazy::SelectorEventType type,
                   std::function<void()> callback) {
                selector_.registerEvent(fd, type, std::move(callback));
            });

        session->registerSelectEventUnregisterCallback(
            [this](int32_t fd, crazy::SelectorEventType type) {
                selector_.unregisterEvent(fd, type);
            });

        session->registerMessageCallback(
            [session](crazy::MessageBase::ptr message) {
                auto response = message->createResponse();
                response->setData("echo: " + message->getData());
                session->sendMessage(response);
            });

        session->registerDisconnectCallback([this, session] {
            sessions_.erase(session->getSessionId());
        });

        sessions_[session->getSessionId()] = session;
        selector_.registerEvent(
            socket->socket(), crazy::SelectorEventType::read,
            [session] { session->onReadEvent(); });
    }

private:
    crazy::Selector selector_;
    std::map<uint64_t, crazy::Session::ptr> sessions_;
};
```

## Callback ordering

The message callback runs for one complete decoder frame. The disconnect callback should remove business state and container references. The write-event callbacks are required whenever the output buffer cannot be fully flushed immediately.

## Backpressure and heartbeat

A slow peer grows the send buffer. Add a business-level queue limit or disconnect policy. `lastHeartbeatTimestamp()` updates on inbound activity and is used by higher-level service actors to close idle sessions. Never operate one Session from multiple threads.

Related: [Encoder](encoder.md), [Decoder](decoder.md), [ServiceActor](service-actor.md), [ClientActor](client-actor.md), [Buffer](buffer.md).
