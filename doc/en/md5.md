# MD5

[Back to documentation index](../README.en.md)

`crazy::MD5` computes MD5 digests for legacy compatibility and non-security checks.

## API

| API | Purpose |
|-----|---------|
| `MD5(message)` | Construct and calculate |
| `getDigest()` | Return the 16-byte digest |
| `toString()` | Return hexadecimal text |
| `MD5::encryption()` | One-shot hexadecimal result |

## Basic example

```cpp
#include "crazy/encryption/md5.h"

const std::string hex = crazy::MD5::encryption("hello");

crazy::MD5 md5("hello");
const uint8_t* bytes = md5.getDigest();

CRAZY_SYSTEM_INFO() << hex;
CRAZY_SYSTEM_INFO() << static_cast<int>(bytes[0]);
```

## Content checks

```cpp
std::string fileContent = readWholeFile(path);
const std::string digest =
    crazy::MD5::encryption(fileContent);
```

The current public API accepts a complete string, so large files are loaded into memory. MD5 is collision-prone and is not suitable for trusted download verification or signatures.

## Digest object behavior

`MD5(message)` stores the message state. The first `getDigest()` finalizes the digest and returns the internal 16-byte buffer. Later calls return the same result. `toString()` returns 32 lowercase hexadecimal characters.

```cpp
crazy::MD5 digest("hello");
const std::string first = digest.toString();
const std::string second = digest.toString();
```

One object is not thread-safe. Do not call `getDigest()` or `toString()` concurrently on the same instance.

## Output formats

| Form | Size | Typical use |
|------|------|-------------|
| Raw digest | 16 bytes | Binary protocols |
| Hex digest | 32 characters | Logs and text interfaces |

Use `toString()` for logs because raw digest bytes may be unprintable.

## Security guidance

- Do not store password hashes as MD5.
- Do not sign data or issue authentication tokens with MD5.
- Use SHA-256 or stronger for trusted integrity checks.
- Use Argon2, scrypt, or bcrypt for password storage.
- Validate digest length and hexadecimal format for external input.

Related: [Base64](base64.md), [Configuration](config.md), [SMTP client](smtp.md).
