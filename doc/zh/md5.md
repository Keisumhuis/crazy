# MD5

[返回文档目录](../README.md)

`crazy::MD5` 提供 MD5 摘要计算。它适合遗留协议兼容和普通内容校验，不适合安全敏感场景。

## API

| 接口 | 说明 |
|------|------|
| `MD5(message)` | 构造并计算摘要 |
| `getDigest()` | 返回 16 字节摘要指针 |
| `toString()` | 返回十六进制摘要 |
| `MD5::encryption(message)` | 一次性计算十六进制摘要 |

## 基本示例

```cpp
#include "crazy/encryption/md5.h"

const std::string hex =
    crazy::MD5::encryption("hello");

crazy::MD5 md5("hello");
const uint8_t* bytes = md5.getDigest();

CRAZY_SYSTEM_INFO() << hex;
CRAZY_SYSTEM_INFO() << static_cast<int>(bytes[0]);
```

## 文件或分块内容

当前公开接口以完整字符串为输入：

```cpp
std::string fileContent = readWholeFile(path);
const std::string digest =
    crazy::MD5::encryption(fileContent);
```

大文件会整体占用内存。更重要的是 MD5 不具备抗碰撞能力，不能用于判断下载文件来自可信来源，只能用于发现普通传输错误。

## 摘要对象行为

`MD5(message)` 构造后保存消息状态，`getDigest()` 第一次调用时完成填充和最终计算，返回 16 字节内部缓冲区。再次调用会返回同一结果。`toString()` 会调用 `getDigest()` 并输出固定 32 个小写十六进制字符。

```cpp
crazy::MD5 digest("hello");
const std::string first = digest.toString();
const std::string second = digest.toString();

// first 与 second 相同。
```

MD5 对象不是线程安全的，不应由多个线程并发调用同一个对象的 `getDigest()` 或 `toString()`。

## 输出格式

| 形式 | 长度 | 用途 |
|------|------|------|
| 原始摘要 | 16 字节 | 二进制协议、继续处理 |
| 十六进制字符串 | 32 字符 | 日志、文本配置、兼容接口 |

日志中原样输出原始字节可能包含不可打印字符，通常更适合使用 `toString()`。

## 安全建议

- 不要保存 MD5 密码摘要。
- 不要用 MD5 签名或验证安全 Token。
- 需要完整性时使用 SHA-256 等现代摘要并验证可信来源。
- 需要密码存储时使用带盐的 Argon2、scrypt 或 bcrypt。
- 对用户输入的摘要字符串限制长度并校验十六进制格式。

相关文档：[Base64](base64.md)、[配置管理](config.md)、[SMTP 客户端](smtp.md)。
