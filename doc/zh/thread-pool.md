# ThreadPool

[返回文档目录](../README.md)

`crazy::ThreadPool` 把后台任务分配到固定数量的 Actor 工作线程。

## 创建和使用

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

`enqueueRunnable(fn)` 按轮询方式选择线程，`enqueueRunnable(fn, index)` 把任务固定投递到指定线程。销毁线程池前应先调用 `stop()`，任务中要自行处理异常，避免异常穿过工作线程循环。

## 调度语义

ThreadPool 内部是一组 `ActorInterface` 工作线程。无线程索引的任务按轮询选择目标，指定索引的任务始终进入对应线程：

```cpp
for (int i = 0; i < 10; ++i) {
    pool.enqueueRunnable([i] {
        CRAZY_SYSTEM_INFO() << "task " << i;
    });
}

// 需要保持某种资源顺序的任务固定投递到同一线程。
pool.enqueueRunnable(updateCache, 0);
pool.enqueueRunnable(flushCache, 0);
```

指定同一线程只保证任务进入同一队列，不保证在调用 `enqueueRunnable()` 的线程中立即执行。任务的内存可见性由队列内部同步保证。

## 完整示例

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
    return 0;
}
```

## 与 Application 的关系

`Application` 在启动时创建 ThreadPool，并按 `[global] thread_pool_count` 设置线程数。`Application::enqueueRunnable()` 实际转发到该线程池。`stopService()` 会先调用 `threadPool_->stop()`，再停止 Actor 和数据库连接池。

## 使用边界

- 不要从多个任务同时修改未加锁的共享状态。
- 不要在任务中长时间等待另一个相同线程池任务结束，可能形成饥饿。
- 不要依赖匿名任务的绝对执行顺序。
- 需要顺序的业务应固定到同一线程，或直接使用 Actor 消息队列。
- 停止期间提交的任务可能被拒绝或无法执行，调用方应处理这种生命周期边界。

相关文档：[Actor](actor.md)、[Application](application.md)、[AtomicLock](atomic-lock.md)、[CondMutex](cond-mutex.md)。
