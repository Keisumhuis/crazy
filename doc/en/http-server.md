# HTTP Server

[Back to documentation index](../README.en.md)

`crazy::HttpApplication` extends `Application` with an HTTP/1.1 router, deferred responses, static file serving, and WebSocket support.

## HTTPS (TLS)

TLS lives at the **Socket layer** (`crazy::TlsSocket`), so the HTTP layer needs no TLS-specific changes:
once enabled, the listening port accepts TLS only, and the decrypted byte stream is handed to the very same
`HttpSession`. **WebSocket over TLS (WSS) on the server side therefore needs no extra code.**

Enabling it takes two steps: make TLS available at build time (**on by default**, so the compiled-in
capability is already there), then give the certificate and the switch explicitly in configuration.

```bash
# CRAZY_ENABLE_TLS defaults to ON: the default build already has TLS support (it statically links the
# vendored src/third_party/openssl), so no -D flag is needed; the explicit form below is equivalent.
cmake -S . -B build/tls -DCRAZY_ENABLE_TLS=ON

# Explicitly turn TLS support off (then https:// / wss:// fail explicitly instead of silently degrading
# to plaintext):
cmake -S . -B build/notls -DCRAZY_ENABLE_TLS=OFF
```

> **Compiled-in by default does not mean enabled at run time.** The listening port only really speaks
> HTTPS once `tls_enable = true` and readable certificate/key paths are configured in the `[HTTP]` section
> as shown below; when that section is missing or `tls_enable = false`, the port still serves plaintext HTTP.

```ini
[HTTP]
port = 8443
address = 0.0.0.0
tls_enable = true
tls_cert_file = /etc/crazy/server_cert.pem
tls_key_file = /etc/crazy/server_key.pem
tls_min_version = 1.2
```

| Key | Type | Default | Meaning |
|------|------|---------|---------|
| `tls_enable` | bool | `false` | `false` or missing = plaintext, exactly as before |
| `tls_cert_file` | string | empty | Certificate chain, PEM path (required on the server) |
| `tls_key_file` | string | empty | Private key, PEM path (required on the server) |
| `tls_min_version` | string | `"1.2"` | Only `"1.2"` / `"1.3"`; anything else is a configuration error: it logs a fallback alert and keeps serving plaintext (see below) |
| `tls_ca_file` | string | empty | Client role only, see [HTTP client](http-client.md) |
| `tls_verify_peer` | bool | `true` on the client, `false` on the server | Whether to verify the peer certificate |

Only **PEM** certificates and keys are supported; PFX / PKCS#12 is out of scope.

TLS keys follow the same **case-insensitive section fallback** as the other HTTP keys (`[HTTP]` first, then
`[http]`), and reading them is **independent of `app.listen()`**: `tls_*` takes effect whether the port comes
from `app.listen(port, address)` or from the configuration section, so a caller that listens explicitly can
never end up silently serving plaintext.

Programmatic use can pass a `TlsContext` directly:

```cpp
crazy::TlsOptions options;
options.certFile = "server_cert.pem";
options.keyFile = "server_key.pem";
std::string error;
auto context = crazy::TlsContext::createServer(options, error);

crazy::HttpServer server;
server.listen(8443, "0.0.0.0", context);
```

### TLS initialization failures: fall back to plaintext with a prominent alert

**Availability first** (Lead decision, 2026-10-10: "it must run even without a certificate"): when
`tls_enable = true` but TLS initialization fails, `startServer()` **does not refuse to start and does not skip
listening**. It **falls back to plaintext on the same port** and prints a **prominent alert** (`[TLS ALERT]`,
error level, to both the console and the log file) that states that TLS was requested but the service is now
served as **plaintext**, together with the **exact file path** and the underlying error. Plaintext clients get
normal HTTP responses afterwards, and the failure reason is queryable through
`HttpApplication::tlsInitError()` (empty means no failure).

Triggers include:

- `tls_cert_file` / `tls_key_file` is empty, missing or unreadable;
- the private key does not match the certificate;
- `tls_min_version` is invalid (for example `"1.1"`);
- the build has `CRAZY_ENABLE_TLS=OFF` while `tls_enable` is `true`.

> **Security statement (must read)**: this is an **availability-first** trade-off. Deployments that rely on
> HTTPS protection **must monitor the startup alert** — the framework **does not guarantee** that
> "TLS configured" means "running TLS". A deployment that needs "refuse to start when the certificate is
> missing" should read `HttpApplication::tlsInitError()` (or the `[TLS ALERT]` log line) from its startup
> script or health check and terminate the process itself.

A connection whose handshake fails is closed immediately: the **HTTP layer never sees a single byte**
(plaintext requests sent to an HTTPS port included), and the server keeps serving later valid TLS connections.

### Blocking handshake and the handshake timeout

Business sockets are **blocking** in this framework (existing behaviour, not changed here), and `SSL_accept`
on a blocking descriptor waits for the whole handshake. This revision therefore puts a **bounded timeout** on
the handshake: `SO_RCVTIMEO` / `SO_SNDTIMEO` (5 seconds by default) are set on the underlying socket for the
duration of the handshake and restored to an infinite wait as soon as it completes or fails. A client that
connects without sending, or sends half a ClientHello, can occupy that long at most before the connection is
closed. Removing this DoS surface entirely requires non-blocking business sockets, which is **a separate
requirement**.

### Clean shutdown

The normal `HttpSession` close path sends the TLS `close_notify` before closing the underlying connection, and
it drains already-received kernel data in a bounded way first, so `closesocket` cannot turn a clean shutdown
into an RST that the peer would see as a truncated connection. A peer `close_notify` makes
`TlsSocket::recv` return `0`, while truncation returns a negative value — the two are distinguishable.

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

Calling `send()` more than once has no additional effect (except for chunked responses, see below). It is safe to call from a worker thread; the actual socket write returns to the owning session thread.

## Chunked responses

When the body length is unknown or data is produced incrementally, use `beginChunked()` / `writeChunk()` / `endChunked()`. All three actions are driven by the `shared_ptr` returned from `defer()` and can be called from another thread after the handler returns:

```cpp
app.registerHttpHandler<crazy::GET>(
    "/stream",
    [&app](crazy::HttpRequest&, crazy::HttpResponse& response) {
        auto deferred = response.defer();
        deferred->beginChunked();          // status line and headers
        deferred->writeChunk("Hello");     // <hex length>\r\n<data>\r\n
        app.enqueueRunnable([deferred] {
            deferred->writeChunk(" ");
            deferred->writeChunk("World");
            deferred->endChunked();        // terminating chunk 0\r\n\r\n
        });
    });
```

Semantics of a chunked response:

- `beginChunked()` sends only the status line and headers; the header block contains `Transfer-Encoding: chunked` and **no** `Content-Length`. Each data segment is framed per RFC 7230 section 4.1.
- `writeChunk()` may be called any number of times; each call appends one chunk (empty segments are ignored).
- `endChunked()` sends the terminating chunk `0\r\n\r\n`; without it the client waits forever.
- `isChunked()` and `isChunkedFinished()` report the current state.

If the application explicitly set `Content-Length` in chunked mode, `beginChunked()` **throws `std::logic_error`** (an exception thrown inside a handler is turned into a `500` response); the framework never silently removes that header. Bytes produced by the framework in chunked mode never contain both `Transfer-Encoding: chunked` and `Content-Length`.

### Streaming from a data source

When the payload does not fit comfortably in memory (large files, export streams), pass a `ChunkSource` to `beginChunked()` and let the framework pull data on demand:

```cpp
app.registerHttpHandler<crazy::GET>(
    "/big",
    [](crazy::HttpRequest&, crazy::HttpResponse& response) {
        auto file = std::make_shared<std::ifstream>("big.bin", std::ios::binary);
        auto remaining = std::make_shared<std::uint64_t>(fileSize);
        response.beginChunked([file, remaining](std::string& buffer) -> std::size_t {
            if (*remaining == 0) {
                return 0;                 // 0 means end of data
            }
            // append one slice to buffer and return its size
            ...
        });
    });
```

The source runs on the session thread and is only called again **after the kernel write buffer drains**, so memory is bounded by a single chunk (the session high-water mark is 64 KiB) instead of the whole payload. Returning 0 finishes the response; throwing aborts the connection (the client sees a truncated response, never a half payload it trusts). A data source is mutually exclusive with `writeChunk()`; do not mix them.

### Trailers

A chunked response may send fields after the header block:

```cpp
response.addTrailer("X-Checksum", "abc123");   // must precede beginChunked
response.beginChunked();
response.writeChunk("data");
response.endChunked();
```

- `addTrailer()` must be called before `beginChunked()`; afterwards it throws `std::logic_error`;
- the header block declares `Trailer: x-checksum` automatically (not duplicated when the application already set a `Trailer` header);
- the terminating sequence is `0\r\n<trailer fields>\r\n`; clients read them from `response.trailers()`, never from `headers()`.

### HTTP/1.0 fallback

HTTP/1.0 has no chunked coding. When the request version is below 1.1 the session degrades automatically:

- no `Transfer-Encoding: chunked` and no `Content-Length` in the response head;
- `Connection: close` is forced and the body is delimited by **closing the connection** (RFC 7230 section 3.3.3);
- data sources work the same way: an HTTP/1.0 client receives the same bytes, just without chunk framing.

### Expect: 100-continue

When a request carries `Expect: 100-continue` (HTTP/1.1 and later), the framework replies `HTTP/1.1 100 Continue\r\n\r\n` before the body arrives and then keeps reading; without that interim response the client waits for the go-ahead while the server waits for the body. Works for both `Content-Length` and `Transfer-Encoding: chunked` request bodies.

### Connection lifecycle

- The connection may only be reused according to the request keep-alive rules after the terminating chunk `0\r\n\r\n` was written normally. Before that, the connection does not process further requests.
- Abnormal termination (client disconnect, write failure) closes the connection; it is never reused.

Do not produce a chunked response by inserting a `Transfer-Encoding: chunked` header by hand: `toString()` still appends `body()` verbatim, which yields a malformed message that claims chunked encoding without being chunked. The server does decode inbound chunked request bodies (`request.body()` holds the de-chunked payload and request trailers land in `request.trailers()`); client-side chunked upload is covered in the [HTTP client](http-client.md) documentation.

## Static files

```cpp
// Directory listings disabled.
app.registerStaticDirectory("/static", "./public");

// Directory listings enabled.
app.registerStaticDirectory("/files", "./public", true);
```

Static file routes handle `GET` only. Registered API routes take priority. Parent path segments and symlinks that escape the configured root are rejected.

### Chunked by default

Static resources are **streamed in chunks by default**: the whole file is never read into memory and no `Content-Length` is produced.

- each data chunk is at most `HttpRouter::kStaticStreamChunkSize` (64 KiB) and is pulled only when the socket is writable;
- responses carry `Accept-Ranges: bytes` and an `ETag`;
- HTTP/1.0 clients fall back to "body delimited by closing the connection" (see the HTTP/1.0 fallback note above);
- an empty file has nothing to stream and returns `200` with `Content-Length: 0`.

### Range requests

A `Range` header yields a partial response (RFC 7233), which is what resumable downloads and random access into large files use:

```bash
curl -r 0-1023  http://127.0.0.1:8080/static/big.bin   # 206 + Content-Range
curl -r 1024-   http://127.0.0.1:8080/static/big.bin   # 1024 to the end
curl -r -1024   http://127.0.0.1:8080/static/big.bin   # last 1024 bytes
```

- only **single ranges** are supported; multi-range (`bytes=0-1,4-5`) and malformed syntax are handled by ignoring `Range` and returning `200` with the full body;
- a satisfiable range returns `206 Partial Content` + `Content-Range: bytes <first>-<last>/<size>`; the body is still streamed in chunks;
- unsatisfiable ranges (`first >= size`, `first > last`, zero-length suffix, empty file) return `416 Range Not Satisfiable` + `Content-Range: bytes */<size>`;
- `If-Range` supports the ETag form: the range is honoured only when the value matches the response `ETag`, otherwise the full `200` body is returned. The `Last-Modified` form is not supported (static responses do not emit `Last-Modified`, so such values never match);
- resumed download: fetch the `ETag` once, then send `Range: bytes=<downloaded>-` together with `If-Range: <ETag>` on every retry.

## HttpApplication API

| API | Purpose |
|-----|---------|
| `listen()` | Set address and port |
| `listen(port, address, tlsContext)` | Same, and makes that port TLS-only (an empty `TlsContext` means plaintext) |
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

`response.defer()` prevents automatic send when the handler returns and returns a shared response pointer. Call `send()` exactly once from the asynchronous task (for chunked responses use `beginChunked()` / `writeChunk()` / `endChunked()` instead). A missing call leaves the client waiting; repeated calls are ignored.

## Static file security

Static routes handle `GET`, exact API routes take priority, path mapping stays under the configured root, and content types are derived from extensions. Directory listings are disabled by default. Path traversal attempts return `403`; missing files return `404`.

Do not expose configuration, keys, or database files through a static directory.

## Concurrency

HTTP sessions are assigned to the Application worker pool. Synchronous handlers run on that worker. For long work, defer the response, submit work, and send from the task after it completes.

Related: [HTTP client](http-client.md), [WebSocket](websocket.md), [Application](application.md), [Configuration](config.md).
