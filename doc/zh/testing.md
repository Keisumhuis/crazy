# 测试

[返回文档目录](../README.md)

项目使用 CTest 管理测试目标。测试和端到端测试可以通过 CMake 选项单独关闭。

## 运行全部测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
```

## 运行单个测试

```bash
cmake --build build --target test_http_client
./build/test_http_client
```

Visual Studio 多配置生成器的可执行文件通常位于 `build/Debug` 或 `build/Release`：

```powershell
.\build\Debug\test_http_client.exe
```

## 测试分组

| 分组 | 目标 |
|------|------|
| 基础 | `test_actor`, `test_config`, `test_logger`, `test_json`, `test_protocol` |
| 网络服务 | `test_socket`, `test_localsocket`, `test_http`, `test_websocket`, `test_websocket_server` |
| 客户端 | `test_actor_client`, `test_http_client`, `test_websocket_client`, `test_smtp_client` |
| 数据库 | `test_mysql`, `test_clickhouse` |
| 工具 | `test_date_time`, `test_encryption`, `test_key_value_pair`, `test_lock`, `test_mmap`, `test_mvcc_lock_wrapper` |
| 端到端 | `test_http_client_e2e`, `test_http_keep_alive_e2e`, `test_websocket_client_e2e` |

数据库测试需要可用的 MySQL 或 ClickHouse 服务，网络测试需要本机端口和 `tests/app.ini` 中的配置。

## CMake 开关

```cmake
option(CRAZY_BUILD_TESTS "Build crazy test executables" ON)
option(CRAZY_BUILD_E2E_TESTS "Build network e2e tests" ON)
option(CRAZY_BUILD_DB_TESTS "Build database tests" ON)
```

关闭数据库测试：

```bash
cmake -S . -B build \
  -DCRAZY_BUILD_DB_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
```

关闭端到端测试：

```bash
cmake -S . -B build \
  -DCRAZY_BUILD_E2E_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
```

## 测试资源

CMake 会把以下资源复制到构建目录：

```text
tests/app.ini
tests/http_static/
```

网络测试默认使用 `tests/app.ini` 中的端口和地址。修改测试端口时，应确认这些端口没有被本机其他进程占用。端到端测试通常会在测试进程内启动服务端，再通过真实 Socket 发起请求。

## 按名称运行

```bash
ctest --test-dir build -R test_actor --output-on-failure
ctest --test-dir build -R "test_http|test_websocket" --output-on-failure
```

也可以直接运行测试可执行文件获取详细控制台输出：

```bash
./build/test_http_client
```

## 新增测试

测试目标由 `add_crazy_test()` 创建：

```cmake
add_crazy_test(test_your_feature)
```

对应实现文件为 `tests/test_your_feature.cc`。测试目标自动链接 `crazy`、线程库以及可用的数据库库。新增 E2E 或 DB 测试时，应放到相应 CMake 条件块中，避免普通构建在缺少环境时失败。

## 测试覆盖建议

| 模块 | 最低测试内容 |
|------|--------------|
| Buffer/Decoder | 分包、粘包、超长报文、非法长度 |
| Actor | 路由、命令响应、异步任务和停止 |
| HTTP | 路由优先级、延迟响应、静态路径安全 |
| 数据库 | 建连失败、查询、事务回滚和池耗尽 |
| 锁 | 并发争用、重复释放和超时行为 |
| 序列化 | 空值、嵌套容器、向后兼容和非法输入 |
| WebSocket | 握手、分片、控制帧和断开 |

相关文档：[功能总览](overview.md)、[编译与构建](build.md)、[HTTP 服务端](http-server.md)。
