# Testing

[Back to documentation index](../README.en.md)

The project uses CTest. Unit, end-to-end, and database tests can be enabled independently.

## Run all tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
```

## Run one test

```bash
cmake --build build --target test_http_client
./build/test_http_client
```

With Visual Studio multi-configuration generators, targets are usually under `build/Debug` or `build/Release`:

```powershell
.\build\Debug\test_http_client.exe
```

## Test groups

| Group | Targets |
|-------|---------|
| Core | `test_actor`, `test_config`, `test_logger`, `test_json`, `test_protocol` |
| Network services | `test_socket`, `test_localsocket`, `test_http`, `test_websocket`, `test_websocket_server` |
| Clients | `test_actor_client`, `test_http_client`, `test_websocket_client`, `test_smtp_client` |
| Databases | `test_mysql`, `test_clickhouse` |
| Utilities | `test_date_time`, `test_encryption`, `test_key_value_pair`, `test_lock`, `test_mmap`, `test_mvcc_lock_wrapper` |
| End-to-end | `test_http_client_e2e`, `test_http_keep_alive_e2e`, `test_http_chunked`, `test_http_chunked_upload`, `test_http_range`, `test_websocket_client_e2e` |
| TLS / HTTPS | `test_https_tls_server`, `test_https_tls_client`, `test_https_tls_config`, `test_https_client` (TLS is **on by default** at build time, so the default configuration builds all of them; `test_https_client` is also built with `-DCRAZY_ENABLE_TLS=OFF`, to assert that `https://` fails explicitly) |
| Public network (**off by default**) | `test_https_baidu` (**not built by default**; requires `-DCRAZY_BUILD_NETWORK_TESTS=ON`; reaches the real public site `https://www.baidu.com` and echoes the response, using the repository's own OpenSSL client) |

Database tests require a running MySQL or ClickHouse service. Network tests use local ports and settings from `tests/app.ini`.

### Public-network case `test_https_baidu` (switch `CRAZY_BUILD_NETWORK_TESTS`, currently on by default)

`test_https_baidu` is a **real public-network HTTPS end-to-end case**: it uses the repository's own
`TlsContext`/`TlsSocket` + `HttpClient` (**not** PowerShell/.NET) to reach `https://www.baidu.com/`,
prints the status line, key response headers, the first 4 KB of the body and the total body size, and
writes the **complete body** to `baidu-response.html` under the build-tree root (the path is logged).

It **needs public internet** (DNS + TCP 443 + a real CA chain), hence the `CRAZY_BUILD_NETWORK_TESTS`
switch (**currently `ON` by default**): pass `-DCRAZY_BUILD_NETWORK_TESTS=OFF` on a clean or offline machine
and it is neither built nor listed by `ctest -N`. A genuinely offline run is judged **Skipped** (exit code 77)
through the DNS/TCP failure path, so it never turns a machine red.

```powershell
cmake -S . -B build/net-tests -G "Visual Studio 18 2026" -A x64 `
  -DCRAZY_BUILD_NETWORK_TESTS=ON
cmake --build build/net-tests --config Release --target test_https_baidu
ctest --test-dir build/net-tests -C Release --output-on-failure -R test_https_baidu
```

Verdict rules:

- **Offline / DNS failure / overall timeout** (the whole case is bounded, 9 s budget) → prints a `SKIP:`
  line with the concrete reason and exits with code 77, which CTest records as **Skipped**
  (`SKIP_RETURN_CODE`) — **not** Failed.
- **This build has no TLS client compiled in** (`-DCRAZY_ENABLE_TLS=OFF`) → also prints a `SKIP:` line
  with the reason and exits with 77 (a missing TLS client is a build configuration, not a defect of the
  client under test).
- **TLS handshake or certificate verification failure** → **Failed**, printing the raw OpenSSL error
  string verbatim. The case **never** disables verification (no `SSL_VERIFY_NONE`, no skipped hostname check).
- Certificate verification **works by default and needs no environment variable**: the client has the
  bundled CA bundle (`src/third_party/ca/cacert.pem`, 121 certificates) compiled into the library — which is
  exactly what is needed because on Windows OpenSSL does **not** read the Windows certificate store. See
  "Default trust store and how to override it" in [http-client.md](http-client.md).
  If `SSL_CERT_FILE` / `SSL_CERT_DIR` is set explicitly the case uses **only** that trust store and does not
  add the bundled bundle: pointing it at a file without the target CA makes the case **really fail** instead
  of falling back to "no verification".

## CMake switches

```cmake
option(CRAZY_BUILD_TESTS "Build crazy test executables" ON)
option(CRAZY_BUILD_E2E_TESTS "Build network e2e tests" ON)
option(CRAZY_BUILD_DB_TESTS "Build database tests" ON)
option(CRAZY_ENABLE_TLS "Enable TLS/HTTPS transport (requires OpenSSL)" ON)
option(CRAZY_BUILD_NETWORK_TESTS "Build tests that require real public network access" ON)
```

TLS is **ON by default** (the default build already includes TLS support, using the vendored
`src/third_party/openssl`), so no `-D` flag is needed. To turn it off explicitly:

```bash
cmake -S . -B build/notls -G "Visual Studio 18 2026" -A x64 \
  -DCRAZY_ENABLE_TLS=OFF
```

(Compiled-in by default does not mean enabled at run time: HTTPS still needs `tls_enable=true` plus
certificate paths in the `[HTTP]` section — see the HTTPS section of [http-server.md](http-server.md).)

If `CRAZY_OPENSSL_ROOT` lacks the directory, `include/openssl/ssl.h`, or the matching `ssl` / `crypto`
libraries, configure **fails hard and names the missing file** instead of silently disabling TLS.

The public-network case `test_https_baidu` follows `CRAZY_BUILD_NETWORK_TESTS` (**currently `ON` by
default**): it is registered inside the `CRAZY_BUILD_E2E_TESTS` block, so E2E tests must also be `ON` (the
default). On a clean or offline machine pass `-DCRAZY_BUILD_NETWORK_TESTS=OFF` to keep it out of the build
and out of `ctest -N`; a genuinely offline run is marked Skipped (exit code 77) rather than Failed.

```bash
cmake -S . -B build \
  -DCRAZY_BUILD_DB_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
```

CMake copies `tests/app.ini` and `tests/http_static` into the build tree. Network and end-to-end tests use these resources and may bind local ports.

## Select tests

```bash
ctest --test-dir build -R test_actor --output-on-failure
ctest --test-dir build -R "test_http|test_websocket" --output-on-failure
```

Direct execution gives detailed console output:

```bash
./build/test_http_client
```

## Add a test

```cmake
add_crazy_test(test_your_feature)
```

Create `tests/test_your_feature.cc`. The helper links the library, threads, and available database libraries. Add environment-dependent tests under the E2E or DB options.

## Coverage priorities

| Module | Important cases |
|--------|-----------------|
| Buffer/Decoder | Split frames, coalesced frames, oversized and invalid lengths |
| Actor | Routing, command responses, async work, shutdown |
| HTTP | Route priority, deferred send, static path safety, chunked responses and requests, Range slices, HTTP/1.0 fallback |
| Databases | Connect failure, queries, rollback, pool exhaustion |
| Locks | Contention, duplicate release, timeout behavior |
| Serialization | Empty values, nested containers, compatibility, malformed input |
| WebSocket | Handshake, fragmentation, control frames, disconnect |

Related: [Feature overview](overview.md), [Build](build.md), [HTTP server](http-server.md).
