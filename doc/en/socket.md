# TCP Socket

[Back to documentation index](../README.en.md)

`crazy::Socket` wraps a TCP socket that **defaults to IPv4 only** and can explicitly listen on or connect to IPv6 and dual-stack endpoints. It suits synchronous clients, simple servers, and custom event loops. This page covers TCP only; local IPC is documented in [LocalSocket](local-socket.md).

## API

| API | Result | Purpose |
|-----|--------|---------|
| `listen(port, address, dualStack)` | `bool` | Bind and listen; `address` defaults to `"0.0.0.0"`, `dualStack` defaults to `false` (IPv4-only / IPv6-only) |
| `connect(address, port)` | `bool` | Establish TCP; accepts IPv4 and IPv6 literals |
| `accept()` | `Socket::ptr` | Accept a client |
| `send(data, length, flags)` | `int32_t` | Write bytes, possibly partial |
| `recv(data, length, flags)` | `int32_t` | Read bytes, 0 means EOF |
| `active()` | `bool` | Local descriptor is valid |
| `close()` | `void` | Release descriptor |
| `getOption()` / `setOption()` | `bool` | Socket options |
| `remoteAddress()` / `localAddress()` | address strings | Endpoint details |

## Server and client

```cpp
#include "crazy/net/socket.h"

int main() {
    crazy::Socket server;
    if (!server.listen(8080, "0.0.0.0")) {
        return 1;
    }

    auto client = server.accept();
    char buffer[1024] = {};
    const int32_t received =
        client->recv(buffer, sizeof(buffer));
    if (received > 0) {
        client->send(buffer, static_cast<size_t>(received));
    }
}
```

```cpp
crazy::Socket socket;
if (socket.connect("127.0.0.1", 8080)) {
    const std::string request = "ping";
    socket.send(request.data(), request.size());
}
```

## IPv6 and dual-stack

`Socket` is **IPv4-only by default**, exactly as before; IPv6 and dual-stack must both be requested explicitly. The address family is derived from the address string:

| `listen()` `address` | Family | Behaviour |
|---|---|---|
| omitted, `"0.0.0.0"`, `"127.0.0.1"` | `AF_INET` | IPv4 only (default) |
| `"::1"`, `"::"`, `"[::1]"` with `dualStack = false` | `AF_INET6` | IPv6 only (no implicit dual stack) |
| `"::"` with `dualStack = true` | `AF_INET6` | Dual-stack: one port accepts both IPv4 and IPv6 |

```cpp
// IPv6 loopback only
crazy::Socket onlyV6;
onlyV6.listen(8080, "::1");

// Dual-stack: one port serves both 127.0.0.1 and ::1
crazy::Socket both;
both.listen(8080, "::", true);
```

`connect(address, port)` accepts IPv6 literals (`"::1"`, `"[::1]"`) as well. **Non-literal host names still yield IPv4 candidates only**: the address family of `connect("localhost")` is unchanged by this revision, and IPv6 name resolution is left to a separate requirement.

**Bind addresses must be standard literals**: the `address` argument of `listen(port, address)` has to be a standard dotted-decimal IPv4 literal (four decimal octets, e.g. `"127.0.0.1"`) or a standard IPv6 literal (e.g. `"::1"`, `"[::1]"`). **Non-standard spellings fail and return `false`**, including abbreviations such as `"127.1"`, octal or hexadecimal forms such as `"0177.0.0.1"` and `"0x7f.0.0.1"`, and host names such as `"localhost"`. `inet_addr` used to accept these incidentally; they are no longer accepted, so this is a **tightening** of the contract rather than a relaxation — spell bind addresses as standard literals when migrating.

Address string format:

| Case | `localAddress()` / `remoteAddress()` |
|---|---|
| IPv4 | `tcp://127.0.0.1:45678` |
| IPv6 | `tcp://[::1]:45678` (bracketed, port still last) |
| IPv4 peer on a dual-stack listener | `tcp://127.0.0.1:45678` (the IPv4-mapped peer is normalised; `::ffff:` never appears) |

### IPV6_V6ONLY and dual-stack semantics

A dual-stack listener cannot rely on platform defaults: `IPV6_V6ONLY` must be set explicitly **before `bind()`**.

- On Windows (Vista and later) a new `AF_INET6` socket defaults to `IPV6_V6ONLY = 1` (IPv6 only); without clearing it, IPv4 clients get `ConnectionRefused`.
- On Linux the default comes from `/proc/sys/net/ipv6/bindv6only`, which defaults to `0` (dual-stack); an operator setting it to `1` makes "do nothing" drift with the environment.
- Changing the option after `bind()` has no effect, hence it must be set beforehand.

`Socket::listen()` therefore **always sets the option explicitly** for IPv6 addresses: `dualStack = false` sets `1` (IPv6 only) and `dualStack = true` sets `0` (dual-stack). **If the option cannot be set, `listen()` returns `false`** instead of logging a warning and continuing with whatever the platform happens to default to.

## TLS (`TlsSocket`)

TLS lives at this layer as a `Socket` subclass: `crazy::TlsSocket : public Socket` overrides
`send` / `recv` / `close` / `pending` / `wantRead` / `wantWrite`, so layers such as `Session`,
`Connection`, and `HttpSession` keep using the plain `Socket` interface and stay unaware of TLS.
`crazy::TlsContext` loads PEM certificates and keys and holds the verification policy.

TLS support is **on by default** at build time (`CRAZY_ENABLE_TLS` defaults to `ON`, using the vendored
`src/third_party/openssl`) and can be turned off explicitly with `-DCRAZY_ENABLE_TLS=OFF`; HTTPS is only
actually enabled at **run time**, by configuring `tls_enable = true` and a certificate in the `[HTTP]`
section (see the HTTPS section of [HTTP server](http-server.md)).
HTTPS/WSS configuration and usage are documented in [HTTP server](http-server.md) and
[HTTP client](http-client.md).

```cpp
#include "crazy/net/tls.h"

std::string error;
crazy::TlsOptions options;
options.certFile = "server_cert.pem";
options.keyFile = "server_key.pem";
options.minVersion = "1.2";
auto context = crazy::TlsContext::createServer(options, error);
if (!context) {
    CRAZY_SYSTEM_ERROR() << error;   // names the exact file; the caller decides what happens next (HttpApplication falls back to plaintext with an alert, see http-server.md)
}

// A listening socket produces TlsSocket from accept() via the Socket::createAcceptedSocket hook
crazy::TlsServerSocket listener(context);
listener.listen(8443, "0.0.0.0");
auto tls = std::dynamic_pointer_cast<crazy::TlsSocket>(listener.accept());
if (tls && tls->handshake(error)) {
    // tls->negotiatedVersion() / negotiatedCipher() expose the negotiation result
}
```

Contract highlights:

| Behaviour | Semantics |
|-----------|-----------|
| `TlsSocket::recv()` | `> 0` plaintext bytes; `0` peer `close_notify` (clean shutdown); `< 0` error, including truncation |
| `TlsSocket::send()` | **all-or-nothing**: a partial `SSL_write` is never exposed as a byte-offset contract |
| `close()` | sends `close_notify` first; drains unread data in a bounded way so an RST cannot make the peer see a truncated connection |
| `wantRead()` / `wantWrite()` | TLS `WANT_READ` / `WANT_WRITE`; `recv` / `send` also report them as `errno == EAGAIN` so callers treat them as "retry later" |
| `pending()` | bytes already decrypted by the TLS layer but not yet read, used to drain the TLS buffer (always 0 for plaintext) |

`handshake()` has a **5 second timeout** by default (`SO_RCVTIMEO` / `SO_SNDTIMEO` during the handshake,
restored right after), which bounds the blocking-handshake window for clients that connect without sending.
Business sockets remain blocking; converting them to non-blocking I/O is a separate requirement.

## Handle partial writes

`send()` can write fewer bytes than requested:

```cpp
bool sendAll(crazy::Socket& socket, const std::string& data) {
    const char* current = data.data();
    size_t remaining = data.size();

    while (remaining > 0) {
        const int32_t sent =
            socket.send(current, remaining);
        if (sent <= 0) {
            return false;
        }
        current += sent;
        remaining -= static_cast<size_t>(sent);
    }
    return true;
}
```

`Session` and `Connection` manage output buffering for framed protocols.

## Blocking and event-driven use

The direct examples are useful for tools and tests. Production servers should register read and write callbacks with `Selector`:

```cpp
selector.registerEvent(
    client->socket(),
    crazy::SelectorEventType::read,
    [client] { handleRead(client); });
```

Blocking `accept()` or `recv()` must not occupy a production Actor thread.

## Lifecycle and options

Create, listen/connect, exchange data, detect closure from return values or heartbeat policy, and close. `active()` only says that the local descriptor is valid.

```cpp
int enabled = 1;
socket.setOption(SOL_SOCKET, SO_REUSEADDR, enabled);
```

Prefer portable option wrappers over platform-specific flags in business code.

Related: [LocalSocket](local-socket.md), [Selector and timers](selector.md), [Encoder](encoder.md), [Decoder](decoder.md), [Session](session.md).
