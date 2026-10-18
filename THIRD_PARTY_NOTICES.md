# 第三方组件许可说明（THIRD_PARTY_NOTICES）

本文件记录随本仓库一同分发的第三方组件及其许可，用于满足这些组件的署名与许可文本保留要求。
本仓库自身的许可见 [LICENSE](LICENSE)（MIT）。

---

## OpenSSL 3.3.0（已 vendor 进仓库）

| 项 | 值 |
|---|---|
| 组件 | OpenSSL |
| 版本 | **3.3.0**（`OpenSSL 3.3.0 9 Apr 2024`，取自 `src/third_party/openssl/include/openssl/opensslv.h`） |
| 仓库内位置 | `src/third_party/openssl/`（相对仓库根目录） |
| 内容 | 374 个文件；`include/{openssl,crypto,internal}` + `lib/{linux,windows}/{x86,x64}/{debug,release}` 共 16 个静态库 |
| 上游许可 | **Apache License 2.0** |
| 版权 | `Copyright 1995-2024 The OpenSSL Project Authors. All Rights Reserved.`；部分文件另含 `Copyright (c) 2002, Oracle and/or its affiliates.` |
| 使用方式 | **默认构建即通过 `crazy` 静态链接 `ssl` + `crypto`**（`CRAZY_ENABLE_TLS` 默认 `ON`，OpenSSL 已 vendor 进本仓库）；可用 `-DCRAZY_ENABLE_TLS=OFF` 显式关闭 |

### 该副本内不含任何许可文件（已核实）

`src/third_party/openssl/` 是 OpenSSL 3.3.0 的**二进制发行副本**，经核实其内部
**不包含任何 `LICENSE` / `COPYING` / `NOTICE` 文件，也不含任何 `.txt` / `.md` 文件**
（递归检索 374 个文件，命中 0）。也就是说，许可文本并未随这份副本一起进入本仓库。

因此：**任何分发本仓库（或分发链接了它的产物）的一方，必须随发行一并提供下面的
Apache License 2.0 完整文本**，以及上面的版权与版本信息。本节以下的许可文本即为该要求的落地。

### 兼容性说明

Apache-2.0 与本仓库根目录的 MIT 许可兼容：MIT 项目可以链接并分发 Apache-2.0 组件，
只要保留 Apache-2.0 的许可文本、版权声明与变更声明。本仓库**未修改** OpenSSL 副本的任何内容。

---

## CA bundle（随框架分发的根证书集合，已 vendor 进仓库）

| 项 | 值 |
|---|---|
| 组件 | CA 根证书 bundle（PEM 文本，不是代码） |
| 仓库内位置 | `src/third_party/ca/cacert.pem` |
| 大小 / 内容 | **188900 字节**，**121 张证书**（按 `-----BEGIN CERTIFICATE-----` 计数） |
| 来源 URL | `https://curl.se/ca/cacert.pem`（说明页 `https://curl.se/docs/caextract.html`） |
| 上游性质 | **Mozilla CA extract**（Mozilla 根证书库 `certdata.txt` 的转换产物，由 `mk-ca-bundle.pl` 1.33 生成） |
| bundle 头部自述的数据日期 | **Fri Sep 25 03:12:01 2026 GMT** |
| SHA256（本仓库副本） | `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505` |
| SHA256 校验值来源 | `https://curl.se/ca/cacert.pem.sha256`（上游公布的同一值；已逐位比对一致） |
| 获取日期 | **2026-10-10**（本机经自带信任库的 OpenSSL 客户端直连 `https://curl.se` 下载，并与上游 `.sha256` 比对通过） |
| 上游许可 | **MPL 2.0**（curl.se 明确说明：该 PEM 只是 Mozilla 源文件的转换版本，因此沿用源文件的许可） |
| 独立许可文件 | **不含**：该 PEM 内没有任何独立的 LICENSE / COPYING / NOTICE 文件；许可条款取自上表的上游声明 |
| 使用方式 | **默认构建即被编译进 `crazy` 静态库**（CMake 在 configure 期读取该 PEM，生成源码树内的 `src/crazy/net/ca_bundle.cpp`，该文件随仓库提交并作为普通源文件编译），作为 HTTPS/WSS **客户端**的默认信任库；服务端证书仍由 `[HTTP]` 节的 `tls_cert_file` / `tls_key_file` 指定 |

### 该 bundle 的再分发要求

这份 PEM 里的证书数据源自 Mozilla 的根证书库（`certdata.txt`），**许可为 MPL-2.0**。
它既不是本仓库的源代码，也不是本仓库的产物：若你的发行版随附（或编译进了）本仓库，
请**保留本节的上表信息与 MPL-2.0 声明**。仓库内该文件**未做任何修改**（逐字节等于上游下载结果）。

### 如何更新该 bundle

```bash
# 1) 取上游最新文件与其公布校验值（需要一个可用的 CA bundle 才能直连 https://curl.se）
curl -O https://curl.se/ca/cacert.pem
curl -O https://curl.se/ca/cacert.pem.sha256
# 2) 核对 SHA256（Windows：Get-FileHash cacert.pem -Algorithm SHA256）
sha256sum -c cacert.pem.sha256
# 3) 替换仓库副本并重新 configure（会重新生成 src/crazy/net/ca_bundle.cpp 并重新编译进库）
mv cacert.pem <repo>/src/third_party/ca/cacert.pem
cmake -S . -B build
```

若本机的系统 TLS 栈不可用（例如 Windows Schannel 损坏），可用任意自带 CA bundle 的 OpenSSL 客户端
（Python + `certifi`、`openssl s_client -CAfile <bundle>` 等）代替 `curl`；关键是**第 2 步的 SHA256 必须与上游公布值一致**。
也可用 `-DCRAZY_CA_BUNDLE_FILE=<path>` 在构建时指向别处的 PEM（此时请自行确认其来源与许可）。

---

## 在别处构建时指向自定义 OpenSSL

`CRAZY_OPENSSL_ROOT` 默认指向仓库内相对路径 `${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/openssl`。
若使用自备的 OpenSSL 包（只要含有 `include/` 与 `lib/{windows,linux}/{x86,x64}/{debug,release}/`），
可在 configure 时覆盖：

```bash
cmake -S . -B build/tls -DCRAZY_ENABLE_TLS=ON -DCRAZY_OPENSSL_ROOT=/path/to/openssl
```

此时该外部包的许可与署名责任同样由使用者承担；本文件仅覆盖仓库内的 vendored 副本。

---

# Apache License

Version 2.0, January 2004

http://www.apache.org/licenses/

TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

## 1. Definitions.

"License" shall mean the terms and conditions for use, reproduction, and distribution as defined by Sections 1 through 9 of this document.

"Licensor" shall mean the copyright owner or entity authorized by the copyright owner that is granting the License.

"Legal Entity" shall mean the union of the acting entity and all other entities that control, are controlled by, or are under common control with that entity. For the purposes of this definition, "control" means (i) the power, direct or indirect, to cause the direction or management of such entity, whether by contract or otherwise, or (ii) ownership of fifty percent (50%) or more of the outstanding shares, or (iii) beneficial ownership of such entity.

"You" (or "Your") shall mean an individual or Legal Entity exercising permissions granted by this License.

"Source" form shall mean the preferred form for making modifications, including but not limited to software source code, documentation source, and configuration files.

"Object" form shall mean any form resulting from mechanical transformation or translation of a Source form, including but not limited to compiled object code, generated documentation, and conversions to other media types.

"Work" shall mean the work of authorship, whether in Source or Object form, made available under the License, as indicated by a copyright notice that is included in or attached to the work (an example is provided in the Appendix below).

"Derivative Works" shall mean any work, whether in Source or Object form, that is based on (or derived from) the Work and for which the editorial revisions, annotations, elaborations, or other modifications represent, as a whole, an original work of authorship. For the purposes of this License, Derivative Works shall not include works that remain separable from, or merely link (or bind by name) to the interfaces of, the Work and Derivative Works thereof.

"Contribution" shall mean any work of authorship, including the original version of the Work and any modifications or additions to that Work or Derivative Works thereof, that is intentionally submitted to Licensor for inclusion in the Work by the copyright owner or by an individual or Legal Entity authorized to submit on behalf of the copyright owner. For the purposes of this definition, "submitted" means any form of electronic, verbal, or written communication sent to the Licensor or its representatives, including but not limited to communication on electronic mailing lists, source code control systems, and issue tracking systems that are managed by, or on behalf of, the Licensor for the purpose of discussing and improving the Work, but excluding communication that is conspicuously marked or otherwise designated in writing by the copyright owner as "Not a Contribution."

"Contributor" shall mean Licensor and any individual or Legal Entity on behalf of whom a Contribution has been received by Licensor and subsequently incorporated within the Work.

## 2. Grant of Copyright License.

Subject to the terms and conditions of this License, each Contributor hereby grants to You a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable copyright license to reproduce, prepare Derivative Works of, publicly display, publicly perform, sublicense, and distribute the Work and such Derivative Works in Source or Object form.

## 3. Grant of Patent License.

Subject to the terms and conditions of this License, each Contributor hereby grants to You a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable (except as stated in this section) patent license to make, have made, use, offer to sell, sell, import, and otherwise transfer the Work, where such license applies only to those patent claims licensable by such Contributor that are necessarily infringed by their Contribution(s) alone or by combination of their Contribution(s) with the Work to which such Contribution(s) was submitted. If You institute patent litigation against any entity (including a cross-claim or counterclaim in a lawsuit) alleging that the Work or a Contribution incorporated within the Work constitutes direct or contributory patent infringement, then any patent licenses granted to You under this License for that Work shall terminate as of the date such litigation is filed.

## 4. Redistribution.

You may reproduce and distribute copies of the Work or Derivative Works thereof in any medium, with or without modifications, and in Source or Object form, provided that You meet the following conditions:

1. You must give any other recipients of the Work or Derivative Works a copy of this License; and
2. You must cause any modified files to carry prominent notices stating that You changed the files; and
3. You must retain, in the Source form of any Derivative Works that You distribute, all copyright, patent, trademark, and attribution notices from the Source form of the Work, excluding those notices that do not pertain to any part of the Derivative Works; and
4. If the Work includes a "NOTICE" text file as part of its distribution, then any Derivative Works that You distribute must include a readable copy of the attribution notices contained within such NOTICE file, excluding those notices that do not pertain to any part of the Derivative Works, in at least one of the following places: within a NOTICE text file distributed as part of the Derivative Works; within the Source form or documentation, if provided along with the Derivative Works; or, within a display generated by the Derivative Works, if and wherever such third-party notices normally appear. The contents of the NOTICE file are for informational purposes only and do not modify the License. You may add Your own attribution notices within Derivative Works that You distribute, alongside or as an addendum to the NOTICE text from the Work, provided that such additional attribution notices cannot be construed as modifying the License.

You may add Your own copyright statement to Your modifications and may provide additional or different license terms and conditions for use, reproduction, or distribution of Your modifications, or for any such Derivative Works as a whole, provided Your use, reproduction, and distribution of the Work otherwise complies with the conditions stated in this License.

## 5. Submission of Contributions.

Unless You explicitly state otherwise, any Contribution intentionally submitted for inclusion in the Work by You to the Licensor shall be under the terms and conditions of this License, without any additional terms or conditions. Notwithstanding the above, nothing herein shall supersede or modify the terms of any separate license agreement you may have executed with Licensor regarding such Contributions.

## 6. Trademarks.

This License does not grant permission to use the trade names, trademarks, service marks, or product names of the Licensor, except as required for reasonable and customary use in describing the origin of the Work and reproducing the content of the NOTICE file.

## 7. Disclaimer of Warranty.

Unless required by applicable law or agreed to in writing, Licensor provides the Work (and each Contributor provides its Contributions) on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied, including, without limitation, any warranties or conditions of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A PARTICULAR PURPOSE. You are solely responsible for determining the appropriateness of using or redistributing the Work and assume any risks associated with Your exercise of permissions under this License.

## 8. Limitation of Liability.

In no event and under no legal theory, whether in tort (including negligence), contract, or otherwise, unless required by applicable law (such as deliberate and grossly negligent acts) or agreed to in writing, shall any Contributor be liable to You for damages, including any direct, indirect, special, incidental, or consequential damages of any character arising as a result of this License or out of the use or inability to use the Work (including but not limited to damages for loss of goodwill, work stoppage, computer failure or malfunction, or any and all other commercial damages or losses), even if such Contributor has been advised of the possibility of such damages.

## 9. Accepting Warranty or Additional Liability.

While redistributing the Work or Derivative Works thereof, You may choose to offer, and charge a fee for, acceptance of support, warranty, indemnity, or other liability obligations and/or rights consistent with this License. However, in accepting such obligations, You may act only on Your own behalf and on Your sole responsibility, not on behalf of any other Contributor, and only if You agree to indemnify, defend, and hold each Contributor harmless for any liability incurred by, or claims asserted against, such Contributor by reason of your accepting any such warranty or additional liability.

END OF TERMS AND CONDITIONS

## APPENDIX: How to apply the Apache License to your work.

To apply the Apache License to your work, attach the following boilerplate notice, with the fields enclosed by brackets "[]" replaced with your own identifying information. (Don't include the brackets!) The text should be enclosed in the appropriate comment syntax for the file format. We also recommend that a file or class name and description of purpose be included on the same "printed page" as the copyright notice for easier identification within third-party archives.

```text
Copyright [yyyy] [name of copyright owner]

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
