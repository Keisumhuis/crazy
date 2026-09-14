# Daemon

[Back to documentation index](../README.en.md)

`crazy::Daemon` implements a supervisor/worker model. `Application::exec()` parses `-d` automatically, so most programs do not need to assemble daemon arguments manually.

## Run as a daemon

```cpp
#include "crazy.h"

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<crazy::ServiceActor>("service");
    app.exec();
}
```

```bash
./your_app -d
```

The supervisor starts and monitors the worker. If the worker exits unexpectedly, it is started again. `restartService()` exits with code 1 so the supervisor can replace the worker.

## Parse arguments manually

```cpp
auto args = crazy::Daemon::ParseArguments(argc, argv);

if (args.enabled && args.role == crazy::DaemonRole::supervisor) {
    return crazy::Daemon::RunSupervisor(args.childArgs);
}

// Continue as a worker or a normal foreground process.
```

| Role | Meaning |
|------|---------|
| `none` | Normal foreground process |
| `supervisor` | Monitors and restarts the worker |
| `worker` | Runs the application logic |

Daemon mode uses file locks to prevent duplicate supervisors or application instances.

## Startup flow

1. The initial process sees `-d` and starts a supervisor.
2. It attempts `<program>.daemon.lock` and `<program>.lock`.
3. `Daemon::StartSupervisor()` creates the supervisor.
4. The supervisor starts and monitors a worker process.
5. A normal worker exit stops supervision; an abnormal exit is restarted.
6. `restartService()` exits with code 1 to request replacement.

## Stop and restart

```bash
./your_app -s application@shutdown
./your_app -s application@restart
```

The lock files are held by the operating system and released when the process exits. Do not delete them while the service is running. A stale file does not prevent a new process from acquiring a released lock.

## Manual parsing

```cpp
int main(int argc, char** argv) {
    auto args = crazy::Daemon::ParseArguments(argc, argv);

    if (args.enabled &&
        args.role == crazy::DaemonRole::supervisor) {
        return crazy::Daemon::RunSupervisor(args.childArgs);
    }

    if (args.enabled &&
        args.role == crazy::DaemonRole::worker) {
        CRAZY_SYSTEM_INFO() << "running worker";
    }

    return 0;
}
```

`childArgs` has daemon-internal arguments removed. `applicationArgs` retains arguments for the application parser.

## Deployment guidance

- Do not use this supervisor together with systemd, Windows Service recovery, or a container restart policy unless the layering is intentional.
- Persist logs outside ephemeral directories.
- Distinguish normal and abnormal worker exit codes.
- Give every parallel instance a distinct executable name, lock file, command socket, and log directory.
- Wait for config, data directories, and ports before declaring startup successful.

Related: [Application](application.md), [Configuration](config.md), [Logging](logger.md).
