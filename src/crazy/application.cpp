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
#include "crazy/daemon.h"
#include "crazy/date_time.h"
#include "crazy/logger.h"
#include "crazy/version.h"

namespace crazy {
	Application* Application::s_application = nullptr;

	Application::Application(int32_t argc, char** argv)
		: ActorInterface(PathUtil::RemoveFileExtension(PathUtil::GetExecutableName())), argc_(argc), argv_(argv)
		, commandService_(std::make_shared<LocalSocket>())
		, commandClient_(std::make_shared<LocalSocket>()) {
		initSystem();
		s_application = this;
	}
	Application::~Application() {
		if (s_application == this) {
			s_application = nullptr;
		}
#ifdef _WIN32
		WSACleanup();
#endif
	}
	Application* Application::application() {
		return s_application;
	}
	ClickHouseConnectionPool::ptr Application::getClickHouseConnectionPool() {
		if (!clickhouseConnectionPool_) {
			throw std::logic_error("clickhouse connection pool is't create");
		}
		return clickhouseConnectionPool_;
	}
	MySQLConnectionPool::ptr Application::getMySQLConnectionPool() {
		if (!mysqlConnectionPool_) {
			throw std::logic_error("mysql connection pool is't create");
		}
		return mysqlConnectionPool_;
	}
	void Application::exec() {
		auto daemonArguments = Daemon::ParseArguments(argc_, argv_);
		if (daemonArguments.role == DaemonRole::supervisor) {
			exit(Daemon::RunSupervisor(daemonArguments.childArgs));
		}
		if (daemonArguments.enabled && daemonArguments.role == DaemonRole::none) {
			{
				FileLock daemonLock(PathUtil::GetExecutableName() + ".daemon.lock");
				if (!daemonLock.lock()) {
					CRAZY_SYSTEM_ERROR() << "the daemon supervisor has been launched. please do not start it again";
					exit(0);
				}
			}
			{
				FileLock applicationLock(PathUtil::GetExecutableName() + ".lock");
				if (!applicationLock.lock()) {
					CRAZY_SYSTEM_ERROR() << "the application has been launched. please do not start it again";
					exit(0);
				}
			}
			if (!Daemon::StartSupervisor(daemonArguments.childArgs)) {
				CRAZY_SYSTEM_ERROR() << "start daemon supervisor failed";
				exit(1);
			}
			CRAZY_SYSTEM_INFO() << "daemon supervisor started";
			exit(0);
		}

		for (const auto& [name, actor] : actors_) {
			auto helps = actor->helps();
			if (!helps.empty()) {
				helps_[name] = std::move(helps);
			}
		}
		helps_[name_] = helps();

		commandLineParser_.add<std::string>("send", 's', "send command" + commandLineHelp(), false);
		commandLineParser_.add("daemon", 'd', "run as daemon");
		commandLineParser_.parse_check(daemonArguments.applicationArgs);

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
		
		if (Config::HasSection("MySQL")) {
			MySQLConnectionPoolConfig mysqlConfig;
			mysqlConfig.host = Config::GetString("MySQL", "host");
			mysqlConfig.user = Config::GetString("MySQL", "user");
			mysqlConfig.password = Config::GetString("MySQL", "password");
			mysqlConfig.port = Config::GetInteger("MySQL", "port");
			mysqlConfig.min_connections = Config::GetInteger("MySQL", "min_connections", 5);
			mysqlConfig.max_connections = Config::GetInteger("MySQL", "max_connections", 20);
			mysqlConfig.max_idle_time = Config::GetInteger("MySQL", "max_idle_time", 300);
			mysqlConfig.max_wait_time = Config::GetInteger("MySQL", "max_wait_time", 30);
			mysqlConfig.connection_timeout = Config::GetInteger("MySQL", "connection_timeout", 10);
			mysqlConnectionPool_ = std::make_shared<MySQLConnectionPool>(mysqlConfig);
			mysqlConnectionPool_->start();
		}
		if (Config::HasSection("ClickHouse")) {
			ClickHouseConnectionPoolConfig clickhouseConfig;
			clickhouseConfig.host = Config::GetString("ClickHouse", "host");
			clickhouseConfig.user = Config::GetString("ClickHouse", "user");
			clickhouseConfig.password = Config::GetString("ClickHouse", "password");
			clickhouseConfig.port = Config::GetInteger("ClickHouse", "port");
			clickhouseConfig.min_connections = Config::GetInteger("ClickHouse", "min_connections", 5);
			clickhouseConfig.max_connections = Config::GetInteger("ClickHouse", "max_connections", 20);
			clickhouseConfig.max_idle_time = Config::GetInteger("ClickHouse", "max_idle_time", 300);
			clickhouseConfig.max_wait_time = Config::GetInteger("ClickHouse", "max_wait_time", 30);
			clickhouseConfig.connection_timeout = Config::GetInteger("ClickHouse", "connection_timeout", 10);
			clickhouseConnectionPool_ = std::make_shared<ClickHouseConnectionPool>(clickhouseConfig);
			clickhouseConnectionPool_->start();
		}
		
		auto threadPoolCount = Config::GetInteger("global", "thread_pool_count", std::thread::hardware_concurrency());
		threadPool_ = std::make_shared<ThreadPool>(threadPoolCount < 1 ? 1 : static_cast<uint32_t>(threadPoolCount));
		threadPool_->start();
		actors_[name_] = ActorInterface::ptr(this, [](ActorInterface*) {});
		for (const auto& [actorName, actorPtr] : actors_) {
			addRouteTable(name_, actorName, InternalCommand::command_line_request);
			addRouteTable(actorName, name_, InternalCommand::command_line_response);
			if (actorName != name_) {
				actorPtr->init();
				actorPtr->start();
			}
		}

		if (!commandService_->listen(PathUtil::GetExecutableName() + ".command")) {
			CRAZY_SYSTEM_ERROR() << "listen command service failed";
			exit(1);
		}
		registerEvent(commandService_->socket(), SelectorEventType::read, std::bind(&Application::acceptCommandClient, this));

		startServer();
		ActorInterface::run();
	}
	std::map<std::string, std::string> Application::helps() {
		return { 
			{"restart", "重启应用"},
			{"shutdown", "关闭应用"},
			{"time", "系统时间"},
			{"info", "框架信息"},
		};
	}
	void Application::stopService(int32_t exitCode) {
		if (threadPool_) {
			threadPool_->stop();
		}
		for (auto& [_, actor] : actors_) {
			actor->stop();
		}
		if (mysqlConnectionPool_) {
			mysqlConnectionPool_->stop();
		}
		if (clickhouseConnectionPool_) {
			clickhouseConnectionPool_->stop();
		}
		exit(exitCode);
	}
	void Application::restartService() {
		registerTimer("restart_service", 1000, std::bind(&Application::stopService, this, 1));
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
	bool Application::dispatchCommandLine(const std::string& input, uint64_t sessionId, const std::string& requestId, std::string* error) {
		auto recvMessage = StringUtil::Trim(input);
		if (recvMessage.empty()) {
			if (error) {
				*error = "command error, command is empty.";
			}
			return false;
		}

		auto pos = recvMessage.find_first_of("@");
		std::string actorName;
		std::string command;
		if (std::string::npos == pos) {
			actorName = name_;
			command = recvMessage;
		}
		else if (pos == 0 || pos == recvMessage.length() - 1) {
			if (error) {
				*error = "command error, please check command format.";
			}
			return false;
		}
		else {
			actorName = recvMessage.substr(0, pos);
			command = recvMessage.substr(pos + 1);
		}

		auto it = actors_.find(actorName);
		if (it == actors_.end()) {
			if (error) {
				*error = "command error, please actor name.";
			}
			return false;
		}

		auto message = std::make_shared<MessageBase>();
		message->setCmd(InternalCommand::command_line_request);
		message->setSessionId(sessionId);
		message->setComment(requestId);
		message->setData(command);
		it->second->enqueueMessage(message);
		return true;
	}
	std::string Application::commandLineHelp() {
		std::stringstream ss;
		ss << "\n";
		for (const auto& [actorName, helpMap] : helps_) {
			ss << "\t\t" << actorName << ":\n";
			for (const auto& [cmd, desc] : helpMap) {
				ss << "\t\t\t" << cmd << "\t" << desc << "\n";
			}
		}
		return ss.str();
	}
	std::vector<std::string> Application::actorNames() {
		std::vector<std::string> names;
		names.reserve(actors_.size());
		for (const auto& [actorName, _] : actors_) {
			names.push_back(actorName);
		}
		return names;
	}

	void Application::initSystem() {
#ifdef _WIN32
		SetConsoleOutputCP(65001);
		SetConsoleCP(65001);
		char szFileName[_MAX_PATH], szFilePath[_MAX_PATH];
		char* pcName;
		::GetModuleFileName(0, szFileName, _MAX_PATH);
		if (::GetFullPathName(szFileName, _MAX_PATH, szFilePath, &pcName) != 0) {
			*pcName = '\0';
			SetCurrentDirectory(szFilePath);
		}

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
	void Application::enqueueRunnable(std::function<void()> runnable, int32_t threadIndex) {
		threadPool_->enqueueRunnable(std::move(runnable), threadIndex);
	}
	ActorInterface::ptr Application::getActorImplement() {
		return threadPool_->getActorImplement();
	}
	void Application::handleCommandLineMessageBase(MessageBase::ptr request, MessageBase::ptr response) {
		auto commands = crazy::StringUtil::Split(request->getData());
		if (commands.empty()) {
			return;
		}
		if ("shutdown" == commands[0]) {
			stopService();
		}
		else if ("restart" == commands[0]) {
			response->setData("application restarting.");
			restartService();
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
	void Application::handleMessageBase(MessageBase::ptr message) {
		if (message->getCmd() == InternalCommand::command_line_response) {
			if (message->getComment().empty() && commandClient_ && commandClient_->active()) {
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
					actor->enqueueMessage(message->clone());
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

		std::string error;
		if (!dispatchCommandLine(recvMessage, 0, "", &error)) {
			commandClient_->send(error.data(), error.size());
			unregisterTimer("recv_command_timeout");
			cancelEvent(commandClient_->socket());
			commandClient_->close();
			return;
		}
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
	void Application::startServer() {
	}
}
