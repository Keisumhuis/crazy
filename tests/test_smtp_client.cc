/**
 * @file test_smtp_client.cc
 * @brief SMTP client pure logic test.
 */

#include <cstdlib>
#include <iostream>
#include <string>

#include "crazy/net/smtp/smtp_client.h"

namespace {
	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
	// 这里只测纯逻辑：邮件头、收件人列表和 dot-stuffing。
	crazy::SmtpClient::MailMessage message;
	message.from = "alice@example.com";
	message.to = { "bob@example.com" };
	message.cc = { "carol@example.com" };
	message.subject = "Hello";
	message.body = ".first line\nsecond line";
	message.headers["X-Test"] = "value";

	const std::string data = crazy::SmtpClient::buildMailData(message);

	CHECK(data.find("From: <alice@example.com>\r\n") != std::string::npos);
	CHECK(data.find("To: <bob@example.com>\r\n") != std::string::npos);
	CHECK(data.find("Cc: <carol@example.com>\r\n") != std::string::npos);
	CHECK(data.find("Subject: Hello\r\n") != std::string::npos);
	CHECK(data.find("X-Test: value\r\n") != std::string::npos);
	CHECK(data.find("\r\n..first line\r\nsecond line\r\n") != std::string::npos);

	std::cout << "smtp client tests passed" << std::endl;
	return 0;
}

#undef CHECK
