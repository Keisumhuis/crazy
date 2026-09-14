# MmapInterface

[返回文档目录](../README.md)

`crazy::MmapInterface` 提供跨平台内存映射文件，支持打开、映射、扩容、重映射和关闭。

## API

| 接口 | 说明 |
|------|------|
| `open(filepath, size)` | 打开或创建文件并映射 |
| `close()` | 解除映射并关闭句柄 |
| `data()` | 映射内存首地址 |
| `size()` | 当前映射字节数 |
| `filepath()` | 文件路径 |
| `remmap(newSize)` | 使用新大小重新映射 |

对象不可复制，默认构造后需要调用 `open()`。

## 基本使用

```cpp
#include "crazy/mmap/mmap.h"

{
    crazy::MmapInterface mapping;
    mapping.open("./data.bin", 4096);

    auto* bytes = static_cast<char*>(mapping.data());
    std::memcpy(bytes, "crazy", 5);

    CRAZY_SYSTEM_INFO() << mapping.filepath()
                        << ", size=" << mapping.size();
}
```

## 扩容

```cpp
crazy::MmapInterface mapping("./data.bin", 1024);
mapping.remmap(1024 * 1024);

// 旧指针已经失效，必须重新获取。
void* newAddress = mapping.data();
```

`remmap()` 会让旧地址失效。不能在扩容后继续使用之前保存的 `data()` 指针。

`open(filepath, size)` 在已有文件大于请求尺寸时保留实际文件大小；大于现有文件时会先扩展文件再映射。传入 `size = 0` 且文件为空时不会建立映射。

## 异常与生命周期

`open()`、底层映射、文件扩展和 `remmap()` 失败时会抛出 `std::runtime_error`。未打开文件时调用 `remmap()` 抛出 `std::logic_error`，`new_size = 0` 抛出 `std::invalid_argument`。

```cpp
try {
    crazy::MmapInterface mapping("./data.bin", 4096);
    mapping.remmap(8192);
} catch (const std::exception& error) {
    CRAZY_SYSTEM_ERROR() << error.what();
}
```

析构和 `close()` 会先尝试同步映射内容，再解除映射和关闭句柄。业务仍应为关键数据设计显式恢复机制。

## 文件格式建议

直接把 C++ 结构体映射到文件存在版本、对齐和平台差异。建议文件开头定义固定宽度头：

```text
magic(4) | version(2) | header_size(2) | payload_size(8)
```

读取已有文件时先校验 magic、版本和长度，再访问 payload。不要直接信任外部文件的尺寸字段。

## 持久化

映射修改最终由操作系统写回磁盘，但具体时机受系统缓存和调度影响。对断电恢复、数据库级一致性有严格要求时，应设计日志、校验和或双文件切换机制，而不能只依赖 mmap。

## 相关限制

- 映射区域必须足够大。
- 旧指针在重映射和关闭后失效。
- 多进程同时写需要额外加锁。
- 文件截断或被其他进程修改可能导致访问异常。
- 一个对象未关闭前不能再次 `open()`。
- `sync()` 是受保护接口，业务通过 `close()` 或 `remmap()` 触发同步。

相关文档：[MmapVector](mmap-vector.md)、[FileLock](file-lock.md)、[Buffer](buffer.md)。
