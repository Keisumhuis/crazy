#include "logger.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>

#define LOGGER_FORMAT_ITEM_MAP(XX)	\
	XX('%', PercentSignFormatItem)	\
	XX('c', CategoryFormatItem)	\
	XX('d', DateFormatItem)		\
	XX('m', MessageFormatItem)	\
	XX('L', LineSeparatorFormatItem)\
	XX('p', ProcessIdFormatItem)	\
	XX('t', ThreadIdFormatItem)	\
	XX('f', FileNameFormatItem)	\
	XX('l', LineFormatItem)		\
	XX('F', FiberIdFormatItem)	\
	XX('T', TabFormatItem)		\
	XX('P', PriorityFormatItem)

namespace crazy {

const std::string GetLoggerLevelString(const LoggerLevel& level) {
	switch (level) {
#define XX(TYPE, VALUE, STRING) case LoggerLevel::TYPE: return #STRING;
		LOGGER_LEVEL(XX)
#undef XX	
		default : return "[UNKNOW]";
	}
}

void PercentSignFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << "%";
}


void CategoryFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << GetLoggerLevelString(event->GetLevel());
}

void PriorityFormatItem::Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << logger->GetName();
}

void DateFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	time_t timestamp = static_cast<time_t>(event->GetTimestamp() / 1000000);
	ss << std::put_time(localtime(&timestamp), "%Y-%m-%d %H:%M:%S.") << std::setw(6) << std::setfill('0') << event->GetTimestamp() % 1000000;
}

void MessageFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << event->GetSS().str();
}

void LineSeparatorFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << std::endl;
}

void ProcessIdFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << event->GetProcessId();
}

void ThreadIdFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << event->GetThreadId();
}

void FileNameFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << event->GetFileName();
}

void LineFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << event->GetLine();
}

void FiberIdFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << event->GetFiberId();
}

void TabFormatItem::Format(std::stringstream& ss,  std::shared_ptr<Logger> logger
		, const LoggerEvent::Ptr event) {
	ss << "\t";
}

void CharacterFormatItem::Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) {
	ss << m_character;
}

LoggerFormatter::LoggerFormatter(const std::string& format_string) {
	SetFormatString(format_string);
}

std::string LoggerFormatter::log(const std::shared_ptr<Logger> logger, const LoggerEvent::Ptr event) {
	std::stringstream ss;
	for (auto& itor : m_format_items) {
		itor->Format(ss, logger, event);
	}
	return ss.str();
}

void LoggerFormatter::SetFormatString(const std::string& format_string) {
	m_format_string = format_string;
	parse(m_format_string);
}

void LoggerFormatter::parse(const std::string& format_string) {
	for (size_t i = 0; i < format_string.size(); ++i) {
		if(format_string[i] == '%') {
			switch(format_string[i + 1]) {
#define XX(CHARACTER, FORMAT_ITEM) \
				case CHARACTER: m_format_items.push_back(LoggerFormatItemInterface::Ptr (new FORMAT_ITEM)); ++i; break;
			LOGGER_FORMAT_ITEM_MAP(XX)
#undef XX
			default: 
				m_format_items.push_back(LoggerFormatItemInterface::Ptr(new CharacterFormatItem(format_string[i]))); 
			}
		} else {
			m_format_items.push_back(LoggerFormatItemInterface::Ptr(new CharacterFormatItem(format_string[i]))); 
		}
	}
}


void LoggerAppenderInterface::SetLoggerFormatter(const LoggerFormatter::Ptr formatter) {
	m_logger_formatter = formatter;
}

void LoggerAppenderInterface::SetLoggerFormatString(const std::string& format_string) {
	m_logger_formatter->SetFormatString(format_string);
}

void ConsoleLoggerAppender::Log(const std::shared_ptr<Logger> logger, const LoggerEvent::Ptr event) {
	if (GetLevel() <= event->GetLevel()) {
		std::cout << GetFormatter()->log(logger, event) << std::endl;
	}
}

void FileLoggerAppender::Log(const std::shared_ptr<Logger> logger, const LoggerEvent::Ptr event) {
	if (GetLevel() <= event->GetLevel() && m_spofs->is_open()) {
		auto log_string =  GetFormatter()->log(logger, event);
		*m_spofs.get() << log_string << std::endl;
		m_pos += log_string.size();
		if(m_pos >= m_max_size) {
			reopen();
		}
	}
}

void FileLoggerAppender::SetFileLoggerPath(const std::string& path, const std::string& name) {
	m_logger_path = path;
	m_logger_file_name = name;
	open();
}

void FileLoggerAppender::open() {
	m_spofs = std::make_shared<std::ofstream>(m_logger_path + "/" + m_logger_file_name, std::ios::out | std::ios::app);
	if ( m_spofs->is_open() ) {
		m_spofs->seekp(0, std::ios::end);
		m_pos = m_spofs->tellp();
	} else {
		std::cout << "open logger file fail, path : " 
			<< m_logger_path << "/" << m_logger_file_name << std::endl;
	}
}

void FileLoggerAppender::reopen() {
	m_spofs->close();
	rename();
	open();
}

void FileLoggerAppender::rename() {
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(now);
    
    	char buffer[80];
    	strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&time));
	std::filesystem::rename(m_logger_path + "/" + m_logger_file_name, 
			m_logger_path + "/" + m_logger_file_name + "." + buffer);
}


void Logger::SetLoggerFormatString(const std::string& format_string) {
	m_spLoggerFormatter->SetFormatString(format_string);
	for (auto& itAppender : m_appenders) {
		itAppender->SetLoggerFormatString(format_string);
	}
}

void Logger::SetLevel(const LoggerLevel& level) {
	m_logger_level = level;
	for (auto& itAppender : m_appenders) {
		itAppender->SetLevel(level);
	}
}

void Logger::SetAppender(const LoggerAppenderInterface::Ptr appender) {
	m_appenders.insert(appender);
}

void Logger::DelAppender(const LoggerAppenderInterface::Ptr appender) {
	m_appenders.erase(appender);
}

void Logger::ClearAppender() {
	m_appenders.clear();
}

void Logger::SetLoggerFormatter(const LoggerFormatter::Ptr formatter) {
	m_spLoggerFormatter = formatter;
	for(auto& itAppender : m_appenders) {
		itAppender->SetLoggerFormatter(formatter);
	}
}

void Logger::Log(const LoggerEvent::Ptr event) {
	Mutex::Guard guard(m_mutex);
	for (auto& itAppender : m_appenders) {
		itAppender->Log(shared_from_this(), event);
	}
}

LoggerWarp::LoggerWarp(const std::shared_ptr<Logger> logger, const LoggerLevel level
				, const uint64_t timestamp , const std::string& file_name
				, const uint32_t line, const uint32_t process_id
				, const uint32_t thread_id, const uint32_t fiber_id) {
	m_spLoggerEvent = std::make_shared<LoggerEvent>(logger, level, timestamp, file_name
			, line, process_id, thread_id, fiber_id);
}

LoggerWarp::~LoggerWarp() {
	m_spLoggerEvent->GetLogger()->Log(m_spLoggerEvent);
}

Logger::Ptr LoggerManager::GetLogger(const std::string& name) {
	auto itLogger = m_loggers.find(name);
	if (m_loggers.end() != itLogger) {
		return itLogger->second;
	} else {
		Logger::Ptr logger (new Logger);
		logger->SetName(name);
        	LoggerAppenderInterface::Ptr appender (new ConsoleLoggerAppender);
        	LoggerFormatter::Ptr formatter (new LoggerFormatter);
        	appender->SetLoggerFormatter(formatter);
		logger->SetAppender(appender);
		m_loggers[name] = logger;
		return logger;
	}
}

}
