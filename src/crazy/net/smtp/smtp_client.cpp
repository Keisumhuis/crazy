#include "crazy/net/smtp/smtp_client.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "crazy/encryption/base64.h"
#include "crazy/logger.h"

namespace crazy {
	namespace {
		// 本地时间，用于 Date 头。
		std::tm LocalTime(std::time_t time) {
			std::tm tm = {};
#ifdef _WIN32
			localtime_s(&tm, &time);
#else
			localtime_r(&time, &tm);
#endif
			return tm;
		}

		// UTC 时间，用于计算本地时区偏移。
		std::tm UtcTime(std::time_t time) {
			std::tm tm = {};
#ifdef _WIN32
			gmtime_s(&tm, &time);
#else
			gmtime_r(&time, &tm);
#endif
			return tm;
		}
	}

	SmtpClient::SmtpClient() = default;

	SmtpClient::~SmtpClient() {
		close();
	}

	void SmtpClient::setClientName(const std::string& clientName) {
		clientName_ = clientName.empty() ? "localhost" : clientName;
	}

	bool SmtpClient::connect(const std::string& host, uint16_t port) {
		close();
		host_ = host;
		port_ = port;
		if (!connectToHost(host, port)) {
			return false;
		}

		// 服务器在连接成功后会先发送 220 greeting。
		auto greeting = readResponse();
		if (greeting.code != 220) {
			CRAZY_SYSTEM_ERROR() << "smtp greeting fail, code = " << greeting.code
				<< ", message = " << greeting.message;
			close();
			return false;
		}

		return hello();
	}

	void SmtpClient::close() {
		socket_.close();
		recvBuffer_.clear();
	}

	bool SmtpClient::isConnected() const {
		return socket_.active();
	}

	bool SmtpClient::hello() {
		if (!isConnected()) {
			return false;
		}

		// 优先 EHLO，失败时回退 HELO。
		Response response;
		if (sendCommand("EHLO " + clientName_, { 250 }, &response)) {
			return true;
		}

		if (response.code != 500 && response.code != 501 && response.code != 502 && response.code != 504 && response.code != 550) {
			return false;
		}
		return sendCommand("HELO " + clientName_, { 250 });
	}

	bool SmtpClient::login(const std::string& username, const std::string& password, AuthType authType) {
		if (!isConnected()) {
			return false;
		}
		if (authType == AuthType::none) {
			return true;
		}

		if (authType == AuthType::plain) {
			// AUTH PLAIN 的格式是: \0username\0password。
			const std::string raw = std::string(1, '\0') + username + std::string(1, '\0') + password;
			return sendCommand("AUTH PLAIN " + Base64::encryption(raw), { 235 });
		}

		// AUTH LOGIN 需要先发送用户名，再发送密码。
		if (!sendCommand("AUTH LOGIN", { 334 })) {
			return false;
		}
		if (!sendCommand(Base64::encryption(username), { 334 })) {
			return false;
		}
		return sendCommand(Base64::encryption(password), { 235 });
	}

	bool SmtpClient::sendMail(const MailMessage& message) {
		if (!isConnected()) {
			return false;
		}
		if (message.from.empty() || message.to.empty()) {
			return false;
		}

		// 标准 SMTP 投递顺序: MAIL FROM -> RCPT TO -> DATA。
		if (!sendCommand("MAIL FROM:<" + normalizeAddress(message.from) + ">", { 250 })) {
			return false;
		}

		const auto sendRecipient = [this](const std::string& address) {
			return sendCommand("RCPT TO:<" + normalizeAddress(address) + ">", { 250, 251, 252 });
		};

		for (const auto& address : message.to) {
			if (!sendRecipient(address)) {
				return false;
			}
		}
		for (const auto& address : message.cc) {
			if (!sendRecipient(address)) {
				return false;
			}
		}
		for (const auto& address : message.bcc) {
			if (!sendRecipient(address)) {
				return false;
			}
		}

		if (!sendCommand("DATA", { 354 })) {
			return false;
		}

		// DATA 阶段先发邮件头和正文，再用单独一行 "." 结束。
		const std::string data = buildMailData(message);
		if (!sendAll(data.data(), data.size())) {
			return false;
		}
		if (!sendLine(".")) {
			return false;
		}
		return expectResponse({ 250 });
	}

	bool SmtpClient::sendMail(const std::string& from, const std::vector<std::string>& to, const std::string& subject, const std::string& body,
		const std::vector<std::string>& cc, const std::vector<std::string>& bcc) {
		MailMessage message;
		message.from = from;
		message.to = to;
		message.cc = cc;
		message.bcc = bcc;
		message.subject = subject;
		message.body = body;
		return sendMail(message);
	}

	std::string SmtpClient::buildMailData(const MailMessage& message) {
		std::stringstream ss;

		// 常见邮件头部。
		ss << "Date: " << buildDateHeader() << "\r\n";
		ss << "From: <" << normalizeAddress(message.from) << ">\r\n";
		ss << "To: " << joinAddresses(message.to) << "\r\n";
		if (!message.cc.empty()) {
			ss << "Cc: " << joinAddresses(message.cc) << "\r\n";
		}
		ss << "Subject: " << message.subject << "\r\n";
		ss << "MIME-Version: 1.0\r\n";
		ss << "Content-Type: " << message.contentType << "\r\n";
		ss << "Content-Transfer-Encoding: 8bit\r\n";
		for (const auto& [key, value] : message.headers) {
			ss << key << ": " << value << "\r\n";
		}
		ss << "\r\n";

		// 正文需要做 dot-stuffing，防止误触 DATA 结束符。
		const std::string body = dotStuffBody(message.body);
		ss << body;
		if (body.size() < 2 || body.substr(body.size() - 2) != "\r\n") {
			ss << "\r\n";
		}
		return ss.str();
	}

	bool SmtpClient::connectToHost(const std::string& host, uint16_t port) {
		return socket_.connect(host, port);
	}

	bool SmtpClient::sendLine(const std::string& line) {
		const std::string data = line + "\r\n";
		return sendAll(data.data(), data.size());
	}

	bool SmtpClient::sendAll(const char* data, size_t size) {
		size_t sent = 0;
		while (sent < size) {
			const int32_t ret = socket_.send(data + sent, size - sent);
			if (ret <= 0) {
				CRAZY_SYSTEM_ERROR() << "smtp send fail, host = " << host_ << ", port = " << port_;
				socket_.close();
				return false;
			}
			sent += static_cast<size_t>(ret);
		}
		return true;
	}

	bool SmtpClient::readLine(std::string& line) {
		for (;;) {
			const auto pos = recvBuffer_.find("\r\n");
			if (pos != std::string::npos) {
				line = recvBuffer_.substr(0, pos);
				recvBuffer_.erase(0, pos + 2);
				return true;
			}

			char buffer[4096];
			const int32_t ret = socket_.recv(buffer, sizeof(buffer));
			if (ret <= 0) {
				socket_.close();
				return false;
			}
			recvBuffer_.append(buffer, static_cast<size_t>(ret));
		}
	}

	SmtpClient::Response SmtpClient::readResponse() {
		Response response;
		std::string line;
		while (readLine(line)) {
			if (line.size() < 3) {
				continue;
			}

			// 多行响应格式: 250-xxx 继续，250 xxx 结束。
			const int code = std::atoi(line.substr(0, 3).c_str());
			if (response.code == 0) {
				response.code = code;
			}
			if (line.size() > 4) {
				if (!response.message.empty()) {
					response.message += "\n";
				}
				response.message += line.substr(4);
			}
			if (line.size() >= 4 && line[3] == ' ') {
				break;
			}
		}
		return response;
	}

	bool SmtpClient::expectResponse(std::initializer_list<int> expectedCodes, Response* response) {
		auto resp = readResponse();
		if (response) {
			*response = resp;
		}
		if (std::find(expectedCodes.begin(), expectedCodes.end(), resp.code) == expectedCodes.end()) {
			std::stringstream expected;
			bool first = true;
			for (const auto code : expectedCodes) {
				if (!first) {
					expected << ", ";
				}
				first = false;
				expected << code;
			}
			CRAZY_SYSTEM_ERROR() << "smtp response unexpected, expect = " << expected.str()
				<< ", got = " << resp.code << ", message = " << resp.message;
			return false;
		}
		return true;
	}

	bool SmtpClient::sendCommand(const std::string& command, std::initializer_list<int> expectedCodes, Response* response) {
		if (!sendLine(command)) {
			return false;
		}
		return expectResponse(expectedCodes, response);
	}

	std::string SmtpClient::normalizeAddress(const std::string& address) {
		auto begin = address.find_first_not_of(" \t\r\n<");
		auto end = address.find_last_not_of(" \t\r\n>");
		if (begin == std::string::npos || end == std::string::npos || end < begin) {
			return address;
		}
		return address.substr(begin, end - begin + 1);
	}

	std::string SmtpClient::joinAddresses(const std::vector<std::string>& addresses) {
		std::stringstream ss;
		bool first = true;
		for (const auto& address : addresses) {
			if (!first) {
				ss << ", ";
			}
			first = false;
			ss << "<" << normalizeAddress(address) << ">";
		}
		return ss.str();
	}

	std::string SmtpClient::buildDateHeader() {
		const auto now = std::time(nullptr);
		auto localTm = LocalTime(now);
		auto utcTm = UtcTime(now);

		// 计算本地时区偏移，拼出 RFC 2822 风格的日期头。
		const auto localAsUtc = std::mktime(&localTm);
		const auto utcAsLocal = std::mktime(&utcTm);
		const auto offsetSeconds = static_cast<long>(std::difftime(localAsUtc, utcAsLocal));
		const auto offsetAbs = std::labs(offsetSeconds);
		const int offsetHours = static_cast<int>(offsetAbs / 3600);
		const int offsetMinutes = static_cast<int>((offsetAbs % 3600) / 60);
		const char offsetSign = offsetSeconds >= 0 ? '+' : '-';

		const char* weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
		const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
		char offset[8] = {};
		std::snprintf(offset, sizeof(offset), "%c%02d%02d", offsetSign, offsetHours, offsetMinutes);

		std::stringstream ss;
		ss << weekdays[localTm.tm_wday] << ", "
			<< std::setfill('0') << std::setw(2) << localTm.tm_mday << " "
			<< months[localTm.tm_mon] << " "
			<< (localTm.tm_year + 1900) << " "
			<< std::setw(2) << localTm.tm_hour << ":"
			<< std::setw(2) << localTm.tm_min << ":"
			<< std::setw(2) << localTm.tm_sec << " " << offset;
		return ss.str();
	}

	std::string SmtpClient::dotStuffBody(const std::string& body) {
		std::stringstream ss;
		size_t start = 0;
		while (start < body.size()) {
			const auto end = body.find('\n', start);
			std::string line = body.substr(start, end == std::string::npos ? std::string::npos : end - start);
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			if (!line.empty() && line.front() == '.') {
				ss << '.';
			}
			ss << line;
			if (end == std::string::npos) {
				break;
			}
			ss << "\r\n";
			start = end + 1;
		}
		return ss.str();
	}
}  // namespace crazy
