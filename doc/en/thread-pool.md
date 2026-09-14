# ThreadPool

[Back to documentation index](../README.en.md)

`crazy::ThreadPool` distributes background work across a fixed set of Actor worker threads.

## Create and use

```cpp
#include "crazy/thread_pool.h"

crazy::ThreadPool pool(4);
pool.start();

pool.enqueueRunnable([] {
    CRAZY_SYSTEM_INFO() << "any worker";
});

pool.enqueueRunnable([] {
    CRAZY_SYSTEM_INFO() << "fixed worker";
}, 2);

pool.stop();
```

`enqueueRunnable(fn)` selects a worker in round-robin order. `enqueueRunnable(fn, index)` targets a worker by index. Call `stop()` before destruction and handle exceptions inside tasks.

## Scheduling semantics

```cpp
crazy::ThreadPool pool(4);
pool.start();

for (int i = 0; i < 10; ++i) {
    pool.enqueueRunnable([i] {
        CRAZY_SYSTEM_INFO() << "task " << i;
    });
}

pool.enqueueRunnable(updateCache, 0);
pool.enqueueRunnable(flushCache, 0);
```

Targeting the same worker provides ordering through one queue, but tasks still run asynchronously to the submitting thread.

## Complete example

```cpp
#include "crazy/thread_pool.h"

#include <atomic>
#include <chrono>

int main() {
    constexpr int taskCount = 100;
    std::atomic<int> completed{0};

    crazy::ThreadPool pool(4);
    pool.start();

    for (int i = 0; i < taskCount; ++i) {
        pool.enqueueRunnable([i, &completed] {
            try {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(10));
                completed.fetch_add(1, std::memory_order_relaxed);
            } catch (const std::exception& error) {
                CRAZY_SYSTEM_ERROR() << "task " << i
                                     << " failed: " << error.what();
            }
        });
    }

    while (completed.load(std::memory_order_relaxed) < taskCount) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    pool.stop();
}
```

## Boundaries

- Do not mutate shared data without synchronization.
- Avoid waiting for another task in the same bounded pool.
- Do not depend on the order of anonymous tasks.
- Use an Actor or a targeted worker when ordering is required.
- Handle submissions during shutdown, when work may no longer run.

Related: [Actor](actor.md), [Application](application.md), [AtomicLock](atomic-lock.md), [CondMutex](cond-mutex.md).
