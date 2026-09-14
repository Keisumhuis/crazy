# SMTP Client

[Back to documentation index](../README.en.md)

`crazy::SmtpClient` implements plaintext SMTP, AUTH PLAIN/AUTH LOGIN, and text email delivery.

## Send mail

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

The current implementation is blocking and plaintext, without TLS. Use a secure tunnel or TLS-capable proxy for public networks.

## Send flow

`connect()` establishes TCP and reads the greeting. `hello()` tries EHLO and falls back to HELO. `login()` supports AUTH PLAIN and AUTH LOGIN. `sendMail()` sends MAIL FROM, RCPT TO, and DATA, applies dot-stuffing to the body, and terminates the message.

## Mail message fields

| Field | Purpose |
|-------|---------|
| `from` | Sender address |
| `to` / `cc` / `bcc` | Recipient lists |
| `subject` | Subject |
| `body` | Message body |
| `contentType` | Defaults to plain UTF-8 text |
| `headers` | Additional headers |

## Convenience API

```cpp
const bool sent = client.sendMail(
    "sender@example.com",
    {"a@example.com", "b@example.com"},
    "daily report",
    "report body",
    {"manager@example.com"},
    {"audit@example.com"});
```

## Error handling

Check every stage separately so failures point to connection, greeting, login, recipient, or DATA handling:

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

## Limitations

- Plaintext SMTP only.
- No built-in TLS.
- Plain text or caller-defined content types.
- No attachment encoder.
- Blocking socket reads and writes.

Run mail delivery in a worker thread rather than an Actor or HTTP handler. Related: [Socket](socket.md), [ThreadPool](thread-pool.md), [Logging](logger.md).
