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

## HTTPS (TLS)

`https://` **really uses TLS**: TLS support is **on by default** at build time (`CRAZY_ENABLE_TLS` defaults
to `ON`, using the vendored `src/third_party/openssl`; pass `-DCRAZY_ENABLE_TLS=OFF` to turn it off), and
`HttpClient` opens a TLS connection for `https://`, verifies the server certificate, completes the handshake,
and only then sends the request. **When TLS is unavailable or verification fails, `request()` returns
`nullptr` and the request is never sent in plaintext** (previously `https://` merely changed the port to 443
and stayed plaintext, which was fake security).

> **Compiled-in by default does not mean enabled at run time.** The client really opens a TLS connection only
> for `https://` / `wss://` requests; `http://` / `ws://` requests stay plaintext.

Client parameters come from the `[HTTP]` / `[http]` section as well:

| Key | Type | Default | Meaning |
|------|------|---------|---------|
| `tls_ca_file` | string | empty | Trusted CA (PEM) path; empty uses the default trust store described below (OpenSSL default verify paths + the bundled CA bundle) |
| `tls_verify_peer` | bool | `true` | Verify chain and validity dates; **on by default on the client**, only an explicit `false` skips it |
| `tls_min_version` | string | `"1.2"` | Only `"1.2"` / `"1.3"` |

```ini
[HTTP]
tls_ca_file = /etc/crazy/ca.pem
tls_verify_peer = true
```

### Default trust store and how to override it (bundled CA bundle)

With an empty `tls_ca_file` the client can verify **real public HTTPS sites out of the box**: the framework
ships a well-known CA bundle (`src/third_party/ca/cacert.pem`, 121 certificates / 188900 bytes, taken from
`https://curl.se/ca/cacert.pem`, derived from the Mozilla CA extract; see
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) for origin and licensing) and **compiles it into the
library** (CMake reads the PEM at configure time, generates `src/crazy/net/ca_bundle.cpp` in the source tree,
and compiles that file like any other source), so at run time it needs
**no** external file, environment variable or relative path.

The trust store priority is fixed:

| Order | Condition | Trust store actually used |
|---|---|---|
| ① | `tls_ca_file` is not empty | **that file only**; the bundled CA bundle is **not** added (otherwise "trust only a private CA / a self-signed test CA" would not hold) |
| ② | `tls_ca_file` is empty but `SSL_CERT_FILE` or `SSL_CERT_DIR` is set | **only the explicitly selected trust store**; the bundled bundle is **not** added (explicit configuration wins and is not rescued by the bundle) |
| ③ | `tls_ca_file` is empty and neither variable is set | the OpenSSL default verify paths **plus** the bundled CA bundle |

Key points:

- **An explicit choice always wins.** Point `SSL_CERT_FILE` at a file that does **not** contain the target
  site's CA and `https://` **really fails** (`unable to get local issuer certificate`) instead of being
  silently rescued by the bundled bundle.
- The bundled bundle is used for **client verification only**; it has nothing to do with the server side
  `tls_cert_file` / `tls_key_file`.
- If the PEM is missing at configure time the build **warns** (it does not fail hard) and clients fall back
  to the OpenSSL default verify paths only, in which case public sites fail the handshake because the trust
  store is empty.
- To update it, replace `src/third_party/ca/cacert.pem` and re-run configure (this regenerates
  `src/crazy/net/ca_bundle.cpp` and recompiles it into the library), or point
  `-DCRAZY_CA_BUNDLE_FILE=<path>` at another PEM.
- Diagnostics: `crazy::bundledCaCertificateCount()` / `crazy::bundledCaSource()`
  (see `crazy/net/ca_bundle.h`).

For finer control (custom CA, capped protocol version) use `TlsContext` + `TlsSocket` directly:

```cpp
std::string error;
crazy::TlsOptions options;
options.caFile = "/etc/crazy/ca.pem";
options.verifyPeer = true;
auto context = crazy::TlsContext::createClient(options, error);
if (!context) {
    CRAZY_SYSTEM_ERROR() << error;   // e.g. load tls_ca_file failed, file = ...
}

auto socket = std::make_shared<crazy::TlsSocket>(
    context, crazy::TlsSocket::Side::client);
socket->setVerifyHost("example.com");   // verify the host name / IP inside the certificate
if (!socket->connect("example.com", 443) || !socket->handshake(error)) {
    CRAZY_SYSTEM_ERROR() << error;      // e.g. certificate verify failed: certificate has expired
}
```

Every failure shows up as `request()` returning `nullptr` plus a log line: untrusted chain, **expired
certificate**, host name mismatch, `tls_ca_file` pointing at a missing path (it fails during initialisation
instead of skipping verification), or a build that turned TLS off with `-DCRAZY_ENABLE_TLS=OFF` (in which case
no TCP connection is
even attempted).

The WebSocket client behaves the same way: `wss://` uses the same TLS stack and the same section, and
`connect()` returns `false` on failure — see [WebSocket](websocket.md).

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
| `request(..., dataCallback)` | Yes | Generic entry with an optional body data callback |
| `postForm()` | Yes | multipart/form-data |

Callbacks run on the calling thread; this class does not create background work.

## Chunked upload (Transfer-Encoding: chunked)

Request bodies are sent with `Content-Length` by default. When the server should consume data incrementally, or the length is not known up front, switch to a chunked request body:

```cpp
crazy::HttpClient client;
client.setChunkedUpload(true, 8192);   // chunked, at most 8192 bytes per chunk

auto response = client.request(crazy::HttpMethod::POST,
                               "http://127.0.0.1:8080/upload",
                               payload, nullptr);
```

- the request carries `Transfer-Encoding: chunked` and **no** `Content-Length`; the body is split into chunks of `chunkSize` bytes and terminated by `0\r\n\r\n`;
- only the request body is affected; body-less methods such as `get()` / `head()` behave exactly as before;
- with chunked upload enabled, a `Content-Length` in the default headers is rejected: `request()` returns `nullptr`, `requestAsync()` returns `false`, and **no byte is written to the connection** (never a truncated, malformed request);
- when you build an `HttpRequest` yourself, call `setChunkedTransferEncoding(true)` and `setChunkSize()`; if `Content-Length` is also set, `toString()` throws `std::logic_error` instead of silently dropping the header;
- chunked transfer coding was introduced by HTTP/1.1: serializing a request whose version is `HTTP/1.0` throws `std::logic_error` as well;
- `chunkSize` is only the framing granularity; it does not change the payload the server receives.

On the server side (`crazy::HttpSession` / `HttpRouter`) inbound chunked bodies are de-chunked before the handler runs, so `request.body()` is the complete payload and request trailers land in `request.trailers()`. See the [HTTP server](http-server.md) documentation.

## Receiving a chunked response incrementally

The last parameter of `request()` is a response body data callback. It fires as body data arrives, without waiting for the end of the message:

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
        streamed += data;  // delivered on arrival
    });
```

- The data callback runs on the calling thread, in the order data arrives.
- Delivery is "at least once, never later than the next data arrival". TCP segmentation can split one chunk into several deliveries, so do not assume one callback per HTTP chunk.
- `response->body()` still holds all chunks concatenated in order once the response completes; the return value and response callback semantics are unchanged.
- Non-chunked (`Content-Length`) responses are delivered incrementally as well; omit the parameter when you do not need it.

## Transfer-Encoding versus Content-Length

When a response contains **both** `Transfer-Encoding: chunked` and `Content-Length`, it is treated as a parse error: `HttpResponseParser::hasError()` is `true`, `isFinished()` is `false`, `request()` returns `nullptr`, the response callback is not invoked, and no exception is thrown. Such input is never accepted as a lenient form.

An invalid chunk size field (for example `Z\r\n`), a missing terminating chunk `0\r\n\r\n`, or a connection closed in the middle of chunk data all fail the same way: `request()` returns `nullptr` within bounded time, the callback is not invoked, and no exception is thrown. The current implementation does not expose the failure reason (only success and failure); distinguishing failure causes requires a separate requirement.

## Trailers and interim 1xx responses

- Chunked response trailers are parsed after the terminating chunk and stored in `response->trailers()`; they never leak into `response->headers()`.
- Interim responses such as `100 Continue` (1xx except 101) are skipped automatically: `HttpResponseParser` keeps parsing towards the final response, `interimResponseCount()` reports how many were skipped, and `isFinished()` only becomes `true` for the final response. Sending `Expect: 100-continue` by hand therefore never returns the `100 Continue` message as the result.

## Request flow and connection reuse

The client parses the URI, establishes or reuses a connection, merges default and request headers, writes the request, incrementally parses the response (skipping interim 1xx responses and collecting trailers into `trailers()`), and preserves the socket when keep-alive is allowed.

An `HttpClient` owns one socket and receive buffer. Do not share one client concurrently across threads. Give each thread its own client or protect the call site.

## Event driven (register into a selector / actor)

`requestAsync()` does not wait for the response: once the connection (including the `https` TLS handshake) is
up it writes the request and returns, then the selector driven `onReadable()` parses the response
incrementally and fires the callback on the selector thread. Write events are registered by the client
whenever the request body is still queued.

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
                // runs on the actor thread; response is nullptr on connect/parse failure
            });
    }

private:
    crazy::HttpClient client_;
};
```

- One in-flight request per client; check `isBusy()` and send the next one from the response callback, which reuses the keep-alive connection.
- Without an actor, bind the same two callbacks to `crazy::Selector::registerEvent` / `unregisterEvent` (see `tests/test_http_client_async.cc`).
- Timeouts and retries are a `Selector::registerTimer()` away.

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
