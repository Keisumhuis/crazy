/**
 * @file redis.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_DB_REDIS_H____
#define ____CRAZY_DB_REDIS_H____

#include <stdarg.h>
#include <stdlib.h>
#include <hiredis-vip/hiredis.h>
#include <hiredis-vip/hircluster.h>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "crazy/config.h"
#include "crazy/logger.h"
#include "crazy/mutex.h"
#include "crazy/singleton.h"

namespace crazy {

    using ReplyPtr = std::shared_ptr<redisReply>;

    class RedisInterface {
    public:
        enum Type {
            Redis = 1,
            Redis_Cluster = 2,
            Fox_Redis = 3,
            Fox_Redis_Cluster = 4
        };
        using Ptr = std::shared_ptr<RedisInterface>;
        RedisInterface();
        virtual ~RedisInterface() {}

        virtual ReplyPtr Cmd(const char* format, ...) = 0;
        virtual ReplyPtr Cmd(const char* format, va_list ap) = 0;
        virtual ReplyPtr Cmd(const std::vector<std::string>& argv) = 0;

        const std::string& GetName() const;
        void SetName(const std::string& v);
        const std::string& GetPasswd() const;
        void SetPasswd(const std::string& v);
        Type GetType() const;
    protected:
        std::string m_name;
        std::string m_passwd;
        Type m_type;
        bool m_logEnable;
    };

    class SyncRedisInterface : public RedisInterface {
    public:
        using Ptr = std::shared_ptr<SyncRedisInterface>;
        virtual ~SyncRedisInterface() {}

        virtual bool Reconnect() = 0;
        virtual bool Connect(const std::string& ip, const int32_t port, const uint64_t ms = 0) = 0;
        virtual bool Connect() = 0;
        virtual bool SetTimeout(uint64_t ms) = 0;

        virtual int32_t AppendCmd(const char* format, ...) = 0;
        virtual int32_t AppendCmd(const char* format, va_list ap) = 0;
        virtual int32_t AppendCmd(const std::vector<std::string>& argv) = 0;

        virtual ReplyPtr GetReply() = 0;

        uint64_t GetLastActiveTime() const;
        void SetLastActiveTime(uint64_t v);
    protected:
        uint64_t m_lastActiveTime;
    };

    class Redis : public SyncRedisInterface {
    public:
        using Ptr = std::shared_ptr<Redis>;
        Redis();
        Redis(const std::map<std::string, std::string>& conf);

        virtual bool Reconnect() override;
        virtual bool Connect(const std::string& ip, const int32_t port, const uint64_t ms = 0) override;
        virtual bool Connect() override;
        virtual bool SetTimeout(uint64_t ms) override;

        virtual ReplyPtr Cmd(const char* format, ...) override;
        virtual ReplyPtr Cmd(const char* format, va_list ap) override;
        virtual ReplyPtr Cmd(const std::vector<std::string>& argv) override;

        virtual int32_t AppendCmd(const char* format, ...) override;
        virtual int32_t AppendCmd(const char* format, va_list ap) override;
        virtual int32_t AppendCmd(const std::vector<std::string>& argv) override;

        virtual ReplyPtr GetReply() override;
    private:
        std::string m_host;
        uint32_t m_port;
        uint32_t m_connectMs;
        struct timeval m_cmdTimeout;
        std::shared_ptr<redisContext> m_context;
    };

    class RedisCluster : public SyncRedisInterface {
    public:
        using Ptr = std::shared_ptr<RedisCluster>;
        RedisCluster();
        RedisCluster(const std::map<std::string, std::string>& conf);

        virtual bool Reconnect() override;
        virtual bool Connect(const std::string& ip, const int32_t port, const uint64_t ms = 0) override;
        virtual bool Connect() override;
        virtual bool SetTimeout(uint64_t ms) override;

        virtual ReplyPtr Cmd(const char* format, ...) override;
        virtual ReplyPtr Cmd(const char* format, va_list ap) override;
        virtual ReplyPtr Cmd(const std::vector<std::string>& argv) override;

        virtual int32_t AppendCmd(const char* format, ...) override;
        virtual int32_t AppendCmd(const char* format, va_list ap) override;
        virtual int32_t AppendCmd(const std::vector<std::string>& argv) override;

        virtual ReplyPtr GetReply() override;
    private:
        std::string m_host;
        uint32_t m_port;
        uint32_t m_connectMs;
        struct timeval m_cmdTimeout;
        std::shared_ptr<redisClusterContext> m_context;
    };

    class RedisManager {
    public:
        RedisManager();
        RedisInterface::Ptr Get(const std::string& name);

        std::ostream& Dump(std::ostream& os);
    private:
        void FreeRedis(RedisInterface* r);
        void Init();
    private:
        RWMutex m_mutex;
        std::map<std::string, std::list<RedisInterface*> > m_datas;
        std::map<std::string, std::map<std::string, std::string> > m_config;
    };

    using RedisMgr = Singleton<RedisManager>;

    class RedisUtil {
    public:
        static ReplyPtr Cmd(const std::string& name, const char* fmt, ...);
        static ReplyPtr Cmd(const std::string& name, const char* fmt, va_list ap); 
        static ReplyPtr Cmd(const std::string& name, const std::vector<std::string>& args); 

        static ReplyPtr TryCmd(const std::string& name, uint32_t count, const char* fmt, ...);
        static ReplyPtr TryCmd(const std::string& name, uint32_t count, const char* fmt, va_list ap);
        static ReplyPtr TryCmd(const std::string& name, uint32_t count, const std::vector<std::string>& args); 
    };

}

#endif // ! ____CRAZY_DB_REDIS_H____