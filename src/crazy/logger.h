/**
 * @file logger.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief 日志类
 * @version 0.1
 * @date 2025-08-31
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once

#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "crazy/cond_mutex.h"
#include "crazy/singleton.h"
#include "crazy/utils.h"

#define LOGGER_LEVEL_MAP(XX)    \
    XX(trace, 0, trace)         \
    XX(debug, 1, debug)         \
    XX(info,  2, info)          \
    XX(warn,  3, warn)          \
    XX(error, 4, error)         \
    XX(fatal, 5, fatal)

#define SYSTEM_LOGGER crazy::Singleton<crazy::LoggerManager>::Instance().getLogger("system")
#define LOGGER(name) crazy::Singleton<crazy::LoggerManager>::Instance().getLogger(#name)

#define CRAZY_LOG(logger, level) \
    crazy::LoggerEventWarp(LOGGER(logger), crazy::GetCurrentMS(), level, __FUNCTION__, __LINE__).getSS()
#define CRAZY_TRACE(logger) CRAZY_LOG(logger, crazy::LoggerLevel::trace)
#define CRAZY_DEBUG(logger) CRAZY_LOG(logger, crazy::LoggerLevel::debug)
#define CRAZY_INFO(logger) CRAZY_LOG(logger, crazy::LoggerLevel::info)
#define CRAZY_WARN(logger) CRAZY_LOG(logger, crazy::LoggerLevel::warn)
#define CRAZY_ERROR(logger) CRAZY_LOG(logger, crazy::LoggerLevel::error)
#define CRAZY_FATAL(logger) CRAZY_LOG(logger, crazy::LoggerLevel::fatal)

#define CRAZY_SYSTEM_LOG(level) CRAZY_LOG(system, level)
#define CRAZY_SYSTEM_TRACE() CRAZY_TRACE(system)
#define CRAZY_SYSTEM_DEBUG() CRAZY_DEBUG(system)
#define CRAZY_SYSTEM_INFO() CRAZY_INFO(system)
#define CRAZY_SYSTEM_WARN() CRAZY_WARN(system)
#define CRAZY_SYSTEM_ERROR() CRAZY_ERROR(system)
#define CRAZY_SYSTEM_FATAL() CRAZY_FATAL(system)

#define CRAZY_ROOT_LOG(level) CRAZY_LOG(root, level)
#define CRAZY_ROOT_TRACE() CRAZY_TRACE(root)
#define CRAZY_ROOT_DEBUG() CRAZY_DEBUG(root)
#define CRAZY_ROOT_INFO() CRAZY_INFO(root)
#define CRAZY_ROOT_WARN() CRAZY_WARN(root)
#define CRAZY_ROOT_ERROR() CRAZY_ERROR(root)
#define CRAZY_ROOT_FATAL() CRAZY_FATAL(root)

namespace crazy { 
    /**
     * @brief 日志等级.
     */
    enum class LoggerLevel : uint8_t {
#define XX(type, value, string) type = value,
        LOGGER_LEVEL_MAP(XX)
#undef XX
    };
    /**
     * @brief 日志等级转字符串.
     */
    inline const char* GetLoggerLevelString(LoggerLevel level);
    /**
     * @brief 字符串转日志等级.
     */
    inline LoggerLevel GetLoggerLevelEnum(const std::string& level);
    class Logger;
    /**
     * @brief 日志事件.
     */
    class LoggerEvent final {
    public:
        using ptr = std::shared_ptr<LoggerEvent>;
        /**
         * @brief 构造函数.
         * @param logger 日志控制器.
         * @param timestamp 日志时间戳.
         * @param level 日志等级.
         * @param function 函数名字.
         * @param line 文件行号.
         */
        explicit LoggerEvent(std::shared_ptr<Logger> logger, uint64_t timestamp, LoggerLevel level
            , const std::string& function, uint32_t line);
        /**
         * @brief 获取日志控制器.
         * @return std::shared_ptr<Logger> 日志控制器
         */
        std::shared_ptr<Logger> getLogger() const;
        /**
         * @brief 设置日志控制器.
         * @param logger 日志控制器.
         */
        void setLogger(const std::shared_ptr<Logger> logger);
        /**
         * @brief 获取日志时间戳.
         * @return uint64_t 日志时间戳.
         */
        uint64_t getTimestamp() const;
        /**
         * @brief 设置日志时间戳.
         * @param timestamp 日志时间戳.
         */
        void setTimestamp(uint64_t timestamp);
        /**
         * @brief 获取日志等级.
         * @return LoggerLevel 日志等级.
         */
        LoggerLevel getLevel() const;
        /**
         * @brief 设置日志等级.
         * @param level 日志等级.
         */
        void setLevel(LoggerLevel level);
        /**
         * @brief 获取当前函数名.
         * @return const std::string& 当前函数名.
         */
        const std::string& getFunction() const;
        /**
         * @brief 设置当前函数名.
         * @param function 当前函数名.
         */
        void setFunction(const std::string& function);
        /**
         * @brief 获取当前行号.
         * @return uint32_t 当前行号.
         */
        uint32_t getLine() const;
        /**
         * @brief 设置当前行号.
         * @param line 当前行号.
         */
        void setLine(uint32_t line);
        /**
         * @brief 获取日志流对象.
         * @return std::stringstream& 日志流对象.
         */
        std::stringstream& getSS();

    private:
        //! 日志控制器
        std::shared_ptr<Logger> logger_;
        //! 日志时间戳
        uint64_t timestamp_;
        //! 日志等级
        LoggerLevel level_;
        //! 函数名
        std::string function_;
        //! 行号
        uint32_t line_;
        //! 日志流对象
        std::stringstream ss_;
    };
    /**
     * @brief 日志组件基类.
     */
    class LoggerComponentInterface {
    public:
        using ptr = std::shared_ptr<LoggerComponentInterface>;
        /**
         * @brief 析构函数.
         */
        virtual ~LoggerComponentInterface() = default;
        /**
         * @brief 日志组件格式化.
         * @param os 输出流对象.
         * @param event 日志事件.
         */
        virtual void log(std::ostream& os, const LoggerEvent::ptr event) = 0;
    };
    /**
     * @brief 日志字符串组件.
     */
    class LoggerStringLiteralComponentImpl final : public LoggerComponentInterface {
    public:
        using ptr = std::shared_ptr<LoggerStringLiteralComponentImpl>;
        /**
         * @brief 构造函数.
         * @param stringliteral 日志字符串
         */
        explicit LoggerStringLiteralComponentImpl(const std::string& stringliteral);
        /**
         * @brief 日志组件格式化.
         * @param os 输出流对象.
         * @param event 日志事件.
         */
        void log(std::ostream& os, const LoggerEvent::ptr event) override;

    private:
        std::string stringliteral_;
    };
    /**
     * @brief 日志事件组件.
     */
    class LoggerDateTimeComponentImpl final : public LoggerComponentInterface {
    public:
        using ptr = std::shared_ptr<LoggerDateTimeComponentImpl>;
        /**
         * @brief 日志组件格式化.
         * @param os 输出流对象.
         * @param event 日志事件.
         */
        void log(std::ostream& os, const LoggerEvent::ptr event) override;
    };
    /**
     * @brief 日志分类组件.
     */
    class LoggerCategoryComponentImpl final : public LoggerComponentInterface {
    public:
        using ptr = std::shared_ptr<LoggerCategoryComponentImpl>;
        /**
         * @brief 日志组件格式化.
         * @param os 输出流对象.
         * @param event 日志事件.
         */
        void log(std::ostream& os, const LoggerEvent::ptr event) override;
    };
    /**
     * @brief 日志等级组件.
     */
    class LoggerPriorityComponentImpl final : public LoggerComponentInterface {
    public:
        using ptr = std::shared_ptr<LoggerPriorityComponentImpl>;
        /**
         * @brief 日志组件格式化.
         * @param os 输出流对象.
         * @param event 日志事件.
         */
        void log(std::ostream& os, const LoggerEvent::ptr event) override;
    };
    /**
     * @brief 日志函数组件.
     */
    class LoggerFunctionComponentImpl final : public LoggerComponentInterface {
    public:
        using ptr = std::shared_ptr<LoggerFunctionComponentImpl>;
        /**
         * @brief 日志组件格式化.
         * @param os 输出流对象.
         * @param event 日志事件.
         */
        void log(std::ostream& os, const LoggerEvent::ptr event) override;
    };
    /**
     * @brief 日志行号组件.
     */
    class LoggerLineComponentImpl final : public LoggerComponentInterface {
    public:
        using ptr = std::shared_ptr<LoggerLineComponentImpl>;
        /**
         * @brief 日志组件格式化.
         * @param os 输出流对象.
         * @param event 日志事件.
         */
        void log(std::ostream& os, const LoggerEvent::ptr event) override;
    };
    /**
     * @brief 日志信息组件.
     */
    class LoggerMessageComponentImpl final : public LoggerComponentInterface {
    public:
        using ptr = std::shared_ptr<LoggerMessageComponentImpl>;
        /**
         * @brief 日志组件格式化.
         * @param os 输出流对象.
         * @param event 日志事件.
         */
        void log(std::ostream& os, const LoggerEvent::ptr event) override;
    };
    /**
     * @brief 日志格式化器.
     */
    class LoggerFormatter final {
    public:
        using ptr = std::shared_ptr<LoggerFormatter>;
        /**
         * @brief 构造函数.
         * @param format 格式化字符串.
         */
        explicit LoggerFormatter(const std::string& format = "%d %c %p %f:%l %m");
        /**
         * @brief 获取格式化字符串.
         * @return const std::string& 格式化字符串
         */
        const std::string& getFormat() const;
        /**
         * @brief 设置格式化字符串.
         * @param format 格式化字符串.
         */
        void setFormat(const std::string format);
        /**
         * @brief 日志格式化.
         * @brief event 日志事件
         */
        std::string log(const LoggerEvent::ptr event);

    protected:
        /**
         * @brief 日志格式化字符串解析.
         */
        void parse();

    private:
        //! 日志格式化字符串
        std::string format_;
        //! 日志格式化组件
        std::vector<LoggerComponentInterface::ptr> components_;
    };
    /**
     * @brief 日志输出地.
     */
    class LoggerAppenderInterface {
    public:
        using ptr = std::shared_ptr<LoggerAppenderInterface>;
        /**
         * @brief 析构函数.
         */
        virtual ~LoggerAppenderInterface() = default;
        /**
         * @brief 获取日志等级.
         * @return LoggerLevel 日志等级.
         */
        LoggerLevel getLevel() const;
        /**
         * @brief 设置日志等级.
         * @param level 日志等级
         */
        void setLevel(LoggerLevel level);
        /**
         * @brief 获取日志格式化器.
         * @return LoggerFormatter::ptr 日志格式化器
         */
        LoggerFormatter::ptr getFormatter() const;
        /**
         * @brief 设置日志格式化器.
         * @param formatter 日志格式化器
         */
        void setFormatter(LoggerFormatter::ptr formatter);
        /**
         * @brief 日志输出接口.
         * @param event 日志事件
         */
        virtual void log(LoggerEvent::ptr event) = 0;

    protected:
        //! 条件变量互斥锁
        CondMutex condMutex_;

    private:
        //! 日志等级
        LoggerLevel level_ = LoggerLevel::trace;
        //! 日志格式化器
        LoggerFormatter::ptr formatter_;
    };
    /**
     * @brief 日志控制台输出地.
     */
    class LoggerConsoleAppenderImpl final : public LoggerAppenderInterface {
    public:
        using ptr = std::shared_ptr<LoggerConsoleAppenderImpl>;
        /**
         * @brief 日志输出接口.
         * @param event 日志事件
         */
        void log(LoggerEvent::ptr event) override;
    };
    /**
     * @brief 日志文件输出地.
     */
    class LoggerFileAppenderImpl final : public LoggerAppenderInterface {
    public:
        /**
         * @brief 日志文件配置.
         */
        struct LoggerFileConfig {
            //! 日志文件描述符
            FILE* file_ = nullptr;
            //! 日志文件路径
            std::string logFilePath_;
            //! 日志最大大小
            uint32_t maxLogFileSize_ = 8 * 1024 * 1024;
            //! 当前日志大小
            uint32_t currentLogFileSize_ = 0;
            //! 等级字符串
            std::string levelString;
        };
        using ptr = std::shared_ptr<LoggerFileAppenderImpl>;
        /**
         * @brief 构造函数.
         * @param logPath 日志文件路径
         */
        explicit LoggerFileAppenderImpl(const std::string& logPath = "./logs");
        /**
         * @brief 日志输出接口.
         * @param event 日志事件
         */
		void log(LoggerEvent::ptr event) override;
        /**
         * @brief 设置日志路径.
         */
        void setPath(const std::string& logPath);
        /**
         * @brief 设置日志最大大小.
         */
        void setMaxSize(const uint32_t size);

    private:
        /**
         * @brief 重新打开日志文件.
         */
        void reopen(LoggerFileConfig* config, const std::string& levelString, const std::string& loggerName);

    private:
        //! 日志文件描述符
        std::map<LoggerLevel, LoggerFileConfig> fileLogConfig_;
    };
    /**
     * @brief 日志控制器.
     */
    class Logger final {
    public:
        using ptr = std::shared_ptr<Logger>;
        /**
         * @brief 构造函数.
         * @param name 日志控制器名称
         */
        explicit Logger(const std::string& name = "root");
        /**
         * @brief 获取日志控制器名称.
         * @return const std::string& 日志控制器名称
         */
        const std::string& getName() const;
        /**
         * @brief 设置日志控制器名称.
         * @param name 日志控制器名称.
         */
        void setName(const std::string& name);
        /**
         * @brief 获取日志等级.
         * @return LoggerLevel 日志等级.
         */
        LoggerLevel getLevel() const;
        /**
         * @brief 设置日志等级.
         * @param level 日志等级
         */
        void setLevel(LoggerLevel level);
        /**
         * @brief 获取日志格式化器.
         * @return LoggerFormatter::ptr 日志格式化器
         */
        LoggerFormatter::ptr getFormatter() const;
        /**
         * @brief 设置日志格式化字符串.
         * @param format 日志格式化字符串
         */
        void setFormatter(const std::string& format);
        /**
         * @brief 设置日志格式化器.
         * @param formatter 日志格式化器
         */
        void setFormatter(LoggerFormatter::ptr formatter);
        /**
         * @brief 添加日志输出地.
         * @param appender 日志输出地
         */
        void addAppender(LoggerAppenderInterface::ptr appender);
        /**
         * @brief 删除日志输出地.
         * @param appender 日志输出地
         */
        void removeAppender(LoggerAppenderInterface::ptr appender);
        /**
         * @brief 日志输出.
         * @param event 日志事件
         */
        void log(LoggerEvent::ptr event);

    private:
        //! 日志控制器名称
        std::string name_;
        //! 日志等级
        LoggerLevel level_ = LoggerLevel::trace;
        //! 日志格式化器
        LoggerFormatter::ptr formatter_;
        //! 日志输出地
        std::set<LoggerAppenderInterface::ptr> appender_;
    };
    /**
     * @brief 日志控制器管理器.
     */
    class LoggerManager final {
    public:
        /**
         * @brief 获取日志控制器.
         * @param name 日志控制器名称
         * @return Logger::ptr 日志控制器
         */
        Logger::ptr getLogger(const std::string& name);    

    private:
        //! 日志控制器
        std::unordered_map<std::string, Logger::ptr> loggers_;
    };
    /**
     * @brief 日志事件包装器.
     */
    class LoggerEventWarp final {
    public:
        /**
         * @brief 构造函数.
         * @param logger 日志控制器.
         * @param timestamp 日志时间戳.
         * @param level 日志等级.
         * @param function 函数名字.
         * @param line 文件行号.
         */
        explicit LoggerEventWarp(std::shared_ptr<Logger> logger, uint64_t timestamp, LoggerLevel level
            , const std::string& function, uint32_t line);
        /**
         * @brief 析构函数.
         */
        ~LoggerEventWarp();
        /**
         * @brief 获取日志流对象.
         */
        std::stringstream& getSS();

    private:
        //! 日志事件
        LoggerEvent::ptr event_;
    };
}
