# TelnetServiceActor

[Back to documentation index](../README.en.md)

`TelnetServiceActor` provides a password-protected Telnet command endpoint and forwards text commands to registered Actors.

## Configuration

```ini
[telnet]
address = "127.0.0.1"
port = 2323
max_sessions = 16
idle_timeout = 300000
command_timeout = 10000
password = "change-me"
```

## Start the service

```cpp
#include "crazy.h"

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<crazy::TelnetServiceActor>("telnet");
    app.registerActor<EchoActor>("echo");
    app.exec();
}
```

Connect and run a command:

```bash
telnet 127.0.0.1 2323
echo@ping
```

Timeout values are in milliseconds. Use a strong password and restrict network access because plain Telnet does not encrypt credentials or traffic.

## Configuration fields

| Key | Default | Meaning |
|-----|---------|---------|
| `address` | `0.0.0.0` | Listen address |
| `port` | `2323` | Listen port |
| `max_sessions` | `16` | Concurrent connection limit |
| `idle_timeout` | `300000` | Idle disconnect time in milliseconds |
| `command_timeout` | `10000` | Actor response timeout in milliseconds |
| `password` | empty | Login password |

## Command flow

After login, enter `counter@inc`. The service parses the Actor name and command, dispatches through Application's local command path, and waits for a response. Built-in help and Actor list commands are available inside the session.

## Custom command Actor

```cpp
class CounterActor final : public crazy::ActorInterface {
public:
    using crazy::ActorInterface::ActorInterface;

    std::map<std::string, std::string> helps() override {
        return {{"inc", "increment"}, {"get", "read count"}};
    }

protected:
    void handleCommandLineMessageBase(
        crazy::MessageBase::ptr request,
        crazy::MessageBase::ptr response) override {
        if (request->getData() == "inc") {
            ++count_;
        }
        response->setData("count=" + std::to_string(count_));
    }

private:
    uint64_t count_ = 0;
};

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<crazy::TelnetServiceActor>("telnet");
    app.registerActor<CounterActor>("counter");
    app.exec();
}
```

Add login rate limiting, network ACLs, and audit logging at the operational layer. The module itself does not provide those controls.

Related: [Actor](actor.md), [Application](application.md), [Configuration](config.md).
