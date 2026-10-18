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
| 端到端 | `test_http_client_e2e`, `test_http_keep_alive_e2e`, `test_http_chunked`, `test_http_chunked_upload`, `test_http_range`, `test_websocket_client_e2e` |
| TLS / HTTPS | `test_https_tls_server`, `test_https_tls_client`, `test_https_tls_config`, `test_https_client`（TLS 编译期**默认开启**，默认配置即可全部构建；`test_https_client` 在 `-DCRAZY_ENABLE_TLS=OFF` 时也构建，用于断言 `https://` 显式失败） |
| 公网（**默认关闭**） | `test_https_baidu`（**默认不构建**，需要 `-DCRAZY_BUILD_NETWORK_TESTS=ON`；访问真实公网 `https://www.baidu.com` 并把应答回显，用仓库自研 OpenSSL 客户端跑） |

数据库测试需要可用的 MySQL 或 ClickHouse 服务，网络测试需要本机端口和 `tests/app.ini` 中的配置。

### 公网用例 `test_https_baidu`（开关 `CRAZY_BUILD_NETWORK_TESTS`，当前默认开启）

`test_https_baidu` 是**真实公网 HTTPS 端到端用例**：用仓库自研的 `TlsContext`/`TlsSocket` +
`HttpClient`（**不是** PowerShell/.NET）访问 `https://www.baidu.com/`，打印状态行、关键响应头、
正文前 4 KB 与正文总字节数，并把**完整正文**写到构建树根下的 `baidu-response.html`（路径会打进日志）。

它**依赖公网**（DNS + TCP 443 + 真实 CA 链），因此开关为 `CRAZY_BUILD_NETWORK_TESTS`（**当前默认 `ON`**）：
需要纯离线/干净机器时可显式 `-DCRAZY_BUILD_NETWORK_TESTS=OFF`，那样它既不构建、也不出现在 `ctest -N` 里。
真正的离线运行会按 DNS/TCP 失败判 **Skipped**（返回码 77），不会变红。

```powershell
cmake -S . -B build/net-tests -G "Visual Studio 18 2026" -A x64 `
  -DCRAZY_BUILD_NETWORK_TESTS=ON
cmake --build build/net-tests --config Release --target test_https_baidu
ctest --test-dir build/net-tests -C Release --output-on-failure -R test_https_baidu
```

判定口径：

- **离线 / DNS 失败 / 整体超时**（用例整体有界，预算 9 秒）→ 打印 `SKIP:` 与具体原因，
  以返回码 77 退出，CTest 记为 **Skipped**（`SKIP_RETURN_CODE`），**不是 Failed**。
- **本构建没有编译进 TLS 客户端**（`-DCRAZY_ENABLE_TLS=OFF`）→ 同样打印 `SKIP:` 与原因后
  以 77 退出（没有 TLS 客户端属于构建配置，不是被测客户端的缺陷）。
- **TLS 握手或证书校验失败** → **Failed**，并**原样**打印 OpenSSL 错误串。
  用例**从不**关闭证书校验（不用 `SSL_VERIFY_NONE`、不跳过主机名校验）。
- 证书校验**默认可用，不需要任何环境变量**：客户端把随包 CA bundle（`src/third_party/ca/cacert.pem`，
  121 张证书）编译进了库——Windows 上 OpenSSL **不**读系统证书存储，这正是必需的。详见
  [http-client.md](http-client.md) 的「默认信任库与覆盖方式」。
  若显式设置了 `SSL_CERT_FILE` / `SSL_CERT_DIR`，用例**只用**该信任库、不叠加随包 bundle：
  指向不含目标站点 CA 的文件时用例会**真的 Failed**，而不是退回「不校验」。

## CMake 开关

```cmake
option(CRAZY_BUILD_TESTS "Build crazy test executables" ON)
option(CRAZY_BUILD_E2E_TESTS "Build network e2e tests" ON)
option(CRAZY_BUILD_DB_TESTS "Build database tests" ON)
option(CRAZY_ENABLE_TLS "Enable TLS/HTTPS transport (requires OpenSSL)" ON)
option(CRAZY_BUILD_NETWORK_TESTS "Build tests that require real public network access" ON)
```

TLS **默认为 ON**（默认构建即包含 TLS 支持，使用仓库内 `src/third_party/openssl`），
因此不需要任何 `-D` 参数；显式关闭用：

```bash
cmake -S . -B build/notls -G "Visual Studio 18 2026" -A x64 \
  -DCRAZY_ENABLE_TLS=OFF
```

（编译期默认支持 ≠ 运行期默认启用：HTTPS 仍要在 `[HTTP]` 节配 `tls_enable=true` 与证书，见
[http-server.md](http-server.md) 的 HTTPS 小节。）

`CRAZY_OPENSSL_ROOT` 缺目录、缺 `include/openssl/ssl.h`、或缺对应的 `ssl`/`crypto` 库时，
configure **硬失败并指名缺的是哪个文件**，不会静默关掉 TLS。

公网用例 `test_https_baidu` 跟随 `CRAZY_BUILD_NETWORK_TESTS`（**当前默认 `ON`**）：它注册在
`CRAZY_BUILD_E2E_TESTS` 条件块内，所以还要求 E2E 测试为 `ON`（默认 `ON`）。
离线或干净机器可用 `-DCRAZY_BUILD_NETWORK_TESTS=OFF` 把它排除在构建与 `ctest -N` 之外；
真正离线运行时它按 DNS/TCP 失败判 **SKIP**（返回码 77），不会把机器搞红。

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
| HTTP | 路由优先级、延迟响应、静态路径安全、分块响应/请求体、Range 分片与 HTTP/1.0 降级 |
| 数据库 | 建连失败、查询、事务回滚和池耗尽 |
| 锁 | 并发争用、重复释放和超时行为 |
| 序列化 | 空值、嵌套容器、向后兼容和非法输入 |
| WebSocket | 握手、分片、控制帧和断开 |

相关文档：[功能总览](overview.md)、[编译与构建](build.md)、[HTTP 服务端](http-server.md)。
