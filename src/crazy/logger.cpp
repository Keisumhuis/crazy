#include "crazy/logger.h"

#include <iomanip>
#include <iostream>

#include "crazy/config.h"

namespace crazy {
	const char* GetLoggerLevelString(LoggerLevel level) {
		switch (level) {
#define XX(type, value, string) case LoggerLevel::type: return #string;
			LOGGER_LEVEL_MAP(XX)
#undef XX
		default: return "unknown";
		}
	}
	LoggerLevel GetLoggerLevelEnum(const std::string& level) {
#define XX(type, value, string) if (level == #string) return LoggerLevel::type;
		LOGGER_LEVEL_MAP(XX)
#undef XX
		return LoggerLevel::trace;
	}
	LoggerEvent::LoggerEvent(std::shared_ptr<Logger> logger, uint64_t timestamp, LoggerLevel level
		, const std::string& function, uint32_t line)
		: logger_(logger), timestamp_(timestamp), level_(level), function_(function), line_(line) {
	}
	std::shared_ptr<Logger> LoggerEvent::getLogger() const {
		return logger_;
	}
	void LoggerEvent::setLogger(const std::shared_ptr<Logger> logger) {
		logger_ = logger;
	}
	uint64_t LoggerEvent::getTimestamp() const {
		return timestamp_;
	}
	void LoggerEvent::setTimestamp(uint64_t timestamp) {
		timestamp_ = timestamp;
	}
	LoggerLevel LoggerEvent::getLevel() const {
		return level_;
	}
	void LoggerEvent::setLevel(LoggerLevel level) {
		level_ = level;
	}
	const std::string& LoggerEvent::getFunction() const {
		return function_;
	}
	void LoggerEvent::setFunction(const std::string& function) {
		function_ = function;
	}
	uint32_t LoggerEvent::getLine() const {
		return line_;
	}
	void LoggerEvent::setLine(uint32_t line) {
		line_ = line;
	}
	std::stringstream& LoggerEvent::getSS() {
		return ss_;
	}
	LoggerStringLiteralComponentImpl::LoggerStringLiteralComponentImpl(const std::string& stringliteral)
		: stringliteral_(stringliteral) {
	}
	void LoggerStringLiteralComponentImpl::log(std::ostream& os, const LoggerEvent::ptr event) {
		os << stringliteral_;
	}
	void LoggerDateTimeComponentImpl::log(std::ostream& os, const LoggerEvent::ptr event) {
		char buffer[80] = {};
		auto time = (time_t)event->getTimestamp() / 1000;
		auto timeinfo = localtime(&time);
		strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S.", timeinfo);
		os << buffer << std::setw(3) << std::setfill('0') << event->getTimestamp() % 1000;
	}
	void LoggerCategoryComponentImpl::log(std::ostream& os, const LoggerEvent::ptr event) {
		os << "[" << event->getLogger()->getName() << "]";
	}
	void LoggerPriorityComponentImpl::log(std::ostream& os, const LoggerEvent::ptr event) {
		os << "[" << GetLoggerLevelString(event->getLevel()) << "]";
	}
	void LoggerFunctionComponentImpl::log(std::ostream& os, const LoggerEvent::ptr event) {
		os << event->getFunction();
	}
	void LoggerLineComponentImpl::log(std::ostream& os, const LoggerEvent::ptr event) {
		os << event->getLine();
	}
	void LoggerMessageComponentImpl::log(std::ostream& os, const LoggerEvent::ptr event) {
		os << event->getSS().str();
	}
	LoggerFormatter::LoggerFormatter(const std::string& format)
		: format_(format) {
		parse();
	}
	const std::string& LoggerFormatter::getFormat() const {
		return format_;
	}
	void LoggerFormatter::setFormat(const std::string format) {
		if (format_ == format) {
			return;
		}
		parse();
	}
	std::string LoggerFormatter::log(const LoggerEvent::ptr event) {
		std::stringstream ss;
		for (auto& it : components_) {
			it->log(ss, event);
		}
		return ss.str();
	}
	void LoggerFormatter::parse() {
		std::string literal;
		char ch;
		LoggerComponentInterface::ptr component = nullptr;
		std::istringstream conversion_stream(format_);

		while (conversion_stream.get(ch)) {
			if ('%' == ch) {
				if (conversion_stream.get(ch)) {
					switch (ch) {
					case 'd':
						component = std::make_shared<LoggerDateTimeComponentImpl>();
						break;
					case 'c':
						component = std::make_shared<LoggerCategoryComponentImpl>();
						break;
					case 'p':
						component = std::make_shared<LoggerPriorityComponentImpl>();
						break;
					case 'f':
						component = std::make_shared<LoggerFunctionComponentImpl>();
						break;
					case 'l':
						component = std::make_shared<LoggerLineComponentImpl>();
						break;
					case 'm':
						component = std::make_shared<LoggerMessageComponentImpl>();
						break;
					default:
						literal += ch;
						component = nullptr;
						break;
					}
				} else {
					literal += '%';
					component = nullptr;
				}
				if (component) {
					if (!literal.empty()) {
						components_.push_back(std::make_shared<LoggerStringLiteralComponentImpl>(literal));
						literal.clear();
					}
					components_.push_back(component);
					component = nullptr;
				}
			} else {
				literal += ch;
			}
		}
		if (!literal.empty()) {
			components_.push_back(std::make_shared<LoggerStringLiteralComponentImpl>(literal));
		}
	}
	LoggerLevel LoggerAppenderInterface::getLevel() const {
		return level_;
	}
	void LoggerAppenderInterface::setLevel(LoggerLevel level) {
		level_ = level;
	}
	LoggerFormatter::ptr LoggerAppenderInterface::getFormatter() const {
		return formatter_;
	}
	void LoggerAppenderInterface::setFormatter(LoggerFormatter::ptr formatter) {
		formatter_ = formatter;
	}
	void LoggerConsoleAppenderImpl::log(LoggerEvent::ptr event) {
		CondMutexGuard guard(condMutex_);
		printf("%s\n", getFormatter()->log(event).c_str());
	}
	LoggerFileAppenderImpl::LoggerFileAppenderImpl(const std::string& logPath) {
		if (!PathUtil::PathExists(logPath)) {
			PathUtil::CreateDirectories(logPath);
		}
#define XX(type, value, string)												\
		{																	\
			LoggerFileConfig config;										\
			config.logFilePath_ = logPath;									\
			config.levelString = GetLoggerLevelString(LoggerLevel::type);	\
			fileLogConfig_[LoggerLevel::type] = config;						\
		}
		LOGGER_LEVEL_MAP(XX)
#undef XX
	}
	void LoggerFileAppenderImpl::log(LoggerEvent::ptr event) {
		CondMutexGuard guard(condMutex_);
		auto it = fileLogConfig_.find(event->getLevel());
		if (fileLogConfig_.end() == it) {
			return;
		}
		if (!it->second.file_) {
			reopen(&it->second, GetLoggerLevelString(event->getLevel()), event->getLogger()->getName());
		}
		
		if (!it->second.file_) {
			return;
		}

		auto logString = getFormatter()->log(event);
		fprintf(it->second.file_, "%s\n", logString.c_str());
		fflush(it->second.file_);
		it->second.currentLogFileSize_ += logString.size();
		if (it->second.currentLogFileSize_ > it->second.maxLogFileSize_) {
			reopen(&it->second, GetLoggerLevelString(event->getLevel()), event->getLogger()->getName());
		}
	}
	void LoggerFileAppenderImpl::setPath(const std::string& logPath) {
		for (auto& it : fileLogConfig_) {
			it.second.logFilePath_ = logPath;
		}
	}
	void LoggerFileAppenderImpl::setMaxSize(const uint32_t size) {
		for (auto& it : fileLogConfig_) {
			it.second.maxLogFileSize_ = size;
		}
	}
	void LoggerFileAppenderImpl::reopen(LoggerFileConfig* config, const std::string& levelString, const std::string& loggerName) {

		std::stringstream ss;
		char buffer[80] = {};
		auto time = (time_t)GetCurrentMS() / 1000;
		auto timeinfo = localtime(&time);
		strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S.", timeinfo);
		ss << buffer << std::setw(3) << std::setfill('0') << GetCurrentMS() % 1000;

		std::string oldFilePath = config->logFilePath_ + "/" + loggerName + "_" + levelString + ".log";
		std::string newFilePath = config->logFilePath_ + "/" + loggerName + "_" + levelString + "_" + ss.str() + ".log";
		if (config->file_) {
			fclose(config->file_);
			config->file_ = nullptr;
			rename(oldFilePath.data(), newFilePath.data());
		}
		config->file_ = fopen(oldFilePath.data(), "a+");
		if (!config->file_) {
			return;
		}
		fseek(config->file_, 0, SEEK_END);
		config->currentLogFileSize_ = ftell(config->file_);
	}
	Logger::Logger(const std::string& name)
		: name_(name) {
	}
	const std::string& Logger::getName() const {
		return name_;
	}
	void Logger::setName(const std::string& name) {
		name_ = name;
	}
	LoggerLevel Logger::getLevel() const {
		return level_;
	}
	void Logger::setLevel(LoggerLevel level) {
		level_ = level;
		for (auto& it : appender_) {
			it->setLevel(level_);
		}
	}
	LoggerFormatter::ptr Logger::getFormatter() const {
		return formatter_;
	}
	void Logger::setFormatter(const std::string& format) {
		if (!formatter_) {
			formatter_ = std::make_shared<LoggerFormatter>(format);
		}
		formatter_->setFormat(format);
		for (auto& it : appender_) {
			it->setFormatter(formatter_);
		}
	}
	void Logger::setFormatter(LoggerFormatter::ptr formatter) {
		formatter_ = formatter;
		for (auto& it : appender_) {
			it->setFormatter(formatter_);
		}
	}
	void Logger::addAppender(LoggerAppenderInterface::ptr appender) {
		appender_.insert(appender);
	}
	void Logger::removeAppender(LoggerAppenderInterface::ptr appender) {
		appender_.erase(appender);
	}
	void Logger::log(LoggerEvent::ptr event) {
		if (level_ > event->getLevel()) {
			return;
		}
		for (auto& it : appender_) {
			it->log(event);
		}
	}
	Logger::ptr LoggerManager::getLogger(const std::string& name) {
		auto itLogger = loggers_.find(name);
		if (loggers_.end() != itLogger) {
			return itLogger->second;
		}
		auto logger = std::make_shared<Logger>();
		auto formatter = std::make_shared<LoggerFormatter>();
		logger->setName(name);

		if (Config::GetBoolean("logger", "console_log", true)) {
			auto consoleAppender = std::make_shared<LoggerConsoleAppenderImpl>();
			logger->addAppender(consoleAppender);
		}
		if (Config::GetBoolean("logger", "file_log", true)) {
			auto fileAppender = std::make_shared<LoggerFileAppenderImpl>();
			fileAppender->setPath(Config::GetString("logger", "path", "./logs"));
			fileAppender->setMaxSize(Config::GetIntager("logger", "max_size", 1024 * 1024 * 8));
			logger->addAppender(fileAppender);
		}
		
		logger->setFormatter(formatter);
		logger->setLevel(GetLoggerLevelEnum(Config::GetString("logger", "level", "trace")));
		loggers_[name] = logger;
		return logger;
	}
	LoggerEventWarp::LoggerEventWarp(std::shared_ptr<Logger> logger, uint64_t timestamp, LoggerLevel level
		, const std::string& function, uint32_t line) {
		event_ = std::make_shared<LoggerEvent>(logger, timestamp, level, function, line);
	}
	LoggerEventWarp::~LoggerEventWarp() {
		event_->getLogger()->log(event_);
	}
	std::stringstream& LoggerEventWarp::getSS() {
		return event_->getSS();
	}
}
