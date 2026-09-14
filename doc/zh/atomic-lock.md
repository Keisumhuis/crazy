# AtomicLock

[返回文档目录](../README.md)

`crazy::AtomicLock` 是基于 `std::atomic_flag` 的自旋锁，适合保护极短、无阻塞的临界区。

## API

| 接口 | 说明 |
|------|------|
| `lock()` | 自旋直到取得锁 |
| `try_lock()` | 立即尝试获取锁 |
| `unlock()` | 释放锁 |
| `AtomicLockGuard` | RAII 自动加锁和解锁 |

## 基本使用

```cpp
#include "crazy/atomic_lock.h"

crazy::AtomicLock lock;

void updateFlag(bool value) {
    crazy::AtomicLockGuard guard(lock);
    flag_ = value;
}
```

## 队列保护示例

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

## 适用边界

自旋锁不主动让出 CPU。持锁时间稍长、临界区可能发生 IO、线程数大于 CPU 核数时，自旋会浪费调度时间甚至放大争用。

当前实现使用 acquire/release 内存序。前几次失败会调用 `std::this_thread::yield()`，持续争用时每轮休眠约 50 微秒，然后继续尝试。这样可以减轻纯忙等，但仍会长于普通短互斥区间的成本。

## 与标准锁对比

| 工具 | 获取失败行为 | 适用场景 |
|------|--------------|----------|
| `AtomicLock` | 自旋、yield 或短暂 sleep | 极短、不可能阻塞的临界区 |
| `std::mutex` | 操作系统等待队列 | 一般共享状态 |
| `CondMutex` | 支持条件等待 | 等待事件或队列状态变化 |

不要因为 AtomicLock 没有显式构造开销就把它用作所有场景的默认锁。真正的选择标准是临界区持续时间和争用程度。

以下操作不应放在 AtomicLock 内：

- Socket 或文件读写。
- 数据库查询。
- `sleep()` 或等待条件变量。
- 调用可能再次获取同一锁的代码。

需要等待状态变化时使用 `CondMutex`，普通短临界区可以直接使用 `std::mutex`。

相关文档：[CondMutex](cond-mutex.md)、[FileLock](file-lock.md)、[ThreadPool](thread-pool.md)。
