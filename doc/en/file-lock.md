# FileLock

[Back to documentation index](../README.en.md)

`crazy::FileLock` uses operating-system file locking for cross-process mutual exclusion.

## API

| API | Behavior |
|-----|----------|
| `setFilePath()` | Set the lock path |
| `lock()` | Immediate attempt, returns false if unavailable |
| `tryLock()` | Immediate attempt and replace the current handle on success |
| `unlock()` | Release |

Windows uses `LockFileEx` and Linux uses `flock()`. Both acquisition methods currently use non-blocking flags, so `lock()` does not wait for another process despite its name. The held handle, not file existence, represents the lock.

## Single instance

```cpp
#include "crazy/file_lock.h"

int main() {
    crazy::FileLock lock("./service.lock");
    if (!lock.tryLock()) {
        CRAZY_SYSTEM_ERROR() << "another instance is running";
        return 1;
    }

    runService();
}
```

## Protect a data file

```cpp
bool writeExclusive(const std::string& path, const std::string& data) {
    crazy::FileLock lock(path + ".lock");
    if (!lock.lock()) {
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return output.good();
}
```

The lock path must be writable and unique to the protected resource. Use `tryLock()` plus a bounded retry loop when a timeout is required. File locks do not coordinate different hosts.

```cpp
bool acquireWithTimeout(
    crazy::FileLock& lock,
    std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        if (lock.tryLock()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}
```

Add jitter for multi-process contention. Never infer lock state from file existence.

Related: [AtomicLock](atomic-lock.md), [CondMutex](cond-mutex.md), [Application](application.md).
