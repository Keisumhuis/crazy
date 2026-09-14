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
