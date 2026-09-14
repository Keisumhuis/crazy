# URI

[返回文档目录](../README.md)

`crazy::Uri` 用于解析、修改、比较和解析相对 URI。

## 解析与读取

```cpp
#include "crazy/uri.h"

crazy::Uri uri(
    "https://user:pass@example.com:8443/api/users?page=1#list");

CRAZY_SYSTEM_INFO() << uri.getScheme();   // https
CRAZY_SYSTEM_INFO() << uri.getHost();     // example.com
CRAZY_SYSTEM_INFO() << uri.getPort();     // 8443
CRAZY_SYSTEM_INFO() << uri.getUserName(); // user
CRAZY_SYSTEM_INFO() << uri.getPath();     // /api/users
```

## 修改与解析相对地址

```cpp
uri.addQueryParam("limit", "20");
uri.setFragment("page-1");

const std::string text = uri.toString();
crazy::Uri next = uri.resolve(crazy::Uri("../orders"));
CRAZY_SYSTEM_INFO() << next.toString();
```

`authority()` 返回只包含 scheme、用户信息、主机和端口的 URI，`resource()` 返回路径、查询和片段。`resolve()` 会合并并简化 `.` 与 `..` 路径段。

## URI 组成部分

```text
https://user:pass@example.com:8443/api/users?page=1#list
\___/   \_______/ \_________/ \__/ \________/ \____/ \__/
scheme  userInfo      host    port   path     query fragment
```

`getUserInfo()` 返回完整 `user:pass`，`getUserName()` 和 `getPassword()` 会进一步拆分。未指定端口时 `getPort()` 返回 0。

## API 摘要

| 接口 | 说明 |
|------|------|
| `getScheme()` / `setScheme()` | 读取或修改协议 |
| `getHost()` / `setHost()` | 读取或修改主机 |
| `getPort()` / `setPort()` | 读取或修改端口 |
| `getPath()` / `setPath()` | 读取或修改路径 |
| `getQuery()` / `setQuery()` | 读取或修改原始查询字符串 |
| `addQueryParam()` | 添加键值查询参数 |
| `getQueryParam()` | 获取指定查询参数 |
| `getQueryParams()` | 获取全部参数列表 |
| `resolve()` | 基于当前 URI 解析相对地址 |
| `isHostLoopback()` / `isHostWildcard()` | 判断特殊主机 |

## 修改查询参数

```cpp
crazy::Uri uri("https://example.com/search");
uri.addQueryParam("q", "crazy framework");
uri.addQueryParam("page", "1");
uri.setFragment("results");

const std::string text = uri.toString();
CRAZY_SYSTEM_INFO() << text;
```

查询参数在修改时标记为 dirty，序列化时重新构建查询字符串。调用 `clearQueryParams()` 会删除全部参数，但不会自动删除路径或片段。

## 相对地址解析

```cpp
crazy::Uri base("https://example.com/api/v1/users?page=1");

const crazy::Uri parent = base.resolve(crazy::Uri("../orders"));
const crazy::Uri absolute =
    base.resolve(crazy::Uri("https://cdn.example.com/a.png"));
const crazy::Uri query =
    base.resolve(crazy::Uri("?page=2"));
```

`resolve()` 会把相对路径与 base path 合并，规范 `.` 和 `..`。解析结果不保证自动进行网络访问，只负责字符串层语义。

## 使用边界

- URI 类不负责 DNS、连接或权限校验。
- 用户信息可能包含敏感密码，日志输出前应脱敏。
- 主机和端口验证仍需业务层执行。
- 默认端口判断依赖 scheme 的约定。
- 不以 `http://` 或 `https://` 开头的输入不一定是绝对 URI。

相关文档：[HTTP 客户端](http-client.md)、[HTTP 服务端](http-server.md)、[通用工具](utilities.md)。
