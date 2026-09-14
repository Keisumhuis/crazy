# CondMutex

[Back to documentation index](../README.en.md)

`crazy::CondMutex` combines a mutex and condition variable, with wait, signal, and broadcast operations.

## API

| API | Purpose |
|-----|---------|
| `lock()` / `unlock()` / `tryLock()` | Mutex operations |
| `wait()` | Release the mutex and wait |
| `signal()` | Wake one waiter |
| `signalAll()` | Wake all waiters |
| `CondMutexGuard` | RAII unlock |

## Producer and consumer

```cpp
#include "crazy/cond_mutex.h"

class Mailbox {
public:
    void put(std::string value) {
        {
            crazy::CondMutexGuard guard(mutex_);
            value_ = std::move(value);
            hasValue_ = true;
        }
        mutex_.signal();
    }

    std::string take() {
        crazy::CondMutexGuard guard(mutex_);
        while (!hasValue_) {
            mutex_.wait();
        }

        hasValue_ = false;
        return std::move(value_);
    }

private:
    crazy::CondMutex mutex_;
    std::string value_;
    bool hasValue_ = false;
};
```

## Correct wait predicate

```cpp
while (!ready_) {
    mutex_.wait();
}
```

Always use a loop, not `if`, because of spurious wakeups and races before reacquiring the lock. Modify shared state while holding the mutex. Use `signalAll()` for shutdown or state changes that can release every waiter.

`wait()` expects the caller to already own the mutex. It adopts the current lock, waits, returns with the mutex locked, and leaves the outer guard responsible for unlocking.

```cpp
crazy::CondMutexGuard guard(mutex_);
while (!ready_) {
    mutex_.wait();
}
```

Waiting without owning the mutex is undefined. Shutdown predicates should combine data and stopped flags and wake all waiters with `signalAll()`.

Related: [AtomicLock](atomic-lock.md), [ThreadPool](thread-pool.md), [MVCCLockWrapper](mvcc.md).
