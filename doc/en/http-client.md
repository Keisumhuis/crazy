# HTTP Client

[Back to documentation index](../README.en.md)

`crazy::HttpClient` is a synchronous HTTP/1.1 client with common methods, default headers, connection reuse, and multipart forms.

## Basic requests

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

## Generic request

```cpp
auto response = client.request(
    crazy::HttpMethod::PATCH,
    "http://127.0.0.1:8080/users/7",
    R"({"enabled":true})",
    {});
```

## Multipart upload

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

Requests block until the response is complete. Avoid making network calls directly from latency-sensitive Actor handlers.

## Method matrix

| API | Body | Purpose |
|-----|------|---------|
| `get()` | No | GET |
| `post()` | Yes | POST |
| `put()` | Yes | PUT |
| `del()` | No | DELETE |
| `head()` | No | HEAD |
| `options()` | No | OPTIONS |
| `patch()` | Yes | PATCH |
| `request()` | Yes | Generic entry |
| `postForm()` | Yes | multipart/form-data |

Callbacks run on the calling thread; this class does not create background work.

## Request flow and connection reuse

The client parses the URI, establishes or reuses a connection, merges default and request headers, writes the request, incrementally parses the response, and preserves the socket when keep-alive is allowed.

An `HttpClient` owns one socket and receive buffer. Do not share one client concurrently across threads. Give each thread its own client or protect the call site.

## Error handling

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

Connection failure, malformed responses, and early EOF can produce a null result. Use socket options or an external timeout policy for strict deadlines.

## Complete multipart upload

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
        "http://127.0.0.1:8080/upload", fields, files,
        [](crazy::HttpResponse::ptr response) {
            CRAZY_SYSTEM_INFO()
                << static_cast<int>(response->status());
        });
}
```

Form file contents are held in memory and are not intended for very large uploads.

Related: [HTTP server](http-server.md), [URI](uri.md), [Buffer](buffer.md).
