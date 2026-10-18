# 接入现有项目

[返回文档目录](../README.md)

## 使用 CMake 接入

```cmake
add_subdirectory(path/to/crazy)

add_executable(your_app main.cpp)
target_link_libraries(your_app PRIVATE crazy)
```

`crazy` 的公开包含目录由 CMake 目标自动传递，业务代码无需再手工添加 `src`。

## 引入头文件

使用聚合头文件：

```cpp
#include "crazy.h"

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.exec();
}
```

按需引入可以缩短编译依赖：

```cpp
#include "crazy/application.h"
#include "crazy/logger.h"

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    CRAZY_SYSTEM_INFO() << "application created";
    app.exec();
}
```

## 不使用 add_subdirectory

先构建并安装或直接链接生成的静态库，然后手动设置头文件目录：

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

Linux 下还需要链接 `pthread` 和 `dl`。推荐优先使用 `add_subdirectory`，避免平台和第三方库配置遗漏。

### TLS 的传递依赖（`CRAZY_ENABLE_TLS`，默认 ON）

`CRAZY_ENABLE_TLS` 默认为 **ON**：默认构建即会**静态链接**仓库内 `src/third_party/openssl`
（OpenSSL 3.3.0，相对仓库根的路径）的 `ssl` 与 `crypto`，所以任何下游都应按下面这样把这两个库带上；
用 `-DCRAZY_ENABLE_TLS=OFF` 可显式关闭（关闭后下游就不再需要它们）。
注意这只是**编译期**是否链接 TLS 库；运行期是否真的走 HTTPS 仍由 `[HTTP]` 节的 `tls_enable` 与证书决定。
由于 `crazy` 是**静态库**，这份依赖不会**被自动“打包”进去**：**下游可执行文件链接时同样需要提供
`ssl` 与 `crypto`**。

- 用 `add_subdirectory` 接入时，CMake 目标会把这一依赖自动传递（`target_link_libraries(... crazy)` 即可）。
- 手动链接 `crazy.lib` / `libcrazy.a` 时，必须自己补上，例如 Windows：

```cmake
target_link_libraries(your_app PRIVATE
    path/to/crazy/build/Release/crazy.lib
    path/to/crazy/src/third_party/openssl/lib/windows/x64/release/ssl.lib
    path/to/crazy/src/third_party/openssl/lib/windows/x64/release/crypto.lib
    ws2_32 crypt32 advapi32 user32 bcrypt gdi32
)
```

并在 configure 时把 `CRAZY_OPENSSL_ROOT`（默认即仓库内 `src/third_party/openssl`）指向同一份包。
Linux 下对应的是 `lib/{linux}/{x64|x86}/{debug|release}/libssl.a` 与 `libcrypto.a`。

OpenSSL 的许可与署名要求见仓库根 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)：
该副本内**不含任何 LICENSE / COPYING / NOTICE**，分发链接了它的产物时必须一并保留 Apache-2.0 文本。

### 随包 CA bundle（客户端默认信任库）

客户端默认就能校验真实公网 HTTPS 站点：随框架分发一份 CA bundle `src/third_party/ca/cacert.pem`
（188900 字节 / 121 张证书，取自 `https://curl.se/ca/cacert.pem`，数据源自 Mozilla CA extract）。
它在 **configure 期**被读成源码树内的 `src/crazy/net/ca_bundle.cpp`（随仓库提交）并**编译进静态库**，
因此**下游不需要额外链接任何库、也不需要随发行附带 CA 文件**，运行期不做任何相对路径查找。
来源、SHA256 与许可（MPL-2.0）说明见仓库根 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)。

- 该 PEM 缺失时 configure 只**告警**（不硬失败），客户端退回「只用 OpenSSL 默认验证路径」。
- 可用 `-DCRAZY_CA_BUNDLE_FILE=<path>` 指向别的 PEM；替换该文件后重新 configure 即生效。
- 信任库优先级（`tls_ca_file` / `SSL_CERT_FILE` / 随包 bundle）见
  [http-client.md](http-client.md) 的「默认信任库与覆盖方式」。

## 典型工程布局

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

构建后必须从能访问 `app.ini` 的目录启动程序，因为 `Application` 默认扫描当前目录：

```bash
cmake -S . -B build
cmake --build build --parallel
cd build
./example
```

## 运行期依赖

链接成功不代表运行配置完整。以下能力依赖运行环境：

| 模块 | 运行期要求 |
|------|------------|
| Application | 当前目录可写，用于锁文件和本地命令 Socket |
| MySQL | 可达的 MySQL 服务和 `[MySQL]` 配置段 |
| ClickHouse | 可达的 ClickHouse 服务和 `[ClickHouse]` 配置段 |
| HTTP 静态目录 | 注册目录真实存在且进程有读取权限 |
| SMTP | 可达的 SMTP 服务，当前仅支持明文连接 |
| HTTPS / WSS | 编译期默认支持（`CRAZY_ENABLE_TLS` 默认 ON）；运行期启用时 `[HTTP]` 节需给出可读的 PEM 证书与私钥与 `tls_enable=true`（客户端还需要 `tls_ca_file` 或系统信任库）；`-DCRAZY_ENABLE_TLS=OFF` 构建的产物则完全不支持 |
| Telnet | 端口可监听，并配置非空且足够强的密码 |

## 平台注意事项

Windows 构建会自动链接 `ws2_32`，数据库模块还需要随库使用的系统运行库。Linux 下本地 Socket 使用 Unix Domain Socket，路径长度受系统限制。若部署到容器，命令 Socket、锁文件和日志目录应放在持久化或可写卷中。

## 推荐初始化顺序

1. 构造 `Application` 或 `HttpApplication`，触发配置加载。
2. 注册所有 Actor，并检查名称是否与配置段一致。
3. 添加消息路由。
4. 配置 HTTP 路由和静态目录。
5. 调用 `exec()`，由框架启动 Actor、线程池和网络服务。

```cpp
crazy::HttpApplication app(argc, argv);
app.registerActor<Worker>("worker");
app.registerHttpHandler<crazy::GET>(
    "/health", [](crazy::HttpRequest&, crazy::HttpResponse& response) {
        response.setBody("ok");
    });
app.exec();
```

相关文档：[Application](application.md)、[配置管理](config.md)、[测试](testing.md)。
