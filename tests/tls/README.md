# TLS 测试夹具证书（自签、仅供 127.0.0.1 测试）

这些 PEM 是**测试专用**的自签证书与私钥，**没有任何信任价值**：

- 只用于本地 `127.0.0.1` 的 TLS 用例，不配置到任何生产环境；
- 自签、有效期 10 年（`server_*` / `other_*`），或不用于任何真实校验（`expired_*`）；
- 提交进仓库是为了让 TLS 用例**完全自包含**——运行测试不需要任何外部 `openssl.exe`。

| 文件 | 用途 |
|---|---|
| `server_cert.pem` / `server_key.pem` | 服务端证书（`CN=localhost`，SAN `DNS:localhost,IP:127.0.0.1`），AC-3/4/5/8/9/10/11 使用 |
| `other_cert.pem` / `other_key.pem` | 「不受信 CA」负例（`CN=other.local`），AC-6② 使用 |
| `expired_cert.pem` / `expired_key.pem` | 已过期证书（2020-01-01 → 2021-01-01），AC-6④ 使用 |

生成命令（OpenSSL 3.6.0，实测通过）：

```powershell
$O = 'C:\Program Files\OpenSSL-Win64\bin\openssl.exe'
& $O req -x509 -newkey rsa:2048 -keyout server_key.pem -out server_cert.pem `
    -days 3650 -nodes -subj "/CN=localhost" `
    -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
& $O req -x509 -newkey rsa:2048 -keyout other_key.pem -out other_cert.pem `
    -days 3650 -nodes -subj "/CN=other.local" `
    -addext "subjectAltName=DNS:other.local"
& $O req -x509 -newkey rsa:2048 -keyout expired_key.pem -out expired_cert.pem `
    -nodes -subj "/CN=expired.local" `
    -addext "subjectAltName=DNS:expired.local,IP:127.0.0.1" `
    -not_before 20200101000000Z -not_after 20210101000000Z
```

`expired_cert.pem` 的 SAN 额外带上 `IP:127.0.0.1`，用于把 AC-6④ 的失败原因**限定在证书有效期**上，
避免「主机名不匹配」先失败而掩盖有效期校验。
