# CondMutex

[返回文档目录](../README.md)

`crazy::CondMutex` 把互斥量和条件变量组合成一个锁对象，提供等待、单次唤醒和全部唤醒。

## API

| 接口 | 说明 |
|------|------|
| `lock()` / `unlock()` / `tryLock()` | 普通互斥量操作 |
| `wait()` | 释放锁并等待通知 |
| `signal()` | 唤醒一个等待线程 |
| `signalAll()` | 唤醒全部等待线程 |
| `CondMutexGuard` | RAII 自动解锁 |

## 生产者与消费者

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

## 正确等待方式

条件变量可能虚假唤醒，也可能在重新获得锁之前条件再次变化，因此必须使用 `while`：

```cpp
while (!ready_) {
    mutex_.wait();
}
```

不要使用 `if`。修改共享条件之前必须持有同一把互斥锁，通知通常可以在临界区之后发送。

`wait()` 假定调用线程已经持有该 CondMutex。它内部使用 `std::adopt_lock` 接管当前锁，等待结束后重新持有锁，再把锁的所有权交回给外层 `CondMutexGuard`。因此下面的结构是有效的：

```cpp
crazy::CondMutexGuard guard(mutex_);
while (!ready_) {
    mutex_.wait();
}
```

如果未先加锁就调用 `wait()`，行为未定义。

## 关闭条件

等待队列不应只检查数据是否存在，还应检查服务是否停止：

```cpp
while (!hasValue_ && !stopped_) {
    mutex_.wait();
}

if (stopped_ && !hasValue_) {
    return std::nullopt;
}
```

这样关闭流程可以调用 `signalAll()` 唤醒所有消费者。

## 多消费者

`signal()` 适合唤醒一个消费者，`signalAll()` 适合状态变化可能让全部等待者继续，例如关闭队列。

```cpp
void shutdown() {
    {
        crazy::CondMutexGuard guard(mutex_);
        stopped_ = true;
    }
    mutex_.signalAll();
}
```

等待条件应同时检查有无数据和是否停止，避免进程关闭时永久阻塞。

相关文档：[AtomicLock](atomic-lock.md)、[ThreadPool](thread-pool.md)、[MVCCLockWrapper](mvcc.md)。
