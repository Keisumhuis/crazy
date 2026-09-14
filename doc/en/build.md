# Build

[Back to documentation index](../README.en.md)

## Requirements

- CMake 3.15 or newer
- A C++17 compiler
- Visual Studio 2022 or newer on Windows
- GCC or Clang on Linux

## Manual build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `CRAZY_BUILD_TESTS` | `ON` | Build tests under `tests/` |
| `CRAZY_BUILD_E2E_TESTS` | `ON` | Build HTTP/WebSocket end-to-end tests |
| `CRAZY_BUILD_DB_TESTS` | `ON` | Build MySQL and ClickHouse tests |

Build only the library:

```bash
cmake -S . -B build \
  -DCRAZY_BUILD_TESTS=OFF \
  -DCRAZY_BUILD_E2E_TESTS=OFF \
  -DCRAZY_BUILD_DB_TESTS=OFF
cmake --build build --target crazy
```

## Build scripts

Windows:

```bat
build_crazy_framework.bat
```

Linux:

```bash
chmod +x build_crazy_framework.sh
./build_crazy_framework.sh
```

The scripts create x86/x64 Debug/Release builds under `out_windows` or `out_linux`. A Linux x86 build requires the 32-bit development libraries.

MySQL and ClickHouse headers and prebuilt libraries are included under `src/third_party`. CMake selects the current platform and architecture automatically.

## Build output

A single-configuration generator normally produces:

```text
build/
  libcrazy.a                 # Linux
  crazy.lib                  # Windows
  test_actor
  test_http
  config/app.ini
  http_static/index.html
```

Multi-configuration generators place executables under `Debug/`, `Release/`, and similar subdirectories. CMake copies `tests/app.ini` and `tests/http_static` into the build tree because network tests use them.

## Debug and Release

Debug builds define `_DEBUG` or `DEBUG` and retain full debug information. Release builds define `NDEBUG` and enable optimization. Both write the Git commit and branch into the generated version header.

Recommended development build:

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure
```

Release verification:

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
```

## Troubleshooting

| Symptom | Cause and action |
|---------|------------------|
| C++17 test fails | Upgrade the compiler or set `CMAKE_CXX_COMPILER` |
| MySQL/ClickHouse symbols are missing | The matching prebuilt library is absent |
| Database tests fail | No local service or invalid connection settings |
| Static HTTP tests cannot find files | Run from the build tree after CMake configuration |
| Linux x86 build fails | Install 32-bit GCC/G++ development packages |

After changing a single component:

```bash
cmake --build build --target test_actor
ctest --test-dir build -R test_actor --output-on-failure
```

Related: [Project integration](integration.md), [Testing](testing.md).
