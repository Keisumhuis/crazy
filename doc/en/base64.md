# Base64

[Back to documentation index](../README.en.md)

`crazy::Base64` encodes and decodes byte strings for JSON, headers, and text-based protocols.

## API

| API | Input | Output |
|-----|-------|--------|
| `encryption()` | Raw byte string | Base64 text |
| `decryption()` | Base64 text | Raw byte string |

Both operations use `std::string` as a byte container, so embedded NUL bytes are supported.

## Basic example

```cpp
#include "crazy/encryption/base64.h"

const std::string binary("\x00\x01\x02\x03", 4);
const std::string encoded = crazy::Base64::encryption(binary);
const std::string decoded = crazy::Base64::decryption(encoded);

CRAZY_SYSTEM_INFO() << encoded;
CRAZY_SYSTEM_INFO() << decoded.size();
```

## Message payload

```cpp
auto message = std::make_shared<crazy::MessageBase>();
message->setCmd(3001);
message->setData(crazy::Base64::encryption(pngBytes));
```

The receiver must know from `cmd` whether `data` is Base64 text or raw binary.

## Size and security

Encoded output is roughly 4/3 the input size and may contain `=` padding. Bound input length in the application protocol. Base64 is not encryption and does not hide content; use TLS or authenticated encryption when confidentiality is required.

| Input bytes | Encoded chars | Padding |
|-------------|---------------|---------|
| 1 | 4 | `==` |
| 2 | 4 | `=` |
| 3 | 4 | none |
| 4 | 8 | `==` |

`decryption()` stops at the first non-Base64 character or padding marker and returns the decoded prefix. It has no error code, so validate alphabet, length, and padding before decoding external input.

## Embed in JSON

```cpp
struct FilePayload {
    std::string name;
    std::string contentBase64;
    REFLECTION(name, contentBase64);
};

FilePayload payload{
    "avatar.png",
    crazy::Base64::encryption(pngBytes),
};

const std::string json =
    crazy::json::Converter::Serializable(payload);
```

The receiver should bound the encoded size before decoding.

Related: [MD5](md5.md), [JSON](json.md), [HTTP server](http-server.md).
