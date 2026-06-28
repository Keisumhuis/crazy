#include "crazy/application.h"

#include <signal.h>
#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <iostream>
#include <utility>

#include "crazy/ascii_logo.h"
#include "crazy/common.h"
#include "crazy/config.h"
#include "crazy/date_time.h"
#include "crazy/logger.h"
#include "crazy/version.h"

namespace crazy {
	Application* Application::s_application = nullptr;

	Application::Application(int32_t argc, char** argv)
		: ActorInterface("master"), argc_(argc), argv_(argv)
		, commandService_(std::make_shared<LocalSocket>())
		, commandClient_(std::make_shared<LocalSocket>()) {
		initSystem();
		s_application = this;
	}

	Application::~Application() {
#ifdef _WIN32
		WSACleanup();
#endif
	}

	Application* Application::application() {
		return s_application;
	}
	MySQLConnectionPool::ptr Application::getMySQLConnectionPool() {
		if (!mysqlConnectionPool_) {
			throw std::logic_error("mysql connection pool is't create");
		}
		return mysqlConnectionPool_;
	}
	void Application::exec() {
		for (const auto& [name, actor] : actors_) {
			auto helps = actor->helps();
			if (!helps.empty()) {
				helps_[name] = std::move(helps);
			}
		}
		helps_[name_] = helps();

		std::stringstream ss;
		ss << "\n";
		for (const auto& [actorName, helpMap] : helps_) {
			ss << "\t\t" << actorName << ":\n";
			for (const auto& [cmd, desc] : helpMap) {
				ss << "\t\t\t" << cmd << "\t" << desc << "\n";
			}
		}

		commandLineParser_.add<std::string>("send", 's', "send command" + ss.str(), false);
		commandLineParser_.parse_check(argc_, argv_);

		if (commandLineParser_.exist("send")) {
			if (!commandClient_->connect(PathUtil::GetExecutableName() + ".command")) {
				CRAZY_SYSTEM_ERROR() << "service has not been started";
				exit(0);
			}
			auto command = commandLineParser_.get<std::string>("send");
			auto rt = commandClient_->send(command.data(), command.size());
			if (rt <= 0) {
				CRAZY_SYSTEM_ERROR() << "service closed connect";
			}

			Buffer buffer(8 * 1024 * 1024);
			rt = commandClient_->recv(buffer.writeBegin(), buffer.writableCount());
			if (rt <= 0) {
				CRAZY_SYSTEM_ERROR() << "service closed connect";
			}
			else {
				buffer.written(rt);
				std::cout << std::string(buffer.readBegin(), buffer.readableCount()) << std::endl;
			}
			exit(0);
		}

		appLock_.setFilePath(PathUtil::GetExecutableName() + ".lock");
		if (!appLock_.lock()) {
			CRAZY_SYSTEM_ERROR() << "the application has been launched. please do not start it again";
			exit(0);
		}
		
		if (Config::HasSession("MySQL")) {
			MySQLConnectionPoolConfig mysqlConfig;
			mysqlConfig.host = Config::GetString("MySQL", "host");
			mysqlConfig.user = Config::GetString("MySQL", "user");
			mysqlConfig.password = Config::GetString("MySQL", "password");
			mysqlConfig.port = Config::GetIntager("MySQL", "port");
			mysqlConnectionPool_ = std::make_shared<MySQLConnectionPool>(mysqlConfig);
			mysqlConnectionPool_->start();
		}
		
		threadPool_ = std::make_shared<ThreadPool>(Config::GetIntager("global", "thread_pool_count", std::thread::hardware_concurrency()));
		threadPool_->start();
		actors_[name_] = ActorInterface::ptr(this);
		for (const auto& [actorName, actorPtr] : actors_) {
			addRouteTable(name_, actorName, InternalCommand::command_line_request);
			addRouteTable(actorName, name_, InternalCommand::command_line_response);
			if (actorName != name_) {
				actorPtr->init();
				actorPtr->start();
			}
		}

		commandService_->listen(PathUtil::GetExecutableName() + ".command");
		registerEvent(commandService_->socket(), SelectorEventType::read, std::bind(&Application::acceptCommandClient, this));

		ActorInterface::run();
	}

	std::map<std::string, std::string> Application::helps() {
		return { 
			{"shutdown", "关闭应用"},
			{"time", "系统时间"},
			{"info", "框架信息"},
		};
	}

	void Application::stopService() {
		threadPool_->stop();
		for (auto& [_, actor] : actors_) {
			actor->stop();
		}
		if (mysqlConnectionPool_) {
			mysqlConnectionPool_->stop();
		}
		exit(0);
	}

	void Application::registerActor(ActorInterface::ptr actor) {
		const auto& name = actor->getName();
		if (Config::GetBoolean("global", name, true)) {
			if (actors_.count(name)) {
				CRAZY_SYSTEM_ERROR() << "actor is exist, name = " << name;
				return;
			}
			actors_[name] = std::move(actor);
		}
	}

	void Application::initSystem() {
#ifdef _WIN32
		SetConsoleOutputCP(65001);
		SetConsoleCP(65001);
		char szFileName[_MAX_PATH], szFilePath[_MAX_PATH];
		char* pcName;
		::GetModuleFileName(0, szFileName, _MAX_PATH);
		::GetFullPathName(szFileName, _MAX_PATH, szFilePath, &pcName);
		char szBuf[_MAX_PATH];
		strcpy(szBuf, pcName);
		*pcName = '\0';
		SetCurrentDirectory(szFilePath);

		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			throw std::runtime_error("WSAStartup failed");
		}
#endif
		srand(time(nullptr));
		Config::LoadConfigPath("./");
	}
	void Application::addRouteTable(const std::string& from, const std::string& to, uint64_t cmd) {
		if (!actors_.count(from)) {
			return;
		}
		if (!actors_.count(to)) {
			return;
		}
		routeTable_[from][cmd].push_back(actors_[to]);
	}

	void Application::enqueueRunnable(std::function<void()> runnable) {
		threadPool_->enqueueRunnable(std::move(runnable));
	}

	ActorInterface::ptr Application::getActorImplement() {
		return threadPool_->getActorImplement();
	}

	void Application::handleCommandLineMessgaBase(MessageBase::ptr request, MessageBase::ptr response) {
		auto commands = crazy::StringUtil::Split(request->getData());
		if ("shutdown" == commands[0]) {
			stopService();
		}
		else if ("time" == commands[0]) {
			response->setData(crazy::DateTime().toString());
		}
		else if ("info" == commands[0]) {
			std::stringstream ss;
			ss << "git commit: " << G_GIT_COMMIT << "\n";
			ss << "git branch: " << G_GIT_BRANCH << "\n";
			response->setData(ss.str());
		}
	}

	void Application::handleMessgaBase(MessageBase::ptr message) {
		if (message->getCmd() == InternalCommand::command_line_response) {
			if (commandClient_ && commandClient_->active()) {
				auto& commandLineResponse = message->getData();
				commandClient_->send(commandLineResponse.data(), commandLineResponse.size());
				unregisterTimer("recv_command_timeout");
				cancelEvent(commandClient_->socket());
				commandClient_->close();
			}
		}
	}

	void Application::routeMessage(const std::string& from, MessageBase::ptr message) {
		auto itRoute = routeTable_.find(from);
		if (itRoute != routeTable_.end()) {
			auto itCmd = itRoute->second.find(message->getCmd());
			if (itCmd != itRoute->second.end()) {
				for (auto& actor : itCmd->second) {
					actor->enqueueMessage(message);
				}
			}
		}
	}

	void Application::acceptCommandClient() {
		commandClient_ = commandService_->accept();
		if (!commandClient_) {
			return;
		}
		registerEvent(commandClient_->socket(), SelectorEventType::read, std::bind(&Application::recvCommandClientMessage, this));
	}

	void Application::recvCommandClientMessage() {
		Buffer buffer(10240);
		auto rt = commandClient_->recv(buffer.writeBegin(), buffer.writableCount());
		if (rt <= 0) {
			CRAZY_SYSTEM_DEBUG() << "command client disconnected";
			commandClient_->close();
			unregisterTimer("recv_command_timeout");
			cancelEvent(commandClient_->socket());
			return;
		}
		registerTimer("recv_command_timeout", 10000, std::bind(&Application::recvCommandTimeout, this));
		buffer.written(rt);
		std::string recvMessage(buffer.readBegin(), buffer.readableCount());
		auto pos = recvMessage.find_first_of("@");
		if (std::string::npos == pos || pos == 0 || pos == recvMessage.length() - 1) {
			const char* err = "command error, please check command format.";
			commandClient_->send(err, strlen(err));
			unregisterTimer("recv_command_timeout");
			cancelEvent(commandClient_->socket());
			commandClient_->close();
			CRAZY_SYSTEM_DEBUG() << "command error: invalid format";
			return;
		}

		auto actorName = recvMessage.substr(0, pos);
		auto command = recvMessage.substr(pos + 1);

		auto it = actors_.find(actorName);
		if (it == actors_.end()) {
			const char* err = "command error, please actor name.";
			CRAZY_SYSTEM_DEBUG() << "actor name error";
			commandClient_->send(err, strlen(err));
			unregisterTimer("recv_command_timeout");
			cancelEvent(commandClient_->socket());
			commandClient_->close();
			return;
		}

		auto message = std::make_shared<MessageBase>();
		message->setCmd(InternalCommand::command_line_request);
		message->setData(command);
		it->second->enqueueMessage(message);
	}

	void Application::recvCommandTimeout() {
		if (!commandClient_) {
			return;
		}
		const std::string commandTimeOutTips = "recv from command service timeout, please try again later.";
		commandClient_->send(commandTimeOutTips.data(), commandTimeOutTips.size());
		unregisterTimer("recv_command_timeout");
		cancelEvent(commandClient_->socket());
		commandClient_->close();
		CRAZY_SYSTEM_DEBUG() << "close command client, timeout";
	}
}
