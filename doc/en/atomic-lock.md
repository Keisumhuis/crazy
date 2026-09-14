# AtomicLock

[Back to documentation index](../README.en.md)

`crazy::AtomicLock` is a spin lock built on `std::atomic_flag`. It is intended only for very short, non-blocking critical sections.

## API

| API | Purpose |
|-----|---------|
| `lock()` | Spin until acquired |
| `try_lock()` | Attempt once |
| `unlock()` | Release |
| `AtomicLockGuard` | RAII acquire/release |

## Basic use

```cpp
#include "crazy/atomic_lock.h"

crazy::AtomicLock lock;

void updateFlag(bool value) {
    crazy::AtomicLockGuard guard(lock);
    flag_ = value;
}
```

## Queue example

```cpp
class TaskQueue {
public:
    void push(Task task) {
        crazy::AtomicLockGuard guard(lock_);
        queue_.push(std::move(task));
    }

    bool pop(Task& task) {
        crazy::AtomicLockGuard guard(lock_);
        if (queue_.empty()) {
            return false;
        }
        task = std::move(queue_.front());
        queue_.pop();
        return true;
    }

private:
    crazy::AtomicLock lock_;
    std::queue<Task> queue_;
};
```

## Boundaries

Spinning wastes CPU when contention lasts. Never perform socket I/O, file I/O, database work, sleeping, or condition waits while holding the lock. Use `std::mutex` for general short sections and `CondMutex` when waiting for state.

The current implementation uses acquire/release ordering, yields for early contention, and sleeps about 50 microseconds after repeated failures. It is still not a replacement for a general mutex.

| Tool | Contention behavior | Intended use |
|------|---------------------|--------------|
| `AtomicLock` | Spin, yield, short sleep | Tiny non-blocking sections |
| `std::mutex` | OS wait queue | General shared state |
| `CondMutex` | Wait plus notification | Event and queue state |

Related: [CondMutex](cond-mutex.md), [FileLock](file-lock.md), [ThreadPool](thread-pool.md).
