# FileLock

[返回文档目录](../README.md)

`crazy::FileLock` 使用操作系统文件锁实现跨进程互斥，常用于防止程序重复启动或保护单写文件。

## API

| 接口 | 行为 |
|------|------|
| `setFilePath(path)` | 设置锁文件路径 |
| `lock()` | 立即尝试获取锁，失败返回 `false` |
| `tryLock()` | 立即尝试，成功后替换当前锁句柄 |
| `unlock()` | 主动释放锁 |

Windows 使用 `LockFileEx`，Linux 使用 `flock()`。当前实现的两个获取接口都使用非阻塞标志，并没有真正的阻塞等待，因此 `lock()` 名称不代表会等待其他进程释放。

锁由操作系统文件句柄持有，不取决于文件是否仍然存在。

## 单实例程序

```cpp
#include "crazy/file_lock.h"

int main() {
    crazy::FileLock lock("./service.lock");
    if (!lock.tryLock()) {
        CRAZY_SYSTEM_ERROR() << "another instance is running";
        return 1;
    }

    runService();
    return 0;
}
```

## 保护持久化文件

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

锁文件与实际数据文件应保持在同一个部署单元中。分布式或多主机场景需要集中式锁服务，文件锁不能跨主机协调。

## 锁粒度与生命周期

- `FileLock` 对象析构时释放锁。
- 不要删除仍由运行进程持有的锁文件作为“解锁”手段。
- 锁文件路径必须可写且位于稳定目录。
- 文件名冲突会让无关服务互相阻塞。
- 文件锁不提供超时，需要超时时可使用 `tryLock()` 加业务重试。

## 重试封装

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

重试间隔应加入随机抖动，避免多个进程同步争抢。不能把文件存在性当作锁状态，因为进程异常退出后文件可能保留，但系统锁已经释放。

相关文档：[AtomicLock](atomic-lock.md)、[CondMutex](cond-mutex.md)、[Application](application.md)。
