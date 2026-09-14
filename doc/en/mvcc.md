# MVCCLockWrapper

[Back to documentation index](../README.en.md)

`crazy::MVCCLockWrapper<T>` uses two versions to provide lock-free reads and serialized write transactions. It fits read-heavy, copyable state.

## Read and write versions

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

Only one write transaction may be active at a time. A transaction commits by default on destruction; call `commit()` or `abort()` explicitly when control flow is clearer. Do not retain references returned by `readRef()` across version changes.

## How it works

The wrapper stores two values and an atomic current index. Readers copy the current value without taking the write mutex. A write transaction locks the mutex, modifies the inactive slot, then flips the index and increments the version.

```text
value_[0] <-> value_[1]
currentIndex_ selects the published value
version_ increments on commit
```

## API summary

| API | Purpose |
|-----|---------|
| `read()` | Copy the current value |
| `readRef()` | Borrow the current value briefly |
| `readWithVersion()` | Read a value and version |
| `isUpdated()` | Compare an old version |
| `beginWrite()` | Acquire a write transaction |
| `get()` / `set()` | Modify the inactive value |
| `commit()` / `abort()` | Publish or discard |

## Complete configuration store

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

## Costs and limits

- Every read copies `T`; very large values should use shared immutable state.
- Only two versions exist, not a full historical snapshot chain.
- Only one object is switched atomically.
- Writes are still serialized by a mutex.
- Prefer `read()` over retaining `readRef()`.

Related: [AtomicLock](atomic-lock.md), [CondMutex](cond-mutex.md), [FileLock](file-lock.md), [ThreadPool](thread-pool.md), [JSON](json.md).
