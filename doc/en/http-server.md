# HTTP Server

[Back to documentation index](../README.en.md)

`crazy::HttpApplication` extends `Application` with an HTTP/1.1 router, deferred responses, static file serving, and WebSocket support.

## Register routes

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
            response.setBody("{\"method\":\"" +
                             request.methodString() + "\"}");
        });

    app.exec();
}
```

## Deferred response

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

Calling `send()` more than once has no additional effect. It is safe to call from a worker thread; the actual socket write returns to the owning session thread.

## Static files

```cpp
// Directory listings disabled.
app.registerStaticDirectory("/static", "./public");

// Directory listings enabled.
app.registerStaticDirectory("/files", "./public", true);
```

Static file routes handle `GET` only. Registered API routes take priority. Parent path segments and symlinks that escape the configured root are rejected.

## HttpApplication API

| API | Purpose |
|-----|---------|
| `listen()` | Set address and port |
| `registerHttpHandler<Methods...>()` | Register functions, lambdas, member functions, or shared objects |
| `registerStaticDirectory()` | Register a URL prefix and local directory |
| `registerWebSocket*Callback()` | Configure WebSocket callbacks |
| `enqueueRunnable()` | Submit work to the Application thread pool |

## Request and response API

```cpp
app.registerHttpHandler<crazy::GET, crazy::POST>(
    "/api/search",
    [](crazy::HttpRequest& request,
       crazy::HttpResponse& response) {
        const auto query = request.uri().getQueryParam("q");
        const auto contentType =
            request.headers().get("Content-Type");

        response.setStatus(crazy::HttpStatus::OK);
        response.setHeader("Content-Type", "application/json");
        response.setBody(
            "{\"q\":\"" + query + "\",\"body_size\":" +
            std::to_string(request.body().size()) + "}");
    });
```

Headers are case-insensitive and support repeated keys. Read the method, URI, headers, and body from `HttpRequest`; set status, headers, body, and send state on `HttpResponse`.

## Deferred response lifecycle

`response.defer()` prevents automatic send when the handler returns and returns a shared response pointer. Call `send()` exactly once from the asynchronous task. A missing call leaves the client waiting; repeated calls are ignored.

## Static file security

Static routes handle `GET`, exact API routes take priority, path mapping stays under the configured root, and content types are derived from extensions. Directory listings are disabled by default. Path traversal attempts return `403`; missing files return `404`.

Do not expose configuration, keys, or database files through a static directory.

## Concurrency

HTTP sessions are assigned to the Application worker pool. Synchronous handlers run on that worker. For long work, defer the response, submit work, and send from the task after it completes.

Related: [HTTP client](http-client.md), [WebSocket](websocket.md), [Application](application.md), [Configuration](config.md).
