# MmapVector

[返回文档目录](../README.md)

`crazy::MmapVector<T>` 在 `MmapInterface` 上提供持久化连续数组，数据保存在文件中，可跨进程重新打开。

## 类型约束

模板要求 `T` 可平凡复制：

```cpp
static_assert(std::is_trivially_copyable_v<T>);
```

不能直接保存 `std::string`、`std::vector` 或带自定义析构函数的对象。需要保存复杂记录时，应使用固定大小 POD、偏移表，或者自己实现序列化。

## 文件布局

| 区域 | 内容 |
|------|------|
| Header | `uint64_t size`、`uint64_t capacity` |
| Data | 连续的 `T` 元素 |

`size` 是逻辑元素数量，`capacity` 是当前映射可容纳的元素数量。

## 基本使用

```cpp
#include "crazy/mmap/mmap_vector.h"

crazy::MmapVector<uint64_t> values("./values.bin", 4);
values[0] = 10;
values[1] = 20;
values[3] = 40;

for (uint64_t value : values) {
    CRAZY_SYSTEM_INFO() << value;
}

if (auto* value = values.at(2)) {
    CRAZY_SYSTEM_INFO() << *value;
} else {
    CRAZY_SYSTEM_INFO() << "index 2 is not committed to size";
}
```

写入 `values[3]` 会把 `size()` 更新为 4，但索引 2 没有显式初始化。业务格式必须定义缺省槽位是否有效。

## 自动扩容

```cpp
crazy::MmapVector<int32_t> samples("./samples.bin", 2);
samples[1000] = 7;

// samples[1000] 可能触发 remmap，因此必须重新获取指针。
auto begin = samples.begin();
```

扩容通常按约两倍容量增长。频繁追加大量数据时，预分配合适容量可以减少重映射。

当新文件首次以非零容量打开时，Header 的 `size` 被初始化为 0，`capacity` 被设置为请求容量。已有文件会保留原 Header，构造参数中的容量不会覆盖已有 `capacity`。

## 安全访问模式

```cpp
void append(crazy::MmapVector<uint64_t>& values, uint64_t value) {
    const uint64_t index = values.size();
    values[index] = value;
}

std::optional<uint64_t> find(
    crazy::MmapVector<uint64_t>& values,
    uint64_t index) {
    if (auto* value = values.at(index)) {
        return *value;
    }
    return std::nullopt;
}
```

追加时使用当前 `size()` 作为索引可以避免无意创建空洞。读取时使用 `at()` 而不是无边界检查的 const `operator[]`。

## 文件大小校验

打开外部文件后至少要确认：

```text
file_size >= sizeof(MmapVectorHeader)
file_size >= header.size * sizeof(T) + header size
header.size <= header.capacity
```

如果业务已有 Header 和元素布局版本，还应增加版本校验。损坏的 `capacity` 可能导致映射尺寸计算溢出。

## 进程共享

两个进程可以映射同一文件，但没有内置并发控制。多写者场景需要：

1. 使用 `FileLock` 或外部锁保护读写。
2. 定义发布顺序和可见性。
3. 避免一个进程扩容时另一个进程使用旧映射。

常见做法是单写多读，并通过版本号或锁文件通知读者重新打开。

## 数据校验

打开不可信文件后应校验：

- Header 是否能完整读取。
- `capacity` 和文件大小是否匹配。
- `size` 是否不大于 `capacity`。
- 元素类型和布局版本是否一致。

相关文档：[MmapInterface](mmap-interface.md)、[FileLock](file-lock.md)、[二进制协议](protocol.md)。
