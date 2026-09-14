# MVCCLockWrapper

[返回文档目录](../README.md)

`crazy::MVCCLockWrapper<T>` 使用双版本切换实现无锁读取和串行写事务，适合读多写少且可复制的数据。

## 读写版本

```cpp
#include "crazy/mvcc_lock_wrapper.h"

crazy::MVCCLockWrapper<ConfigSnapshot> config(
    ConfigSnapshot{"v1", 10});

{
    auto transaction = config.beginWrite();
    auto& next = transaction.get();
    next.name = "v2";
    next.timeout = 20;
    transaction.commit();
}

int64_t version = 0;
ConfigSnapshot snapshot = config.readWithVersion(version);
if (config.isUpdated(version)) {
    CRAZY_SYSTEM_INFO() << "snapshot changed";
}
```

同一时刻只允许一个写事务。事务析构时默认提交，可以显式调用 `commit()` 或 `abort()`。`readRef()` 返回的引用不具备长期稳定性，写线程切换版本后不要继续保存该引用。

## 工作原理

`MVCCLockWrapper<T>` 内部保存两份值：

```text
value_[0] <-> value_[1]
     当前索引 currentIndex_
     version_ 在每次提交后递增
```

读操作先读取当前索引，再返回对应版本的副本；写事务锁定写互斥量，修改非当前版本，提交时原子切换索引并增加版本号。因此读线程不需要等待写事务完成，但 `T` 的复制成本会直接进入每次读取。

## API 摘要

| 接口 | 说明 |
|------|------|
| `read()` | 读取当前版本副本 |
| `readRef()` | 返回当前值引用，只适合短生命周期 |
| `readWithVersion(version)` | 同时读取值和版本号 |
| `isUpdated(oldVersion)` | 判断版本是否变化 |
| `beginWrite()` | 创建写事务 |
| `transaction.get()` | 获取可写副本 |
| `transaction.set(value)` | 覆盖写副本 |
| `transaction.commit()` | 发布新版本 |
| `transaction.abort()` | 放弃修改 |

## 配置快照完整示例

```cpp
struct RuntimeConfig {
    uint32_t timeoutMs = 1000;
    uint32_t retryCount = 3;
    std::string route;
};

class ConfigStore {
public:
    RuntimeConfig get(int64_t* version = nullptr) const {
        if (version) {
            return config_.readWithVersion(*version);
        }
        return config_.read();
    }

    void update(const RuntimeConfig& value) {
        auto tx = config_.beginWrite();
        tx.set(value);
        tx.commit();
    }

private:
    crazy::MVCCLockWrapper<RuntimeConfig> config_{
        RuntimeConfig{1000, 3, "/v1"}};
};
```

## 等待版本变化

`MVCCLockWrapper` 只保存版本号，不提供条件变量通知。需要等待版本变化时，可以由业务在提交后调用自己的通知机制：

```cpp
ConfigStore store;
crazy::CondMutex changed;

store.update(RuntimeConfig{2000, 5, "/v2"});
changed.signal();
```

不要在持有写事务时等待另一个写事务，因为写互斥量会形成死锁。

## 成本与限制

- 每次读取都会复制 `T`，大对象应改用共享不可变指针。
- 同时只支持两个版本，长读操作不能按任意旧版本进行一致性快照。
- 只保证单对象版本切换，不提供跨多个值的原子事务。
- `readRef()` 本质上是危险的长期引用，优先使用 `read()`。
- 写入仍需互斥，适合读多写少而不是高并发写。

相关文档：[AtomicLock](atomic-lock.md)、[CondMutex](cond-mutex.md)、[FileLock](file-lock.md)、[ThreadPool](thread-pool.md)、[JSON 序列化](json.md)。
