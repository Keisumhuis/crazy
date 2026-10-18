# Project Integration

[Back to documentation index](../README.en.md)

## CMake integration

```cmake
add_subdirectory(path/to/crazy)

add_executable(your_app main.cpp)
target_link_libraries(your_app PRIVATE crazy)
```

The `crazy` target propagates its public include paths, including the generated version header.

## Include headers

Use the aggregate header:

```cpp
#include "crazy.h"

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.exec();
}
```

Or include only what is needed:

```cpp
#include "crazy/application.h"
#include "crazy/logger.h"

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    CRAZY_SYSTEM_INFO() << "application created";
    app.exec();
}
```

## Manual linking

Set the source include directory, generated include directory, and static library manually:

```cmake
target_include_directories(your_app PRIVATE
    path/to/crazy/src
    path/to/crazy/build/generated
)
target_link_libraries(your_app PRIVATE
    path/to/crazy/build/crazy.lib
    ws2_32
)
```

On Linux, also link `pthread` and `dl`. `add_subdirectory` is preferred because it handles platform and third-party dependencies consistently.

### Transitive dependencies of TLS (`CRAZY_ENABLE_TLS`, ON by default)

`CRAZY_ENABLE_TLS` defaults to **ON**: the default build **statically links** the vendored
`src/third_party/openssl` (OpenSSL 3.3.0, a path relative to the repository root), specifically `ssl` and
`crypto`, so every downstream should carry those two libraries as shown below. Pass
`-DCRAZY_ENABLE_TLS=OFF` to turn it off explicitly (then downstream no longer needs them). Note that this is
only about whether the TLS libraries are linked at **build time**; whether HTTPS is actually served at **run
time** is still decided by `tls_enable` and the certificate paths in the `[HTTP]` section. Because `crazy` is
a **static library**, that dependency is not bundled into it: **the downstream executable has to provide
`ssl` and `crypto` on its own link line as well**.

- With `add_subdirectory`, the CMake target propagates the dependency for you (`target_link_libraries(... crazy)` is enough).
- When linking `crazy.lib` / `libcrazy.a` manually, add it yourself, for example on Windows:

```cmake
target_link_libraries(your_app PRIVATE
    path/to/crazy/build/Release/crazy.lib
    path/to/crazy/src/third_party/openssl/lib/windows/x64/release/ssl.lib
    path/to/crazy/src/third_party/openssl/lib/windows/x64/release/crypto.lib
    ws2_32 crypt32 advapi32 user32 bcrypt gdi32
)
```

Point `CRAZY_OPENSSL_ROOT` (which already defaults to the in-repo `src/third_party/openssl`) at the same
package when configuring. On Linux the files are
`lib/{linux}/{x64|x86}/{debug|release}/libssl.a` and `libcrypto.a`.

Licensing and attribution for OpenSSL are in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) at the
repository root: the vendored copy contains **no LICENSE / COPYING / NOTICE file at all**, so anyone
distributing a product linked against it must ship the Apache-2.0 text as well.

### Bundled CA bundle (the client's default trust store)

Clients can verify real public HTTPS sites out of the box: the framework ships a CA bundle at
`src/third_party/ca/cacert.pem` (188900 bytes / 121 certificates, taken from `https://curl.se/ca/cacert.pem`,
derived from the Mozilla CA extract). It is read at **configure time** into
`src/crazy/net/ca_bundle.cpp` in the source tree (committed with the repository) and **compiled into the
static library**, so downstream users need **no extra
link library and no CA file next to their distribution**, and there is no relative-path lookup at run time.
Origin, SHA256 and licensing (MPL-2.0) are documented in
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) at the repository root.

- If that PEM is missing, configure only **warns** (it does not fail hard) and clients fall back to the
  OpenSSL default verify paths.
- Point `-DCRAZY_CA_BUNDLE_FILE=<path>` at another PEM, or replace the file and re-run configure.
- The trust store priority (`tls_ca_file` / `SSL_CERT_FILE` / bundled bundle) is documented in
  "Default trust store and how to override it" in [http-client.md](http-client.md).

## Typical project layout

```text
your_project/
  CMakeLists.txt
  src/
    main.cpp
  config/
    app.ini
  third_party/
    crazy/
```

```cmake
cmake_minimum_required(VERSION 3.15)
project(example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(third_party/crazy)

add_executable(example src/main.cpp)
target_link_libraries(example PRIVATE crazy)
```

Start the executable from a directory containing `app.ini`, because `Application` scans the current directory:

```bash
cmake -S . -B build
cmake --build build --parallel
cd build
./example
```

## Runtime dependencies

| Module | Runtime requirement |
|--------|---------------------|
| Application | Writable working directory for lock and command files |
| MySQL | Reachable service and a `[MySQL]` section |
| ClickHouse | Reachable service and a `[ClickHouse]` section |
| Static HTTP | Existing directory readable by the process |
| SMTP | Reachable plaintext SMTP server |
| HTTPS / WSS | TLS support is compiled in by default (`CRAZY_ENABLE_TLS` defaults to ON); enabling it at run time needs `tls_enable=true` plus readable PEM certificate and key paths in `[HTTP]` (clients additionally need `tls_ca_file` or the system trust store). A build with `-DCRAZY_ENABLE_TLS=OFF` has no TLS at all |
| Telnet | Listenable port and a strong password |

## Platform notes

Windows links `ws2_32` automatically. Linux uses Unix Domain Sockets for `LocalSocket`, with platform path-length limits. In containers, keep the command socket, lock files, and logs on a writable or persistent volume.

## Recommended initialization order

1. Construct `Application` or `HttpApplication`.
2. Register Actors, keeping names aligned with config sections.
3. Add message routes.
4. Configure HTTP routes and static directories.
5. Call `exec()` to start Actors, pools, and network services.

```cpp
crazy::HttpApplication app(argc, argv);
app.registerActor<Worker>("worker");
app.registerHttpHandler<crazy::GET>(
    "/health", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
        response.setBody("ok");
    });
app.exec();
```

Related: [Application](application.md), [Configuration](config.md), [Testing](testing.md).
