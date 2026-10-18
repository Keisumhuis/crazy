# HTTP 服务端

[返回文档目录](../README.md)

`crazy::HttpApplication` 在 `Application` 基础上提供 HTTP/1.1 路由、异步响应和静态文件服务。

## 注册路由

```cpp
#include "crazy/http_application.h"

int main(int argc, char** argv) {
    crazy::HttpApplication app(argc, argv);
    app.listen(8080, "0.0.0.0");

    app.registerHttpHandler<crazy::GET>(
        "/", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
            response.setBody("hello crazy");
        });

    app.registerHttpHandler<crazy::GET, crazy::POST>(
        "/users", [](crazy::HttpRequest& request,
                     crazy::HttpResponse& response) {
            response.setHeader("Content-Type", "application/json");
            response.setBody("{\"method\":\"" + request.methodString() + "\"}");
        });

    app.exec();
}
```

## HTTPS（TLS）

TLS 能力落在 **Socket 层**（`crazy::TlsSocket`），HTTP 层不需要为了 TLS 改动：开启后监听端口只接受
TLS 连接，握手与解密完成后，明文字节流交给同一套 `HttpSession` 解析——因此 **WebSocket over TLS（WSS）
服务端不需要额外代码**。

启用需要两步：构建期获得 TLS 支持（**默认开启**，编译期支持即可），运行期再在配置里显式给出证书与开关。

```bash
# CRAZY_ENABLE_TLS 默认 ON：默认构建即包含 TLS 支持（静态链接仓库内 src/third_party/openssl），
# 不需要任何 -D 参数；下面这条显式写法与默认配置等价。
cmake -S . -B build/tls -DCRAZY_ENABLE_TLS=ON

# 显式关闭 TLS 支持（此时 https:// / wss:// 会显式失败，绝不静默退化成明文）：
cmake -S . -B build/notls -DCRAZY_ENABLE_TLS=OFF
```

> **编译期默认支持 ≠ 运行期默认启用。** 只有像下面这样在 `[HTTP]` 节配好 `tls_enable = true` 与可读的
> 证书、私钥，监听端口才真正走 HTTPS；该节缺失或 `tls_enable = false` 时，端口仍按明文 HTTP 服务。

```ini
[HTTP]
port = 8443
address = 0.0.0.0
tls_enable = true
tls_cert_file = /etc/crazy/server_cert.pem
tls_key_file = /etc/crazy/server_key.pem
tls_min_version = 1.2
```

| 键 | 类型 | 默认 | 说明 |
|------|------|------|------|
| `tls_enable` | bool | `false` | `false` 或缺失 = 与今天完全一致的明文 |
| `tls_cert_file` | string | 空 | 证书链 PEM 路径（服务端必须） |
| `tls_key_file` | string | 空 | 私钥 PEM 路径（服务端必须） |
| `tls_min_version` | string | `"1.2"` | 只接受 `"1.2"` / `"1.3"`；其他值是配置错误：打出降级告警并以明文继续服务（见下） |
| `tls_ca_file` | string | 空 | 仅客户端角色使用，见 [HTTP 客户端](http-client.md) |
| `tls_verify_peer` | bool | 客户端 `true` / 服务端 `false` | 是否校验对端证书 |

只支持 **PEM** 证书与私钥；PFX / PKCS#12 不在支持范围。

TLS 键与节名一起走已有的**大小写回退**（`[HTTP]` 优先，其次 `[http]`），并且 TLS 配置的读取
**独立于 `app.listen()`**：无论端口来自 `app.listen(port, address)` 还是来自配置节，`tls_*` 都同样生效，
不会出现「调用方自己 listen 之后监听静默保持明文」。

程序化用法可以直接给出 `TlsContext`：

```cpp
crazy::TlsOptions options;
options.certFile = "server_cert.pem";
options.keyFile = "server_key.pem";
std::string error;
auto context = crazy::TlsContext::createServer(options, error);

crazy::HttpServer server;
server.listen(8443, "0.0.0.0", context);
```

### TLS 初始化失败：降级为明文并打出醒目告警

**可用性优先**（Lead 决策，2026-10-10「没有证书的时候也要正常运行」）：`tls_enable = true` 但 TLS
初始化失败时，`startServer()` **不会拒绝启动、也不会跳过监听**，而是**降级为明文继续监听该端口**，
同时打印一条**醒目告警**（`[TLS ALERT]`，error 级，同时进控制台与日志文件）。告警里写明
「TLS 已启用但加载失败，现以**明文**提供服务」、**具体文件路径**与底层错误；降级后明文客户端能正常
拿到 HTTP 响应，失败原因也可以通过 `HttpApplication::tlsInitError()` 查询（空表示没有失败）。

触发条件包括：

- `tls_cert_file` / `tls_key_file` 为空、缺失或不可读；
- 私钥与证书不匹配；
- `tls_min_version` 取值非法（例如 `"1.1"`）；
- 构建时 `CRAZY_ENABLE_TLS=OFF` 却把 `tls_enable` 配成 `true`。

> **安全声明（务必阅读）**：这是**可用性优先**的取舍。部署方若依赖 HTTPS 保护，**必须监控启动告警**
> ——框架**不保证**「配置了 TLS 就一定在跑 TLS」。需要「证书缺失即拒绝启动」的部署，应在启动脚本或
> 健康检查里读取 `HttpApplication::tlsInitError()`（或日志中的 `[TLS ALERT]`）并主动终止进程。

握手失败的连接会被直接关闭，**HTTP 层一个字节都不会收到**（明文请求打到 HTTPS 端口同样如此），
服务端随后仍能正常服务下一个合法 TLS 连接。

### 握手期阻塞与超时

业务 socket 目前是**阻塞**的（这是框架既有行为，本版本不改造），而 `SSL_accept` 在阻塞 fd 上会一直等到
握手完成，因此本版本给握手加了**有界超时**：握手期间在底层 socket 上设置 `SO_RCVTIMEO` / `SO_SNDTIMEO`
（默认 5 秒），握手完成或失败后立即恢复为无限等待。只连不发、或只发半个 ClientHello 的客户端
最多占用该时长，之后连接被关闭。彻底消除这一 DoS 面需要把业务 socket 改成非阻塞 IO，属**另行立项**的范围。

### 正常关闭

`HttpSession` 的正常关闭路径会先发送 TLS `close_notify` 再关闭底层连接；关闭前还会**有界地**读走内核里
未读的数据，避免 `closesocket` 因未读数据触发 RST，让对端把干净结束看成「连接被截断」。对端正常
`close_notify` 时 `TlsSocket::recv` 返回 `0`，被截断时返回负值，两者可区分。

## 异步响应

```cpp
app.registerHttpHandler<crazy::GET>(
    "/async",
    [&app](crazy::HttpRequest&, crazy::HttpResponse& response) {
        auto deferred = response.defer();
        app.enqueueRunnable([deferred] {
            deferred->setBody("from worker");
            deferred->send();
        });
    });
```

同一个响应重复调用 `send()` 只会发送一次（分块响应除外，见「分块响应」）。后台线程可以安全调用 `send()`，实际写入仍会回到会话线程。

## 分块响应（chunked）

响应体长度未知或需要边产生边发送时，用 `beginChunked()` / `writeChunk()` / `endChunked()` 输出分块响应。三个动作由 `defer()` 返回的 `shared_ptr` 驱动，可以在 Handler 返回后由其他线程继续追加：

```cpp
app.registerHttpHandler<crazy::GET>(
    "/stream",
    [&app](crazy::HttpRequest&, crazy::HttpResponse& response) {
        auto deferred = response.defer();
        deferred->beginChunked();          // 发送状态行与响应头
        deferred->writeChunk("Hello");     // 每段写成 <十六进制长度>\r\n<数据>\r\n
        app.enqueueRunnable([deferred] {
            deferred->writeChunk(" ");
            deferred->writeChunk("World");
            deferred->endChunked();        // 发送终止块 0\r\n\r\n
        });
    });
```

分块模式下写出的语义：

- `beginChunked()` 只发送状态行与响应头，头段含 `Transfer-Encoding: chunked`，**不含** `Content-Length`；此后框架按 RFC 7230 第 4.1 节编码每个数据段；
- `writeChunk()` 可调用任意多次，每次追加一个数据块（空数据段被忽略）；
- 必须调用 `endChunked()` 发送终止块 `0\r\n\r\n`，否则客户端会一直等待；
- `isChunked()`、`isChunkedFinished()` 可用于诊断状态。

分块模式下业务若显式设置了 `Content-Length`，`beginChunked()` **直接抛出 `std::logic_error`**（Handler 内抛出会被框架转换为 `500` 响应），框架不会静默删除该头。由框架产生的分块响应字节中不会同时出现 `Transfer-Encoding: chunked` 与 `Content-Length`。

### 数据源方式（大响应按需拉取）

数据量超过内存承受能力时（大文件、导出流），用 `beginChunked(ChunkSource)` 把数据交给框架按需拉取：

```cpp
app.registerHttpHandler<crazy::GET>(
    "/big",
    [](crazy::HttpRequest&, crazy::HttpResponse& response) {
        auto file = std::make_shared<std::ifstream>("big.bin", std::ios::binary);
        auto remaining = std::make_shared<std::uint64_t>(fileSize);
        response.beginChunked([file, remaining](std::string& buffer) -> std::size_t {
            if (*remaining == 0) {
                return 0;                 // 返回 0 表示数据结束
            }
            // 向 buffer 追加一段数据，返回追加的字节数
            ...
        });
    });
```

`ChunkSource` 在会话线程上调用，**只有内核写缓冲腾出空间时才拉取下一段**，因此内存只占用一个数据块（会话高水位 64 KiB），不会把整份数据堆进发送缓冲。返回 0 表示结束；抛异常会直接断开连接（客户端看到截断的响应，不会把半截数据当作完整响应）。数据源与 `writeChunk()` 互斥，不要混用。

### Trailer

分块响应可以在头段结束后再发送尾部字段：

```cpp
response.addTrailer("X-Checksum", "abc123");   // 必须在 beginChunked 之前
response.beginChunked();
response.writeChunk("data");
response.endChunked();
```

- `addTrailer()` 必须在 `beginChunked()` 之前调用，之后调用抛 `std::logic_error`；
- 头段自动声明 `Trailer: x-checksum`（业务已手工设置 `Trailer` 头时不重复添加）；
- 终止块写成 `0\r\n<trailer 字段>\r\n`；客户端用 `response.trailers()` 读取，不会混进 `headers()`。

### HTTP/1.0 客户端降级

HTTP/1.0 不认识分块编码，会话检测到请求版本低于 1.1 时自动降级：

- 响应头不写 `Transfer-Encoding: chunked`，也不写 `Content-Length`；
- 强制 `Connection: close`，响应体由**关闭连接**界定（RFC 7230 第 3.3.3 节）；
- 数据源方式同样适用：HTTP/1.0 客户端拿到的仍是完整数据，只是没有分块帧。

### Expect: 100-continue

客户端请求头带 `Expect: 100-continue` 时（HTTP/1.1 及以上），框架在请求体到达之前先回 `HTTP/1.1 100 Continue\r\n\r\n`，再继续等请求体；否则客户端在等临时响应、服务端在等请求体，双方互相僵死。带 `Content-Length` 与 `Transfer-Encoding: chunked` 的请求体都适用。

### 连接生命周期

- 只有正常写出终止块 `0\r\n\r\n` 后，连接才按请求的 keep-alive 规则决定复用；分块响应结束前该连接不会处理后续请求；
- 客户端中途断开、写失败等异常中断会强制关闭连接，不会继续复用。

不要通过手工插入 `Transfer-Encoding: chunked` 头来产生分块响应：`toString()` 只会原样追加 `body()`，会得到「声称分块、实际未分块」的畸形报文。服务端本身能解析入站的分块请求体（Handler 里的 `request.body()` 是去分块后的完整数据，请求的 trailer 落在 `request.trailers()`）；客户端分块上传见 [HTTP 客户端](http-client.md)。

## 静态文件

```cpp
// 不显示目录列表
app.registerStaticDirectory("/static", "./public");

// 打开目录列表
app.registerStaticDirectory("/files", "./public", true);
```

静态目录只处理 `GET`。业务路由优先，路径穿越、`..` 路径段以及解析后越出根目录的符号链接会返回 `403`。

### 默认分块发送

静态资源**默认按分块方式流式发送**，不会把整个文件读进内存，也不写 `Content-Length`：

- 每个数据块最多 `HttpRouter::kStaticStreamChunkSize`（64 KiB），会话只在可写时逐块拉取；
- 响应头带 `Accept-Ranges: bytes` 与 `ETag`；
- HTTP/1.0 客户端自动降级为「关闭连接界定响应体」（见「HTTP/1.0 客户端降级」）；
- 空文件没有数据可流式发送，回 `200` + `Content-Length: 0`。

### Range 分片下载

客户端带 `Range` 时按 RFC 7233 返回分片，可用于断点续传与大文件随机读取：

```bash
curl -r 0-1023  http://127.0.0.1:8080/static/big.bin   # 206 + Content-Range
curl -r 1024-   http://127.0.0.1:8080/static/big.bin   # 从 1024 到结尾
curl -r -1024   http://127.0.0.1:8080/static/big.bin   # 末尾 1024 字节
```

- 只支持**单区间**；多区间（`bytes=0-1,4-5`）与非法语法按「忽略 Range」处理，回 `200` 完整响应；
- 命中返回 `206 Partial Content` + `Content-Range: bytes <first>-<last>/<size>`，响应体仍按分块方式发送；
- 区间越界（`first >= size`、`first > last`、后缀长度为 0、空文件）返回 `416 Range Not Satisfiable` + `Content-Range: bytes */<size>`；
- `If-Range` 支持 ETag 形式：值与响应头里的 `ETag` 相同才走 `206`，否则忽略 Range 回 `200` 完整内容；`Last-Modified` 形式暂不支持（静态响应不返回 `Last-Modified`，按不匹配处理）；
- 断点续传：先取一次 `ETag`，之后每次带 `Range: bytes=<已下载字节>-` 与 `If-Range: <ETag>` 即可。

## HttpApplication 接口

| 接口 | 说明 |
|------|------|
| `listen(port, address)` | 设置 HTTP 监听端口和地址 |
| `listen(port, address, tlsContext)` | 同上，并让该端口只接受 TLS（`TlsContext` 为空即明文） |
| `registerHttpHandler<Methods...>()` | 注册自由函数、lambda、成员函数或 `shared_ptr` 对象 |
| `registerStaticDirectory()` | 注册静态文件前缀和本地目录 |
| `registerWebSocket*Callback()` | 注册 WebSocket 连接、消息和关闭回调 |
| `enqueueRunnable()` | 从 Application 继承的线程池入口 |
一个路径可以注册多个 HTTP 方法。重复注册同一路径和方法时，注册函数返回失败，应检查返回值并记录启动日志。

## Request 与 Response

HTTP Handler 固定接收 `HttpRequest&` 和 `HttpResponse&`：

```cpp
app.registerHttpHandler<crazy::GET, crazy::POST>(
    "/api/search",
    [](crazy::HttpRequest& request,
       crazy::HttpResponse& response) {
        const auto query =
            request.uri().getQueryParam("q");
        const auto contentType =
            request.headers().get("Content-Type");

        response.setStatus(crazy::HttpStatus::OK);
        response.setHeader("Content-Type", "application/json");
        response.setBody(
            "{\"q\":\"" + query + "\",\"body_size\":" +
            std::to_string(request.body().size()) + "}");
    });
```

`HttpRequest` 提供方法、URI、Header 和 Body。`HttpResponse` 提供状态码、原因短语、Header、Body 和发送控制。Header 容器对键名大小写不敏感，并允许重复字段。

## 延迟响应状态

默认情况下 Handler 返回后框架自动发送响应。`defer()` 会把响应切换为延迟模式，返回同一个响应的 `shared_ptr`：

```cpp
auto deferred = response.defer();
// Handler 返回后不会自动发送。
```

之后必须调用一次 `deferred->send()`（分块响应改用 `beginChunked()` / `writeChunk()` / `endChunked()`），否则客户端会一直等待。`isDeferred()` 和 `isSent()` 可用于诊断状态，但业务通常不需要检查。

## 静态文件安全规则

静态目录处理遵循以下规则：

1. 只接受 `GET`。
2. 精确业务路由优先于静态前缀。
3. URL 路径映射到注册根目录下。
4. 内容类型按扩展名设置。
5. 目录列表默认关闭。
6. 目录列表开启后，非尾斜杠目录重定向到尾斜杠。
7. 路径包含 `.`、`..` 或解析后越过根目录时拒绝。
8. 文件不存在返回 `404`。

不要在静态目录根下放置配置文件、密钥或数据库文件。注册目录应只包含允许公开访问的资源。

## 并发模型

HTTP 连接由 `HttpServer` 接收，随后分配到 Application 线程池中的线程。同步 Handler 在该线程执行；长任务应调用 `response.defer()` 并投递到线程池。不要让同步 Handler 执行数据库长查询、文件扫描或等待外部网络。

相关文档：[HTTP 客户端](http-client.md)、[WebSocket](websocket.md)、[Application](application.md)、[配置管理](config.md)。
