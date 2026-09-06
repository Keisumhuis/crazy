/**
 * @file test_http_client.cc
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 客户端纯逻辑测试（form-data 构造）.
 * @version 0.1
 * @date 2026-09-06
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "crazy/net/http/http_client.h"

namespace {
	int failures = 0;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
	// ---- generateBoundary：边界字符串生成 ----
	const std::string boundary = crazy::HttpClient::generateBoundary();
	CHECK(boundary.rfind("----crazyBoundary", 0) == 0);
	CHECK(boundary.size() > 16);

	// ---- buildMultipartBody：form-data 请求体构造 ----
	std::vector<crazy::FormDataField> fields;
	fields.push_back({ "name", "alice" });
	fields.push_back({ "age", "13" });

	std::vector<crazy::FormDataFile> files;
	crazy::FormDataFile file;
	file.name = "file";
	file.filename = "a.txt";
	file.contentType = "text/plain";
	file.content = "hello";
	files.push_back(file);

	const std::string body = crazy::HttpClient::buildMultipartBody(fields, files, boundary);

	// 验证字段分隔与字段值
	CHECK(body.find("--" + boundary + "\r\n") != std::string::npos);
	CHECK(body.find("Content-Disposition: form-data; name=\"name\"\r\n\r\nalice\r\n") != std::string::npos);
	CHECK(body.find("Content-Disposition: form-data; name=\"age\"\r\n\r\n13\r\n") != std::string::npos);

	// 验证文件字段
	CHECK(body.find("Content-Disposition: form-data; name=\"file\"; filename=\"a.txt\"\r\n") != std::string::npos);
	CHECK(body.find("Content-Type: text/plain\r\n") != std::string::npos);
	CHECK(body.find("\r\n\r\nhello\r\n") != std::string::npos);

	// 验证结尾边界
	CHECK(body.find("--" + boundary + "--\r\n") != std::string::npos);

	if (failures == 0) {
		std::cout << "http client tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}

#undef CHECK
