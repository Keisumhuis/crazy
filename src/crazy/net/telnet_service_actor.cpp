#include "crazy/net/telnet_service_actor.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#include "crazy/application.h"
#include "crazy/common.h"
#include "crazy/config.h"
#include "crazy/logger.h"
#include "crazy/utils.h"

namespace crazy {
	TelnetServiceActor::TelnetServiceActor(const std::string& name)
		: ActorInterface(name), acceptor_(std::make_shared<Socket>()) {
	}
	TelnetServiceActor::~TelnetServiceActor() {
		sessions_.clear();
	}
	std::map<std::string, std::string> TelnetServiceActor::helps() {
		return {};
	}
	void TelnetServiceActor::run() {
		initConfig();
		initService();
		ActorInterface::run();
	}
	void TelnetServiceActor::handleMessgaBase(MessageBase::ptr message) {
		if (message->getCmd() != InternalCommand::command_line_response) {
			return;
		}
		auto it = sessions_.find(message->getSessionId());
		if (it == sessions_.end()) {
			return;
		}
		auto session = it->second;
		if (!session->waitingResponse_ || session->commandRequestId_ != message->getComment()) {
			return;
		}
		session->waitingResponse_ = false;
		session->commandStartTimestamp_ = 0;
		session->commandRequestId_.clear();

		auto response = normalizeNewLine(message->getData());
		if (response.empty()) {
			response = "OK\r\n";
		}
		else if (response.size() < 2 || response.substr(response.size() - 2) != "\r\n") {
			response += "\r\n";
		}
		sendSession(message->getSessionId(), response);
		sendPrompt(message->getSessionId());
	}
	void TelnetServiceActor::initConfig() {
		config_.address_ = Config::GetString(name_, "address", "0.0.0.0");
		config_.port_ = static_cast<uint16_t>(Config::GetIntager(name_, "port", 2323));
		config_.max_sessions_ = static_cast<uint32_t>(Config::GetIntager(name_, "max_sessions", 16));
		config_.idle_timeout_ = static_cast<uint32_t>(Config::GetIntager(name_, "idle_timeout", 300000));
		config_.command_timeout_ = static_cast<uint32_t>(Config::GetIntager(name_, "command_timeout", 10000));
		config_.password_ = Config::GetString(name_, "password", "");
	}
	void TelnetServiceActor::initService() {
		if (!acceptor_->listen(config_.port_, config_.address_)) {
			CRAZY_SYSTEM_INFO() << "telnet service listening " << config_.address_ << ":" << config_.port_ << " fail";
			return;
		}
		CRAZY_SYSTEM_INFO() << "telnet service listening " << config_.address_ << ":" << config_.port_;
		initCommandRoutes();
		registerEvent(static_cast<int32_t>(acceptor_->socket()), SelectorEventType::read, std::bind(&TelnetServiceActor::onAccept, this));
		registerTimer("telnet_check_sessions", 1000, std::bind(&TelnetServiceActor::onCheckSessions, this));
	}
	void TelnetServiceActor::initCommandRoutes() {
		for (const auto& actorName : Application::application()->actorNames()) {
			if (actorName == name_) {
				continue;
			}
			Application::application()->addRouteTable(actorName, name_, InternalCommand::command_line_response);
		}
	}
	void TelnetServiceActor::onCheckSessions() {
		auto currentTimestamp = GetCurrentMS();
		for (auto it = sessions_.begin(); it != sessions_.end(); ) {
			auto session = it->second;
			if (config_.idle_timeout_ > 0 && currentTimestamp - session->lastActiveTimestamp_ > config_.idle_timeout_) {
				sendSession(session->sessionId_, "\r\nidle timeout.\r\n", true);
				++it;
				continue;
			}
			if (session->waitingResponse_ && config_.command_timeout_ > 0
				&& currentTimestamp - session->commandStartTimestamp_ > config_.command_timeout_) {
				session->waitingResponse_ = false;
				session->commandStartTimestamp_ = 0;
				session->commandRequestId_.clear();
				sendSession(session->sessionId_, "\r\ncommand timeout.\r\n");
				sendPrompt(session->sessionId_);
			}
			++it;
		}
	}
	void TelnetServiceActor::onAccept() {
		auto socket = acceptor_->accept();
		if (!socket) {
			return;
		}
		if (sessions_.size() >= config_.max_sessions_) {
			const std::string message = "too many telnet sessions.\r\n";
			socket->send(message.data(), message.size());
			socket->close();
			return;
		}

		auto session = std::make_shared<TelnetSession>();
		session->sessionId_ = genSessionId();
		session->socket_ = socket;
		session->lastActiveTimestamp_ = GetCurrentMS();
		session->authorized_ = config_.password_.empty();
		sessions_[session->sessionId_] = session;

		CRAZY_SYSTEM_INFO() << "new telnet client connected, remote address = " << socket->remoteAddress();
		registerEvent(static_cast<int32_t>(socket->socket()), SelectorEventType::read, std::bind(&TelnetServiceActor::onRead, this, session->sessionId_));

		std::stringstream ss;
		ss << "Crazy telnet service\r\n";
		if (session->authorized_) {
			ss << "type help for commands.\r\n";
		}
		else {
			ss << "password required, use: auth <password>\r\n";
		}
		sendSession(session->sessionId_, ss.str());
		sendPrompt(session->sessionId_);
	}
	void TelnetServiceActor::onRead(uint64_t sessionId) {
		auto it = sessions_.find(sessionId);
		if (it == sessions_.end()) {
			return;
		}
		auto session = it->second;
		char buffer[1024] = { 0 };
		auto rt = session->socket_->recv(buffer, sizeof(buffer));
		if (rt <= 0) {
			closeSession(sessionId);
			return;
		}
		session->lastActiveTimestamp_ = GetCurrentMS();
		for (int32_t i = 0; i < rt; ++i) {
			handleInputChar(sessionId, static_cast<unsigned char>(buffer[i]));
		}
	}
	void TelnetServiceActor::onWrite(uint64_t sessionId) {
		auto it = sessions_.find(sessionId);
		if (it == sessions_.end()) {
			return;
		}
		auto session = it->second;
		if (session->sendBuffer_.empty()) {
			unregisterEvent(static_cast<int32_t>(session->socket_->socket()), SelectorEventType::write);
			session->writeRegistered_ = false;
			if (session->closeAfterWrite_) {
				closeSession(sessionId);
			}
			return;
		}
		auto rt = session->socket_->send(session->sendBuffer_.data(), session->sendBuffer_.size());
		if (rt <= 0) {
			closeSession(sessionId);
			return;
		}
		session->sendBuffer_.erase(0, rt);
		if (session->sendBuffer_.empty()) {
			unregisterEvent(static_cast<int32_t>(session->socket_->socket()), SelectorEventType::write);
			session->writeRegistered_ = false;
			if (session->closeAfterWrite_) {
				closeSession(sessionId);
			}
		}
	}
	void TelnetServiceActor::closeSession(uint64_t sessionId) {
		auto it = sessions_.find(sessionId);
		if (it == sessions_.end()) {
			return;
		}
		auto session = it->second;
		cancelEvent(static_cast<int32_t>(session->socket_->socket()));
		session->socket_->close();
		CRAZY_SYSTEM_INFO() << "telnet client disconnected, remote address = " << session->socket_->remoteAddress();
		sessions_.erase(it);
	}
	void TelnetServiceActor::sendSession(uint64_t sessionId, const std::string& data, bool closeAfterWrite) {
		auto it = sessions_.find(sessionId);
		if (it == sessions_.end()) {
			return;
		}
		auto session = it->second;
		session->sendBuffer_ += data;
		session->closeAfterWrite_ = session->closeAfterWrite_ || closeAfterWrite;
		if (!session->writeRegistered_) {
			registerEvent(static_cast<int32_t>(session->socket_->socket()), SelectorEventType::write
				, std::bind(&TelnetServiceActor::onWrite, this, sessionId));
			session->writeRegistered_ = true;
		}
	}
	void TelnetServiceActor::handleInputChar(uint64_t sessionId, unsigned char ch) {
		auto it = sessions_.find(sessionId);
		if (it == sessions_.end()) {
			return;
		}
		auto session = it->second;
		if (session->telnetState_ == 1) {
			session->telnetState_ = (ch >= 251 && ch <= 254) ? 2 : 0;
			return;
		}
		if (session->telnetState_ == 2) {
			session->telnetState_ = 0;
			return;
		}
		if (ch == 255) {
			session->telnetState_ = 1;
			return;
		}
		if (ch == '\r') {
			session->ignoreNextLf_ = true;
			executeLine(sessionId);
			return;
		}
		if (ch == '\n') {
			if (session->ignoreNextLf_) {
				session->ignoreNextLf_ = false;
				return;
			}
			executeLine(sessionId);
			return;
		}
		session->ignoreNextLf_ = false;
		if (ch == 8 || ch == 127) {
			if (!session->lineBuffer_.empty()) {
				session->lineBuffer_.pop_back();
			}
			return;
		}
		if (ch >= 32 && ch != 127) {
			session->lineBuffer_.push_back(static_cast<char>(ch));
		}
	}
	void TelnetServiceActor::executeLine(uint64_t sessionId) {
		auto it = sessions_.find(sessionId);
		if (it == sessions_.end()) {
			return;
		}
		auto session = it->second;
		auto line = StringUtil::Trim(session->lineBuffer_);
		session->lineBuffer_.clear();
		if (line.empty()) {
			sendPrompt(sessionId);
			return;
		}

		auto commands = StringUtil::Split(line, " ");
		auto command = StringUtil::ToLower(commands.empty() ? "" : commands[0]);
		if (command == "quit" || command == "exit") {
			sendSession(sessionId, "bye.\r\n", true);
			return;
		}
		if (command == "help") {
			sendSession(sessionId, telnetHelp());
			sendPrompt(sessionId);
			return;
		}
		if (command == "actors") {
			sendSession(sessionId, actorList());
			sendPrompt(sessionId);
			return;
		}
		if (!session->authorized_) {
			if (command == "auth" && commands.size() >= 2 && commands[1] == config_.password_) {
				session->authorized_ = true;
				sendSession(sessionId, "auth ok.\r\n");
			}
			else {
				sendSession(sessionId, "auth failed.\r\n");
			}
			sendPrompt(sessionId);
			return;
		}
		if (session->waitingResponse_) {
			sendSession(sessionId, "previous command is still running.\r\n");
			sendPrompt(sessionId);
			return;
		}

		std::string error;
		auto requestId = genCommandLineRequestId(sessionId);
		if (!Application::application()->dispatchCommandLine(line, sessionId, requestId, &error)) {
			sendSession(sessionId, error + "\r\n");
			sendPrompt(sessionId);
			return;
		}
		session->waitingResponse_ = true;
		session->commandStartTimestamp_ = GetCurrentMS();
		session->commandRequestId_ = requestId;
	}
	void TelnetServiceActor::sendPrompt(uint64_t sessionId) {
		sendSession(sessionId, "crazy> ");
	}
	std::string TelnetServiceActor::telnetHelp() {
		std::stringstream ss;
		ss << "Telnet commands:\r\n";
		ss << "  help              print command help\r\n";
		ss << "  actors            print actor list\r\n";
		ss << "  quit              close current connection\r\n";
		if (!config_.password_.empty()) {
			ss << "  auth <password>   login telnet session\r\n";
		}
		ss << "\r\nCommand format:\r\n";
		ss << "  command\r\n";
		ss << "  actor@command\r\n";
		ss << normalizeNewLine(Application::application()->commandLineHelp());
		return ss.str();
	}
	std::string TelnetServiceActor::actorList() {
		auto names = Application::application()->actorNames();
		std::sort(names.begin(), names.end());
		std::stringstream ss;
		ss << "Actors:\r\n";
		for (const auto& name : names) {
			ss << "  " << name << "\r\n";
		}
		return ss.str();
	}
	std::string TelnetServiceActor::normalizeNewLine(const std::string& data) {
		std::string result;
		result.reserve(data.size() + 16);
		for (size_t i = 0; i < data.size(); ++i) {
			if (data[i] == '\r') {
				result.push_back('\r');
				if (i + 1 < data.size() && data[i + 1] == '\n') {
					result.push_back('\n');
					++i;
				}
				else {
					result.push_back('\n');
				}
			}
			else if (data[i] == '\n') {
				result += "\r\n";
			}
			else {
				result.push_back(data[i]);
			}
		}
		return result;
	}
	std::string TelnetServiceActor::genCommandLineRequestId(uint64_t sessionId) {
		if (commandLineRequestId_ >= UINT64_MAX) {
			commandLineRequestId_ = 0;
		}
		std::stringstream ss;
		ss << name_ << ":" << sessionId << ":" << ++commandLineRequestId_;
		return ss.str();
	}
	uint64_t TelnetServiceActor::genSessionId() {
		if (sessionId_ >= UINT64_MAX) {
			sessionId_ = 0;
		}
		return ++sessionId_;
	}
}
