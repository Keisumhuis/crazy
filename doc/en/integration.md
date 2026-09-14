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
