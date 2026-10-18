# 把 CA bundle 的 PEM 生成成 C++ 源文件（由 CMakeLists.txt 在 configure 期调用，也可手工执行）：
#   cmake -DCA_BUNDLE_PEM=<pem> -DCA_BUNDLE_SOURCE=<cpp> [-DCA_BUNDLE_LABEL=<label>] -P generate_ca_bundle.cmake
# 先按字节切分再逐块转义（不会在 \n / \" 中间切断），换行统一成 LF；内容没变就不重写。
cmake_minimum_required(VERSION 3.15)

if(NOT DEFINED CA_BUNDLE_PEM OR NOT DEFINED CA_BUNDLE_SOURCE)
	message(FATAL_ERROR "必须指定 -DCA_BUNDLE_PEM=<pem> 与 -DCA_BUNDLE_SOURCE=<cpp>")
endif()

if(NOT DEFINED CA_BUNDLE_LABEL)
	set(CA_BUNDLE_LABEL "${CA_BUNDLE_PEM}")
endif()
# 标签里的反斜杠统一成正斜杠
string(REPLACE "\\" "/" CA_BUNDLE_LABEL "${CA_BUNDLE_LABEL}")

# 每块字节数
set(CA_BUNDLE_CHUNK_SIZE 6000)

function(ca_bundle_escape INPUT OUTPUT)
	set(value "${INPUT}")
	string(REPLACE "\\" "\\\\" value "${value}")
	string(REPLACE "\"" "\\\"" value "${value}")
	string(REPLACE "\r" "\\r" value "${value}")
	string(REPLACE "\n" "\\n" value "${value}")
	string(REPLACE "\t" "\\t" value "${value}")
	set(${OUTPUT} "${value}" PARENT_SCOPE)
endfunction()

set(CA_BUNDLE_AVAILABLE FALSE)
set(CA_BUNDLE_BYTES 0)
set(CA_BUNDLE_CERT_COUNT 0)
if(EXISTS "${CA_BUNDLE_PEM}")
	file(READ "${CA_BUNDLE_PEM}" CA_BUNDLE_RAW)
	string(REPLACE "\r\n" "\n" CA_BUNDLE_RAW "${CA_BUNDLE_RAW}")
	string(LENGTH "${CA_BUNDLE_RAW}" CA_BUNDLE_BYTES)
	string(REGEX MATCHALL "-----BEGIN CERTIFICATE-----" CA_BUNDLE_CERTIFICATES "${CA_BUNDLE_RAW}")
	list(LENGTH CA_BUNDLE_CERTIFICATES CA_BUNDLE_CERT_COUNT)
	unset(CA_BUNDLE_CERTIFICATES)
	set(CA_BUNDLE_AVAILABLE TRUE)
endif()

ca_bundle_escape("${CA_BUNDLE_LABEL}" CA_BUNDLE_LABEL_ESCAPED)

set(CA_BUNDLE_CONTENT "")
string(APPEND CA_BUNDLE_CONTENT
	"// 由 generate_ca_bundle.cmake 生成，请勿手工修改。\n")
if(CA_BUNDLE_AVAILABLE)
	string(APPEND CA_BUNDLE_CONTENT
		"// 数据来源：${CA_BUNDLE_LABEL}（${CA_BUNDLE_BYTES} 字节 / ${CA_BUNDLE_CERT_COUNT} 张证书，换行统一为 LF）\n")
else()
	string(APPEND CA_BUNDLE_CONTENT
		"// 未找到 PEM（${CA_BUNDLE_LABEL}），本构建没有随包 CA bundle。\n")
endif()
string(APPEND CA_BUNDLE_CONTENT
	"// 重新生成：cmake -DCA_BUNDLE_PEM=<pem> -DCA_BUNDLE_SOURCE=<cpp> -P generate_ca_bundle.cmake\n"
	"#include <cstddef>\n"
	"#include <string>\n"
	"\n"
	"#include \"crazy/net/ca_bundle.h\"\n"
	"\n"
	"namespace crazy {\n")

if(CA_BUNDLE_AVAILABLE)
	string(APPEND CA_BUNDLE_CONTENT
		"\tnamespace {\n"
		"\t\t// PEM 分块（每块 ${CA_BUNDLE_CHUNK_SIZE} 字节）\n"
		"\t\tconst char* const kCaBundleChunks[] = {\n")
	if(CA_BUNDLE_BYTES EQUAL 0)
		string(APPEND CA_BUNDLE_CONTENT "\t\t\t\"\",\n")
	endif()
	set(CA_BUNDLE_OFFSET 0)
	while(CA_BUNDLE_OFFSET LESS CA_BUNDLE_BYTES)
		string(SUBSTRING "${CA_BUNDLE_RAW}" ${CA_BUNDLE_OFFSET} ${CA_BUNDLE_CHUNK_SIZE} CA_BUNDLE_CHUNK)
		string(LENGTH "${CA_BUNDLE_CHUNK}" CA_BUNDLE_CHUNK_LENGTH)
		math(EXPR CA_BUNDLE_OFFSET "${CA_BUNDLE_OFFSET} + ${CA_BUNDLE_CHUNK_LENGTH}")
		ca_bundle_escape("${CA_BUNDLE_CHUNK}" CA_BUNDLE_CHUNK_ESCAPED)
		string(APPEND CA_BUNDLE_CONTENT "\t\t\t\"${CA_BUNDLE_CHUNK_ESCAPED}\",\n")
	endwhile()
	unset(CA_BUNDLE_CHUNK)
	unset(CA_BUNDLE_CHUNK_ESCAPED)
	unset(CA_BUNDLE_RAW)
	string(APPEND CA_BUNDLE_CONTENT
		"\t\t};\n"
		"\t\tconst std::size_t kCaBundleChunkCount = sizeof(kCaBundleChunks) / sizeof(kCaBundleChunks[0]);\n"
		"\t}\n"
		"\tconst std::string& bundledCaPem() {\n"
		"\t\tstatic const std::string kPem = [] {\n"
		"\t\t\tstd::string value;\n"
		"\t\t\tfor (std::size_t index = 0; index < kCaBundleChunkCount; ++index) {\n"
		"\t\t\t\tvalue += kCaBundleChunks[index];\n"
		"\t\t\t}\n"
		"\t\t\treturn value;\n"
		"\t\t}();\n"
		"\t\treturn kPem;\n"
		"\t}\n"
		"\tstd::size_t bundledCaCertificateCount() {\n"
		"\t\treturn ${CA_BUNDLE_CERT_COUNT};\n"
		"\t}\n"
		"\tconst char* bundledCaSource() {\n"
		"\t\treturn \"${CA_BUNDLE_LABEL_ESCAPED}\";\n"
		"\t}\n")
else()
	string(APPEND CA_BUNDLE_CONTENT
		"\tconst std::string& bundledCaPem() {\n"
		"\t\tstatic const std::string kPem;\n"
		"\t\treturn kPem;\n"
		"\t}\n"
		"\tstd::size_t bundledCaCertificateCount() {\n"
		"\t\treturn 0;\n"
		"\t}\n"
		"\tconst char* bundledCaSource() {\n"
		"\t\treturn \"\";\n"
		"\t}\n")
endif()
string(APPEND CA_BUNDLE_CONTENT "}\n")

# 内容一致（忽略 CRLF/LF）就不重写
set(CA_BUNDLE_NEEDS_WRITE TRUE)
if(EXISTS "${CA_BUNDLE_SOURCE}")
	file(READ "${CA_BUNDLE_SOURCE}" CA_BUNDLE_EXISTING)
	string(REPLACE "\r\n" "\n" CA_BUNDLE_EXISTING "${CA_BUNDLE_EXISTING}")
	if("${CA_BUNDLE_EXISTING}" STREQUAL "${CA_BUNDLE_CONTENT}")
		set(CA_BUNDLE_NEEDS_WRITE FALSE)
	endif()
endif()
if(CA_BUNDLE_NEEDS_WRITE)
	file(WRITE "${CA_BUNDLE_SOURCE}" "${CA_BUNDLE_CONTENT}")
	set(CA_BUNDLE_ACTION "written")
else()
	set(CA_BUNDLE_ACTION "up to date")
endif()

if(CA_BUNDLE_AVAILABLE)
	message(STATUS "CA bundle: ${CA_BUNDLE_LABEL} -> ${CA_BUNDLE_SOURCE} (${CA_BUNDLE_CERT_COUNT} certificates, ${CA_BUNDLE_BYTES} bytes, ${CA_BUNDLE_ACTION})")
else()
	# 缺失只告警、不硬失败：显式设置的 SSL_CERT_FILE/SSL_CERT_DIR 仍然可用，
	# 但默认情况下公网 HTTPS 站点会因信任库为空而握手失败。
	message(WARNING
		"Bundled CA bundle is missing: ${CA_BUNDLE_PEM}\n"
		"  TLS clients will fall back to the OpenSSL default verify paths only\n"
		"  (SSL_CERT_FILE / SSL_CERT_DIR), so public HTTPS sites may fail with\n"
		"  'unable to get local issuer certificate'. Point -DCRAZY_CA_BUNDLE_FILE=<path> at a PEM bundle to restore it.")
endif()
