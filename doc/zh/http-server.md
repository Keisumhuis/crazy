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

同一个响应重复调用 `send()` 只会发送一次。后台线程可以安全调用 `send()`，实际写入仍会回到会话线程。

## 静态文件

```cpp
// 不显示目录列表
app.registerStaticDirectory("/static", "./public");

// 打开目录列表
app.registerStaticDirectory("/files", "./public", true);
```

静态目录只处理 `GET`。业务路由优先，路径穿越、`..` 路径段以及解析后越出根目录的符号链接会返回 `403`。

## HttpApplication 接口

| 接口 | 说明 |
|------|------|
| `listen(port, address)` | 设置 HTTP 监听端口和地址 |
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

之后必须调用一次 `deferred->send()`，否则客户端会一直等待。`isDeferred()` 和 `isSent()` 可用于诊断状态，但业务通常不需要检查。

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
