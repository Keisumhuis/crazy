# Base64

[返回文档目录](../README.md)

`crazy::Base64` 提供字符串级 Base64 编码和解码，用于把任意字节放入 JSON、HTTP 头或文本协议。

## API

| 接口 | 输入 | 输出 |
|------|------|------|
| `encryption(message)` | 原始字节 | Base64 文本 |
| `decryption(message)` | Base64 文本 | 原始字节 |

接口使用 `std::string` 承载字节，因此内容中可以包含 `\0` 等不可打印字符。

## 基本示例

```cpp
#include "crazy/encryption/base64.h"

const std::string binary("\x00\x01\x02\x03", 4);
const std::string encoded = crazy::Base64::encryption(binary);
const std::string decoded = crazy::Base64::decryption(encoded);

CRAZY_SYSTEM_INFO() << encoded;
CRAZY_SYSTEM_INFO() << decoded.size();
```

## 在消息正文中使用

```cpp
auto message = std::make_shared<crazy::MessageBase>();
message->setCmd(3001);
message->setData(crazy::Base64::encryption(pngBytes));
```

接收方需要根据命令判断正文是 Base64 文本还是原始二进制，并检查解码后的长度。

## 大小与限制

编码后长度约为原始长度的 4/3，并可能包含填充字符 `=`。传输协议必须为解码结果预留空间。当前接口不会对超长输入设置统一上限，调用方应根据业务协议限制输入大小。

| 输入长度 | 编码长度 | 填充 |
|----------|----------|------|
| 1 | 4 | `==` |
| 2 | 4 | `=` |
| 3 | 4 | 无 |
| 4 | 8 | `==` |

`decryption()` 遇到非 Base64 字符或填充符时会停止，并返回已经解码的前缀。它当前不提供错误码，因此解码外部输入前应自行校验字符集、长度和填充规则。

## 嵌入 JSON

```cpp
struct FilePayload {
    std::string name;
    std::string contentBase64;
    REFLECTION(name, contentBase64);
};

FilePayload payload{
    "avatar.png",
    crazy::Base64::encryption(pngBytes),
};

const std::string json =
    crazy::json::Converter::Serializable(payload);
```

接收端解码前应检查 `contentBase64.size()` 是否在业务允许范围内。

## 安全说明

Base64 不是加密，也不会隐藏数据内容。它不能替代 TLS、签名或访问控制。不要把口令、Token 或隐私数据仅靠 Base64 包装后发送到不可信链路。

相关文档：[MD5](md5.md)、[JSON 序列化](json.md)、[HTTP 服务端](http-server.md)。
