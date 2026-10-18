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

## 事件驱动（注册到 selector / actor）

`requestAsync()` 不等响应：建连（含 `https` 的 TLS 握手）完成后把请求写出去，响应由 selector 驱动的
`onReadable()` 增量解析，完成时在 selector 线程触发回调；有积压数据时客户端自己挂上写事件。

```cpp
class Fetcher : public crazy::ActorInterface {
public:
    Fetcher() : ActorInterface("fetcher") {}

    void init() override {
        client_.registerSelectorCallbacks(
            [this](int32_t fd, crazy::SelectorEventType type, std::function<void()> callback) {
                registerEventOnThread(fd, type, std::move(callback));
            },
            [this](int32_t fd, crazy::SelectorEventType type) {
                unregisterEventOnThread(fd, type);
            });
        client_.requestAsync(crazy::HttpMethod::GET, "https://example.com/", "",
            [](crazy::HttpResponse::ptr response) {
                // 在 actor 线程触发；连接或解析失败时 response 为 nullptr
            });
    }

private:
    crazy::HttpClient client_;
};
```

- 同一客户端同一时刻只允许一个在途请求（`isBusy()`）；上一个响应完成后可以直接发下一个，keep-alive 连接会被复用；
- 不用 actor 时把这两个回调直接绑到 `crazy::Selector::registerEvent` / `unregisterEvent` 即可（见 `tests/test_http_client_async.cc`）；
- 超时/重试自己配 `Selector::registerTimer()`。

## HTTPS（TLS）

`https://` **真的走 TLS**：构建期 TLS 支持**默认开启**（`CRAZY_ENABLE_TLS` 默认 `ON`，使用仓库内
`src/third_party/openssl`；可用 `-DCRAZY_ENABLE_TLS=OFF` 显式关闭），`HttpClient` 对 `https://` 会建立 TLS 连接、
校验服务端证书、完成握手，然后才发送请求。**TLS 不可用或校验失败时 `request()` 返回 `nullptr`，
绝不会把请求明文发到 443**（在此之前 `https://` 只是把端口改成 443，实际是明文，属于假性安全）。

> **编译期默认支持 ≠ 运行期默认启用。** 客户端只有在请求 `https://` / `wss://` 时才真正建立 TLS 连接，
> 请求 `http://` / `ws://` 时依旧是明文。

客户端参数同样取 `[HTTP]`/`[http]` 节：

| 键 | 类型 | 默认 | 说明 |
|------|------|------|------|
| `tls_ca_file` | string | 空 | 信任的 CA（PEM）路径；为空时使用下面的「默认信任库」（OpenSSL 默认验证路径 + 随包 CA bundle） |
| `tls_verify_peer` | bool | `true` | 是否校验证书链与有效期，**客户端默认开启**，只有显式设为 `false` 才跳过 |
| `tls_min_version` | string | `"1.2"` | 只接受 `"1.2"` / `"1.3"` |

```ini
[HTTP]
tls_ca_file = /etc/crazy/ca.pem
tls_verify_peer = true
```

### 默认信任库与覆盖方式（随包 CA bundle）

`tls_ca_file` 为空时客户端**默认就能校验真实公网 HTTPS 站点**：框架把一份公认的 CA bundle
（`src/third_party/ca/cacert.pem`，121 张证书 / 188900 字节，取自 `https://curl.se/ca/cacert.pem`，
数据源自 Mozilla CA extract；来源与许可见仓库根 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)）
**编译进了库**（configure 期由 CMake 读 PEM，生成源码树内的 `src/crazy/net/ca_bundle.cpp`
再作为普通源文件编进静态库），
运行期**不依赖**任何外部文件、环境变量或相对路径。

信任库的优先级是固定的：

| 顺序 | 条件 | 实际使用的信任库 |
|---|---|---|
| ① | `tls_ca_file` 非空 | **只用该文件**；随包 CA bundle **不叠加**（否则「只信私有 CA / 自签夹具 CA」不成立） |
| ② | `tls_ca_file` 为空，但设置了 `SSL_CERT_FILE` 或 `SSL_CERT_DIR` | **只用显式指定的信任库**；随包 bundle **不叠加**（显式配置优先，不被 bundle 兜住） |
| ③ | `tls_ca_file` 为空，也未设置这两个环境变量 | OpenSSL 默认验证路径**加上**随包 CA bundle |

要点：

- **显式指定永远优先。** 把 `SSL_CERT_FILE` 指向一份**不包含目标站点 CA** 的文件后，`https://` 会**真的失败**
  （`unable to get local issuer certificate`），不会被随包 bundle 悄悄救回来。
- 随包 bundle **只用于客户端校验**，与服务端的 `tls_cert_file` / `tls_key_file` 无关。
- 客户端默认**不准备**第二份信任库去覆盖 `tls_ca_file`：显式给 `tls_ca_file` 时只有它生效。
- 若 configure 期找不到该 PEM，构建会**告警**（不硬失败），客户端退回「只用 OpenSSL 默认验证路径」，
  此时公网站点会因信任库为空而握手失败。
- 更新 bundle：替换 `src/third_party/ca/cacert.pem` 后重新 configure 即可（会重新生成
  `src/crazy/net/ca_bundle.cpp` 并重新编译进库）；也可用 `-DCRAZY_CA_BUNDLE_FILE=<path>` 指向别处的 PEM。
- 诊断接口：`crazy::bundledCaCertificateCount()` / `crazy::bundledCaSource()`
  （见 `crazy/net/ca_bundle.h`）。

需要更细的控制（例如自定 CA、限制最高协议版本）时可以直接使用 `TlsContext` + `TlsSocket`：

```cpp
std::string error;
crazy::TlsOptions options;
options.caFile = "/etc/crazy/ca.pem";
options.verifyPeer = true;
auto context = crazy::TlsContext::createClient(options, error);
if (!context) {
    CRAZY_SYSTEM_ERROR() << error;   // 例如：load tls_ca_file failed, file = ...
}

auto socket = std::make_shared<crazy::TlsSocket>(
    context, crazy::TlsSocket::Side::client);
socket->setVerifyHost("example.com");   // 校验证书里的主机名/IP
if (!socket->connect("example.com", 443) || !socket->handshake(error)) {
    CRAZY_SYSTEM_ERROR() << error;      // 例如：certificate verify failed: certificate has expired
}
```

失败一律表现为 `request()` 返回 `nullptr` 并写日志，触发条件包括：证书链不可信、**证书已过期**、
主机名与证书不匹配、`tls_ca_file` 指向不存在的路径（初始化阶段即失败，不跳过校验）、
以及构建期用 `-DCRAZY_ENABLE_TLS=OFF` 关闭了 TLS 支持（此时连 TCP 连接都不会建立）。

WebSocket 客户端同理：`wss://` 走同一套 TLS 与同一节配置，失败时 `connect()` 返回 `false`，
详见 [WebSocket](websocket.md)。

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
| `request(method, uri, body, callback, dataCallback)` | 有 | 通用请求入口，可附加响应体逐段数据回调 |
| `postForm(uri, fields, files, callback)` | 有 | multipart/form-data 上传 |

所有方法都返回响应指针或通过回调交付响应。回调在发起请求的线程中执行，不会自行创建后台线程。

## 分块上传（Transfer-Encoding: chunked）

默认情况下请求体按 `Content-Length` 发送。服务端更希望边收边处理、或长度事先未知时，可以切到分块上传：

```cpp
crazy::HttpClient client;
client.setChunkedUpload(true, 8192);   // 开启分块，单块最大 8192 字节

auto response = client.request(crazy::HttpMethod::POST,
                               "http://127.0.0.1:8080/upload",
                               payload, nullptr);
```

- 请求头写 `Transfer-Encoding: chunked`，**不写** `Content-Length`，请求体按 `chunkSize` 切块并以终止块 `0\r\n\r\n` 收尾；
- 分块上传只影响请求体，`get` / `head` 等无请求体的方法照常；
- 开启分块后默认请求头里不得再有 `Content-Length`：此时 `request()` 返回 `nullptr`、`requestAsync()` 返回 `false`，并且**不会向连接写出任何字节**（不会出现半截畸形请求）；
- 业务直接构造 `HttpRequest` 时用 `setChunkedTransferEncoding(true)` + `setChunkSize()`；此时若还设置了 `Content-Length`，`toString()` 直接抛 `std::logic_error`，不会静默删头；
- 分块传输编码是 HTTP/1.1 引入的：把请求版本设成 `HTTP/1.0` 再序列化同样抛 `std::logic_error`；
- 单个数据块的大小只是切块粒度，不会改变服务端收到的请求体内容。

服务端侧（`crazy::HttpSession` / `HttpRouter`）会把分块请求体去分块后交给 Handler，`request.body()` 就是完整 payload；请求的 trailer 落在 `request.trailers()`。服务端用法见 [HTTP 服务端](http-server.md)。

## 逐段接收分块响应

`request()` 的最后一个参数是响应体数据回调。响应体数据到达时就会回调，不需要等整个响应结束：

```cpp
std::string streamed;
auto response = client.request(
    crazy::HttpMethod::GET,
    "http://127.0.0.1:8080/stream",
    "",
    [](crazy::HttpResponse::ptr resp) {
        CRAZY_SYSTEM_INFO() << static_cast<int>(resp->status());
    },
    [&streamed](const std::string& data) {
        streamed += data;  // 数据到达即交付
    });
```

- 数据回调在发起请求的线程执行，回调顺序与数据到达顺序一致；
- 交付粒度是「至少一次、且不晚于后续数据到达」，TCP 分段可能把同一个 chunk 的数据拆成多次交付，因此不要假设「一次 chunk 一次回调」；
- 响应结束后 `response->body()` 仍是所有 chunk 数据按序拼接的完整结果，返回值与响应回调语义保持不变；
- 非分块（`Content-Length`）响应也会逐段回调，不需要时省略该参数即可。

## Transfer-Encoding 与 Content-Length

响应**同时**包含 `Transfer-Encoding: chunked` 与 `Content-Length` 时按解析错误处理：`HttpResponseParser::hasError()` 为 `true`、`isFinished()` 为 `false`，`request()` 返回 `nullptr`，响应回调不会被调用，也不抛异常。框架不会把这种输入当作可容错报文处理。

chunk 长度字段非法（例如 `Z\r\n`）、声明 chunked 却没有终止块 `0\r\n\r\n`、或者连接在 chunk 数据中途关闭，失败表现相同：`request()` 在有限时间内返回 `nullptr`、回调不被调用、不抛异常。当前实现不对外区分失败原因（只有成功与失败两种结果），需要区分失败原因时应另立需求。

## Trailer 与临时 1xx 响应

- 分块响应的 trailer 字段在终止块之后解析，写入 `response->trailers()`，**不会**混进 `response->headers()`；
- `100 Continue` 等临时响应（1xx，101 除外）会被自动跳过：`HttpResponseParser` 继续解析最终响应，`interimResponseCount()` 记录跳过数量，`isFinished()` 只在最终响应结束时为 `true`。因此手工在请求头里带 `Expect: 100-continue` 也不会把 `100 Continue` 当成结果返回。

## 请求构建流程

1. 解析 URI，得到 scheme、host、port 和资源路径。
2. 如果连接目标变化，关闭旧连接并建立新 TCP 连接。
3. 合并默认 Header 与本次请求 Header。
4. 生成 `Host`、`Content-Length`（分块上传时改为 `Transfer-Encoding: chunked`）、`Connection` 等必要字段。
5. 序列化请求并完整发送。
6. 增量解析响应头和响应体（临时 1xx 响应被跳过，trailer 写入 `trailers()`）。
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
