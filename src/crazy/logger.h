/**
 * @file logger.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_LOGGER_H____
#define ____CRAZY_LOGGER_H____

#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <sstream>
#include <vector>

#include "mutex.h"
#include "noncopyable.h"
#include "singleton.h"
#include "util.h"

#define LOGGER_LEVEL(XX) 	\
	XX(Trace, 0, [TRACE]) 	\
	XX(Debug, 1, [DEBUG])	\
	XX(Info,  2, [INFO])	\
	XX(Warn,  3, [WARN])	\
	XX(Error, 4, [ERROR])	\
	XX(Fatal, 5, [FATAL])

#define CRAZY_ROOT_LOGGER() crazy::Singleton<crazy::LoggerManager>::GetInstance().GetLogger("root")
#define CRAZY_SYSTEM_LOGGER() crazy::Singleton<crazy::LoggerManager>::GetInstance().GetLogger("system")
#define CRAZY_LOGGER(NAME) crazy::Singleton<crazy::LoggerManager>::GetInstance().GetLogger(#NAME)

#define CRAZY_LOG(LOGGER, LEVEL) crazy::LoggerWarp(LOGGER, LEVEL, crazy::GetCurrentUS(), __FILE__, __LINE__, getpid(), crazy::GetThreadId(), crazy::GetCoroutineId()).GetSS() 
#define CRAZY_TRACE(LOGGER) CRAZY_LOG(LOGGER, crazy::LoggerLevel::Trace)
#define CRAZY_DEBUG(LOGGER) CRAZY_LOG(LOGGER, crazy::LoggerLevel::Debug)
#define CRAZY_INFO(LOGGER)  CRAZY_LOG(LOGGER, crazy::LoggerLevel::Info )
#define CRAZY_WARN(LOGGER)  CRAZY_LOG(LOGGER, crazy::LoggerLevel::Warn )
#define CRAZY_ERROR(LOGGER) CRAZY_LOG(LOGGER, crazy::LoggerLevel::Error)
#define CRAZY_FATAL(LOGGER) CRAZY_LOG(LOGGER, crazy::LoggerLevel::Fatal)

#define CRAZY_LOG_FORMAT(LOGGER, LEVEL, FMT, ...) crazy::LoggerWarp(LOGGER, LEVEL, crazy::GetCurrentUS(), __FILE__, __LINE__, getpid(), crazy::GetThreadId(), crazy::GetCoroutineId()).GetEvent()->Format(FMT, __VA_ARGS__) 
#define CRAZY_TRACE_FORMAT(LOGGER, FMT, ...) CRAZY_LOG_FORMAT(LOGGER, crazy::LoggerLevel::Trace, FMT, __VA_ARGS__)
#define CRAZY_DEBUG_FORMAT(LOGGER, FMT, ...) CRAZY_LOG_FORMAT(LOGGER, crazy::LoggerLevel::Debug, FMT, __VA_ARGS__)
#define CRAZY_INFO_FORMAT(LOGGER, FMT, ...)  CRAZY_LOG_FORMAT(LOGGER, crazy::LoggerLevel::Info,  FMT, __VA_ARGS__)
#define CRAZY_WARN_FORMAT(LOGGER, FMT, ...)  CRAZY_LOG_FORMAT(LOGGER, crazy::LoggerLevel::Warn,  FMT, __VA_ARGS__)
#define CRAZY_ERROR_FORMAT(LOGGER, FMT, ...) CRAZY_LOG_FORMAT(LOGGER, crazy::LoggerLevel::Error, FMT, __VA_ARGS__)
#define CRAZY_FATAL_FORMAT(LOGGER, FMT, ...) CRAZY_LOG_FORMAT(LOGGER, crazy::LoggerLevel::Fatal, FMT, __VA_ARGS__)

namespace crazy { 

	enum class LoggerLevel : char {
#define XX(TYPE, VALUE, STRING) TYPE = VALUE,
		LOGGER_LEVEL(XX)	
#undef XX
	};

	const std::string GetLoggerLevelString(const LoggerLevel& level);

	class Logger;
	class LoggerEvent final {
	public:
		using Ptr = std::shared_ptr<LoggerEvent>;
		LoggerEvent(const std::shared_ptr<Logger> logger, const LoggerLevel level
				, const uint64_t timestamp , const std::string& file_name
				, const uint32_t line, const uint32_t process_id
				, const uint32_t thread_id, const uint64_t fiber_id)
			: m_logger(logger), m_level(level), m_timestamp(timestamp), m_file_name(file_name)
			  , m_line(line), m_process_id(process_id), m_thread_id(thread_id), m_fiber_id(fiber_id) {}
		
		const std::shared_ptr<Logger> GetLogger() const { return m_logger; }
		const LoggerLevel GetLevel() const { return m_level; }
		const uint64_t GetTimestamp() const { return m_timestamp; }
		const std::string GetFileName() const { return m_file_name; }
		const uint32_t GetLine() const { return m_line; }
		const uint32_t GetProcessId() const { return m_process_id; }
		const uint64_t GetThreadId() const { return m_thread_id; }
		const uint32_t GetFiberId() const { return m_fiber_id; }
		std::stringstream& GetSS() { return m_ss; }
    	void Format(const char* fmt, ...);
    	void Format(const char* fmt, va_list al);
	private:
		std::shared_ptr<Logger> m_logger;
		LoggerLevel m_level;
		uint64_t m_timestamp;
		std::string m_file_name;
		uint32_t m_line;
		uint32_t m_process_id;
		uint32_t m_thread_id;
		uint64_t m_fiber_id;
		std::stringstream m_ss;
	};
	
	class LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<LoggerFormatItemInterface>;
		virtual ~LoggerFormatItemInterface() {};
		virtual void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) = 0;
	};
	
	class PercentSignFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<PercentSignFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};	

	class CategoryFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<CategoryFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};

	class PriorityFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<PriorityFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};

	class DateFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<DateFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};

	class MessageFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<MessageFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};
	
	class LineSeparatorFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<LineSeparatorFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};
	
	class ProcessIdFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<ProcessIdFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};

	class ThreadIdFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<ThreadIdFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};

	class FileNameFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<FileNameFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};

	class LineFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<LineFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};

	class FiberIdFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<FiberIdFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};

	class TabFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<TabFormatItem>;
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	};

	class CharacterFormatItem final : public LoggerFormatItemInterface {
	public:
		using Ptr = std::shared_ptr<CharacterFormatItem>;
		CharacterFormatItem(const char character)
			: m_character(character) {}
		void Format(std::stringstream& ss, const std::shared_ptr<Logger> logger
				, const LoggerEvent::Ptr event) override;
	private:
		char m_character;
	};
	
	class LoggerFormatter {
	public:
		using Ptr = std::shared_ptr<LoggerFormatter>;
		LoggerFormatter(const std::string& format_string = 
				"%d %P %c %p %t %F %f:%l %m");
		std::string log(const std::shared_ptr<Logger> logger, const LoggerEvent::Ptr event);
		void SetFormatString(const std::string& format_string);
		bool HasError() const { return m_is_error; }

	private:
		void parse(const std::string& format_string);
	
	private:
		bool m_is_error = false;
		std::string m_format_string;
		std::vector<LoggerFormatItemInterface::Ptr> m_format_items;
	};

	class LoggerAppenderInterface {
	public:
		using Ptr = std::shared_ptr<LoggerAppenderInterface>;
		virtual ~LoggerAppenderInterface() {}
		virtual void Log(const std::shared_ptr<Logger> logger, const LoggerEvent::Ptr event) = 0;	
		LoggerFormatter::Ptr GetFormatter() const { return m_logger_formatter; }
		void SetLevel(const LoggerLevel& level) { m_level = level; }
		const LoggerLevel GetLevel() const { return m_level; }
		void SetLoggerFormatter(const LoggerFormatter::Ptr formatter);
		void SetLoggerFormatString(const std::string& format_string);

	private:
		LoggerLevel m_level = LoggerLevel::Trace;
		LoggerFormatter::Ptr m_logger_formatter;
	};

	class ConsoleLoggerAppender final : public LoggerAppenderInterface {
	public:
		using Ptr = std::shared_ptr<ConsoleLoggerAppender>;
		void Log(const std::shared_ptr<Logger> logger, const LoggerEvent::Ptr event) override;	
	};

	class FileLoggerAppender final : public LoggerAppenderInterface {
	public:
		using Ptr = std::shared_ptr<FileLoggerAppender>;
		void Log(const std::shared_ptr<Logger> logger, const LoggerEvent::Ptr event) override;	
		void SetFileLoggerPath(const std::string& path, const std::string& name = "applection.log");
		void SetMaxFileSize(const size_t size) { m_max_size = size; }
		std::string GetFileLoggerPath() const { return m_logger_path; }
		size_t GetFileMaxSize() const { return m_max_size; }
	private:	
		void open();
		void reopen();
		void rename();

	private:
		size_t m_pos;
		size_t m_max_size;
		std::string m_logger_file_name;
		std::string m_logger_path;
		std::shared_ptr<std::ofstream> m_spofs;
	};

	class Logger final : public std::enable_shared_from_this<Logger> {
	public:
		using Ptr = std::shared_ptr<Logger>;
		void SetName(const std::string& name) { m_name = name; }
		void SetLoggerFormatString(const std::string& format_string);
		void SetLevel(const LoggerLevel& level);
		void SetAppender(const LoggerAppenderInterface::Ptr appender);
		void DelAppender(const LoggerAppenderInterface::Ptr appender);
		void ClearAppender();
		void SetLoggerFormatter(const LoggerFormatter::Ptr formatter);
		
		std::string GetLoggerFormatString() const { return m_format_string; }
		LoggerLevel GetLoggerLevel() const { return m_logger_level; }
		LoggerFormatter::Ptr GetLoggerFormatter() const { return m_spLoggerFormatter; }
		std::set<LoggerAppenderInterface::Ptr> GetLoggerAppender() const { return m_appenders; } 
		std::string GetName() const { return m_name; }
		void Log(const LoggerEvent::Ptr event);

	private:
		Mutex m_mutex;
		std::string m_name;
		std::string m_format_string;
		LoggerLevel m_logger_level;
		LoggerFormatter::Ptr m_spLoggerFormatter;
		std::set<LoggerAppenderInterface::Ptr> m_appenders;
	};
	
	class LoggerWarp final {
	public:
		using Ptr = std::shared_ptr<LoggerWarp>;
		LoggerWarp(const std::shared_ptr<Logger> logger, const LoggerLevel level
				, const uint64_t timestamp , const std::string& file_name
				, const uint32_t line, const uint32_t process_id
				, const uint32_t thread_id, const uint32_t fiber_id);
		~LoggerWarp();
		std::stringstream& GetSS() { return m_spLoggerEvent->GetSS(); }
		LoggerEvent::Ptr GetEvent() { return m_spLoggerEvent; }
	private:
		LoggerEvent::Ptr m_spLoggerEvent;
	};

	class LoggerManager final : public Noncopyable {
	public:
		using Ptr = std::shared_ptr<LoggerManager>;
		Logger::Ptr GetLogger(const std::string& name);
	private:
		std::map<std::string, Logger::Ptr> m_loggers;
	};
}

#endif // ! ____CRAZY_LOGGER_H____
