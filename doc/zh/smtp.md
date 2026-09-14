# SMTP 客户端

[返回文档目录](../README.md)

`crazy::SmtpClient` 提供明文 SMTP、AUTH PLAIN/AUTH LOGIN 和文本邮件发送。

## 发送邮件

```cpp
#include "crazy/net/smtp/smtp_client.h"

crazy::SmtpClient client;
client.setClientName("mail.example.com");

if (!client.connect("smtp.example.com", 25)) {
    CRAZY_SYSTEM_ERROR() << "connect smtp failed";
    return;
}

if (!client.login("user@example.com", "password",
                  crazy::SmtpClient::AuthType::login)) {
    CRAZY_SYSTEM_ERROR() << "smtp login failed";
    return;
}

crazy::SmtpClient::MailMessage mail;
mail.from = "user@example.com";
mail.to = {"receiver@example.com"};
mail.subject = "crazy mail";
mail.body = "hello from crazy";

if (!client.sendMail(mail)) {
    CRAZY_SYSTEM_ERROR() << "send mail failed";
}
```

当前实现是阻塞的明文 SMTP，不包含 TLS。公网环境应通过加密隧道连接邮件服务器，或在外层使用支持 TLS 的代理。

## 发送流程

`connect()` 会完成 TCP 连接并处理服务器 greeting，`hello()` 优先发送 EHLO，失败后回退 HELO。`login()` 根据选择使用 AUTH PLAIN 或 AUTH LOGIN。`sendMail()` 依次发送 MAIL FROM、RCPT TO 和 DATA，正文执行 dot-stuffing，最后发送结束标记。

## MailMessage 字段

| 字段 | 说明 |
|------|------|
| `from` | 发件人地址 |
| `to` | 主收件人 |
| `cc` | 抄送地址 |
| `bcc` | 密送地址，写入 DATA 前不会出现在正文头 |
| `subject` | 邮件主题 |
| `body` | 邮件正文 |
| `contentType` | 默认 `text/plain; charset=UTF-8` |
| `headers` | 额外自定义头 |

## 便捷发送接口

```cpp
const bool sent = client.sendMail(
    "sender@example.com",
    {"a@example.com", "b@example.com"},
    "daily report",
    "report body",
    {"manager@example.com"},
    {"audit@example.com"});
```

## 错误处理

每个阶段都返回布尔值。生产代码应拆分判断，以便定位 DNS、连接、greeting、认证、收件人或 DATA 阶段问题：

```cpp
if (!client.connect(host, 587)) {
    CRAZY_SYSTEM_ERROR() << "connect failed";
} else if (!client.login(user, password,
                         crazy::SmtpClient::AuthType::plain)) {
    CRAZY_SYSTEM_ERROR() << "auth failed";
} else if (!client.sendMail(mail)) {
    CRAZY_SYSTEM_ERROR() << "send failed";
}
```

## 兼容性与限制

- 当前只支持明文 SMTP。
- 只发送纯文本或调用方自行构造的 Content-Type。
- 不包含附件编码和前台上传。
- Socket 读写为阻塞行为。
- 服务器响应按单行或多行响应解析。

邮件发送适合在线程池任务中执行，不应阻塞 Actor 或 HTTP 请求线程。相关文档：[Socket](socket.md)、[ThreadPool](thread-pool.md)、[日志系统](logger.md)。
