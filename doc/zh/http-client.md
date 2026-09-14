# HTTP 客户端

[返回文档目录](../README.md)

`crazy::HttpClient` 是同步 HTTP/1.1 客户端，支持常用方法、默认请求头、连接复用和 multipart/form-data。

## 基本请求

```cpp
#include "crazy/net/http/http_client.h"

crazy::HttpClient client;
client.setHeader("Authorization", "Bearer token");

client.get("http://127.0.0.1:8080/health",
           [](crazy::HttpResponse::ptr response) {
               CRAZY_SYSTEM_INFO()
                                   << static_cast<int>(response->status())
                                   << ", " << response->body();
           });

client.post("http://127.0.0.1:8080/users", R"({"name":"alice"})",
            [](crazy::HttpResponse::ptr response) {
                CRAZY_SYSTEM_INFO() << response->body();
            });
```

## 通用方法

```cpp
auto response = client.request(
    crazy::HttpMethod::PATCH,
    "http://127.0.0.1:8080/users/7",
    R"({"enabled":true})",
    {});
```

## 上传表单

```cpp
std::vector<crazy::FormDataField> fields = {
    {"name", "alice"},
    {"role", "admin"},
};

std::vector<crazy::FormDataFile> files = {
    {"avatar", "avatar.png", "image/png", pngContent},
};

client.postForm("http://127.0.0.1:8080/upload", fields, files,
                [](crazy::HttpResponse::ptr response) {
                    CRAZY_SYSTEM_INFO() << response->body();
                });
```

客户端调用会阻塞当前线程，直到响应读取完成。网络请求不应直接放在需要快速响应的 Actor 事件回调中。

## 请求方法

| 接口 | Body | 说明 |
|------|------|------|
| `get(uri, callback)` | 无 | GET 请求 |
| `post(uri, body, callback)` | 有 | POST 请求 |
| `put(uri, body, callback)` | 有 | PUT 请求 |
| `del(uri, callback)` | 无 | DELETE 请求 |
| `head(uri, callback)` | 无 | HEAD 请求 |
| `options(uri, callback)` | 无 | OPTIONS 请求 |
| `patch(uri, body, callback)` | 有 | PATCH 请求 |
| `request(method, uri, body, callback)` | 有 | 通用请求入口 |
| `postForm(uri, fields, files, callback)` | 有 | multipart/form-data 上传 |

所有方法都返回响应指针或通过回调交付响应。回调在发起请求的线程中执行，不会自行创建后台线程。

## 请求构建流程

1. 解析 URI，得到 scheme、host、port 和资源路径。
2. 如果连接目标变化，关闭旧连接并建立新 TCP 连接。
3. 合并默认 Header 与本次请求 Header。
4. 生成 `Host`、`Content-Length`、`Connection` 等必要字段。
5. 序列化请求并完整发送。
6. 增量解析响应头和响应体。
7. 根据 Keep-Alive 规则保留或关闭 Socket。

## 连接复用

`HttpClient` 保存当前 Socket、目标主机、端口和未消费的接收缓冲区。同一主机和端口上的连续请求可以复用连接。服务端返回 `Connection: close`、解析失败或 Socket 断开时，下一次请求会重新建连。

客户端对象通常应按线程使用。多线程并发调用同一个 `HttpClient` 会共享 Socket 和缓冲区，可能造成请求交错；为每个线程创建独立客户端更安全。

## 错误处理

```cpp
auto response = client.request(
    crazy::HttpMethod::GET,
    "http://127.0.0.1:8080/health",
    "",
    [](crazy::HttpResponse::ptr value) {
        CRAZY_SYSTEM_INFO() << value->status();
    });

if (!response) {
    CRAZY_SYSTEM_ERROR() << "request failed";
}
```

连接失败、响应格式错误或提前关闭时可能返回空指针。业务应设置连接和读取超时策略；当前实现以阻塞 Socket 为主，超时能力由底层 Socket 选项或调用方控制。

## 完整上传示例

```cpp
#include "crazy/net/http/http_client.h"

void uploadAvatar(const std::string& pngContent) {
    crazy::HttpClient client;
    client.setHeader("Authorization", "Bearer token");

    std::vector<crazy::FormDataField> fields = {
        {"userId", "7"},
    };
    std::vector<crazy::FormDataFile> files = {
        {"avatar", "avatar.png", "image/png", pngContent},
    };

    client.postForm(
        "http://127.0.0.1:8080/upload",
        fields,
        files,
        [](crazy::HttpResponse::ptr response) {
            CRAZY_SYSTEM_INFO()
                << static_cast<int>(response->status());
        });
}
```

`multipart/form-data` Boundary 由客户端自动生成。文件内容目前以字符串保存在内存中，不适合上传超大文件。

相关文档：[HTTP 服务端](http-server.md)、[URI](uri.md)、[Buffer](buffer.md)。
