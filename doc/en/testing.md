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
| End-to-end | `test_http_client_e2e`, `test_http_keep_alive_e2e`, `test_websocket_client_e2e` |

Database tests require a running MySQL or ClickHouse service. Network tests use local ports and settings from `tests/app.ini`.

## CMake switches

```cmake
option(CRAZY_BUILD_TESTS "Build crazy test executables" ON)
option(CRAZY_BUILD_E2E_TESTS "Build network e2e tests" ON)
option(CRAZY_BUILD_DB_TESTS "Build database tests" ON)
```

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
| HTTP | Route priority, deferred send, static path safety |
| Databases | Connect failure, queries, rollback, pool exhaustion |
| Locks | Contention, duplicate release, timeout behavior |
| Serialization | Empty values, nested containers, compatibility, malformed input |
| WebSocket | Handshake, fragmentation, control frames, disconnect |

Related: [Feature overview](overview.md), [Build](build.md), [HTTP server](http-server.md).
