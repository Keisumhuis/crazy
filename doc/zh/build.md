# 编译与构建

[返回文档目录](../README.md)

## 环境要求

- CMake 3.15 或更高版本
- 支持 C++17 的编译器
- Windows 使用 Visual Studio 2022 或更新版本
- Linux 使用主流的 GCC 或 Clang 工具链

## 手动构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `CRAZY_BUILD_TESTS` | `ON` | 构建 `tests/` 下的测试目标 |
| `CRAZY_BUILD_E2E_TESTS` | `ON` | 构建 HTTP/WebSocket 端到端测试 |
| `CRAZY_BUILD_DB_TESTS` | `ON` | 构建 MySQL 和 ClickHouse 测试 |

只构建库：

```bash
cmake -S . -B build \
  -DCRAZY_BUILD_TESTS=OFF \
  -DCRAZY_BUILD_E2E_TESTS=OFF \
  -DCRAZY_BUILD_DB_TESTS=OFF
cmake --build build --target crazy
```

## 构建脚本

Windows：

```bat
build_crazy_framework.bat
```

Linux：

```bash
chmod +x build_crazy_framework.sh
./build_crazy_framework.sh
```

脚本会在 `out_windows` 或 `out_linux` 下分别生成 x86/x64、Debug/Release 构建目录。Linux x86 构建需要系统安装 32 位开发库。

## 第三方依赖

MySQL 和 ClickHouse 客户端头文件及预编译库已经放在 `src/third_party`。CMake 根据操作系统和指针宽度选择对应目录。缺少当前平台的库时，CMake 会发出警告，数据库模块可能无法链接。

## 构建产物

单配置生成器通常产生：

```text
build/
  libcrazy.a                  # Linux
  crazy.lib                   # Windows
  test_actor
  test_http
  config/app.ini
  http_static/index.html
```

Visual Studio 等多配置生成器会把可执行文件放到 `Debug/`、`Release/` 等子目录。`CMakeLists.txt` 还会把 `tests/app.ini` 和 `tests/http_static` 复制到构建目录，网络测试默认依赖这些资源。

## Debug 与 Release

Debug 构建会增加 `_DEBUG` 或 `DEBUG` 宏，并保留完整调试信息。Release 构建启用优化并定义 `NDEBUG`。两者生成的版本头文件都会写入当前 Git 提交和分支，本地没有 Git 信息时字段可能为空。

推荐日常开发使用：

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure
```

性能或部署验证使用：

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
cmake --install build/release --prefix ./dist
```

项目当前没有安装规则时，`cmake --install` 可能不会复制库和头文件，应直接引用构建目录中的目标，或自行复制 `src`、生成的头文件目录和目标库。

## 常见问题

| 现象 | 原因与处理 |
|------|------------|
| CMake 找不到编译器的 C++17 能力 | 升级编译器，或显式指定 `CMAKE_CXX_COMPILER` |
| MySQL/ClickHouse 符号缺失 | 当前平台或架构没有预编译库，检查 `src/third_party` |
| 数据库测试失败 | 本机没有对应服务，或连接参数不匹配 |
| HTTP 静态测试找不到文件 | 从构建目录运行测试，或先完成 CMake 配置步骤 |
| Linux 构建 x86 失败 | 安装 `gcc-multilib` / `g++-multilib` 等 32 位开发包 |

## 只验证改动

修改文档后至少检查链接和文件结构：

```powershell
Get-ChildItem doc -Recurse -File
```

修改 C++ 代码后建议至少构建一个测试目标：

```bash
cmake --build build --target test_actor
ctest --test-dir build -R test_actor --output-on-failure
```

相关文档：[接入现有项目](integration.md)、[测试](testing.md)。
