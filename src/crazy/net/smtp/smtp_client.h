/**
 * @file smtp_client.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief SMTP 客户端
 * @version 0.1
 * @date 2026-09-07
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "crazy/net/socket.h"

namespace crazy {
	/**
	 * @brief SMTP 客户端。
	 * @details 当前仅实现明文 SMTP 会话、AUTH PLAIN / AUTH LOGIN 和纯文本邮件发送。
	 */
	class SmtpClient final {
	public:
		using ptr = std::shared_ptr<SmtpClient>;

		/**
		 * @brief 认证方式。
		 */
		enum class AuthType : uint8_t {
			none,
			plain,
			login,
		};

		/**
		 * @brief 邮件内容。
		 */
		struct MailMessage {
			//! 发件人地址
			std::string from;
			//! 收件人列表
			std::vector<std::string> to;
			//! 抄送列表
			std::vector<std::string> cc;
			//! 密送列表
			std::vector<std::string> bcc;
			//! 邮件主题
			std::string subject;
			//! 邮件正文
			std::string body;
			//! 内容类型
			std::string contentType = "text/plain; charset=UTF-8";
			//! 附加头部
			std::map<std::string, std::string> headers;
		};

		/**
		 * @brief SMTP 响应。
		 */
		struct Response {
			//! 状态码
			int code = 0;
			//! 响应文本
			std::string message;
		};

		SmtpClient();
		~SmtpClient();

		/**
		 * @brief 设置 EHLO/HELO 使用的客户端名称。
		 */
		void setClientName(const std::string& clientName);
		/**
		 * @brief 连接服务器并完成 greeting / hello。
		 */
		bool connect(const std::string& host, uint16_t port = 25);
		/**
		 * @brief 关闭连接。
		 */
		void close();
		/**
		 * @brief 判断 socket 是否仍然有效。
		 */
		bool isConnected() const;
		/**
		 * @brief 主动发送 EHLO，失败时回退到 HELO。
		 */
		bool hello();
		/**
		 * @brief 进行 SMTP 认证。
		 */
		bool login(const std::string& username, const std::string& password, AuthType authType = AuthType::login);
		/**
		 * @brief 发送完整邮件。
		 */
		bool sendMail(const MailMessage& message);
		/**
		 * @brief 发送纯文本邮件的便捷接口。
		 */
		bool sendMail(const std::string& from, const std::vector<std::string>& to, const std::string& subject, const std::string& body,
			const std::vector<std::string>& cc = {}, const std::vector<std::string>& bcc = {});
		/**
		 * @brief 组装 SMTP DATA 段内容。
		 */
		static std::string buildMailData(const MailMessage& message);

	private:
		/**
		 * @brief 解析地址并建立 TCP 连接。
		 */
		bool connectToHost(const std::string& host, uint16_t port);
		/**
		 * @brief 发送一行命令并附带 CRLF。
		 */
		bool sendLine(const std::string& line);
		/**
		 * @brief 确保把缓冲区全部发送出去。
		 */
		bool sendAll(const char* data, size_t size);
		/**
		 * @brief 从 socket 中读取一行响应。
		 */
		bool readLine(std::string& line);
		/**
		 * @brief 读取多行 SMTP 响应。
		 */
		Response readResponse();
		/**
		 * @brief 判断响应码是否命中预期集合。
		 */
		bool expectResponse(std::initializer_list<int> expectedCodes, Response* response = nullptr);
		/**
		 * @brief 发送命令并等待预期响应。
		 */
		bool sendCommand(const std::string& command, std::initializer_list<int> expectedCodes, Response* response = nullptr);
		/**
		 * @brief 去掉邮箱地址两侧的空白和尖括号。
		 */
		static std::string normalizeAddress(const std::string& address);
		/**
		 * @brief 将多个地址格式化成 RFC 风格列表。
		 */
		static std::string joinAddresses(const std::vector<std::string>& addresses);
		/**
		 * @brief 生成 Date 头部。
		 */
		static std::string buildDateHeader();
		/**
		 * @brief 对正文执行 SMTP dot-stuffing。
		 */
		static std::string dotStuffBody(const std::string& body);

	private:
		std::string clientName_ = "localhost";
		std::string host_;
		uint16_t port_ = 25;
		Socket socket_;
		std::string recvBuffer_;
	};
}  // namespace crazy
