# URI

[Back to documentation index](../README.en.md)

`crazy::Uri` parses, modifies, compares, and resolves relative URIs.

## Parse and inspect

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

## Modify and resolve

```cpp
uri.addQueryParam("limit", "20");
uri.setFragment("page-1");

const std::string text = uri.toString();
crazy::Uri next = uri.resolve(crazy::Uri("../orders"));
CRAZY_SYSTEM_INFO() << next.toString();
```

`authority()` returns the scheme, user information, host, and port only. `resource()` returns path, query, and fragment. `resolve()` merges paths and simplifies `.` and `..` segments.

## Components

```text
https://user:pass@example.com:8443/api/users?page=1#list
\___/   \_______/ \_________/ \__/ \________/ \____/ \__/
scheme  userInfo      host    port   path     query fragment
```

`getUserInfo()` returns `user:pass`; `getUserName()` and `getPassword()` split it further. A missing port returns 0.

## API summary

| API | Purpose |
|-----|---------|
| `getScheme()` / `setScheme()` | Protocol |
| `getHost()` / `setHost()` | Host |
| `getPort()` / `setPort()` | Port |
| `getPath()` / `setPath()` | Path |
| `addQueryParam()` / `getQueryParam()` | Query values |
| `getQueryParams()` | Query list |
| `resolve()` | Resolve a relative URI |
| `isHostLoopback()` / `isHostWildcard()` | Address classification |

## Query mutation

```cpp
crazy::Uri uri("https://example.com/search");
uri.addQueryParam("q", "crazy framework");
uri.addQueryParam("page", "1");
uri.setFragment("results");

const std::string text = uri.toString();
```

## Relative resolution

```cpp
crazy::Uri base("https://example.com/api/v1/users?page=1");

const crazy::Uri parent = base.resolve(crazy::Uri("../orders"));
const crazy::Uri absolute =
    base.resolve(crazy::Uri("https://cdn.example.com/a.png"));
const crazy::Uri query =
    base.resolve(crazy::Uri("?page=2"));
```

`Uri` performs syntax operations only. DNS, authorization, and redirect policy remain the application's responsibility.

Related: [HTTP client](http-client.md), [HTTP server](http-server.md), [General utilities](utilities.md).
