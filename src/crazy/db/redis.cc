#include "redis.h"

namespace crazy {

static ConfigValue<std::map<std::string, std::map<std::string, std::string> > >::Ptr g_redis =
    Config::Lookup("redis.config", std::map<std::string, std::map<std::string, std::string> >(), "redis config");


static std::string get_value(const std::map<std::string, std::string>& m
                             ,const std::string& key
                             ,const std::string& def = "") {
    auto it = m.find(key);
    return it == m.end() ? def : it->second;
}

RedisInterface::RedisInterface() 
    : m_logEnable(true) {
}
const std::string& RedisInterface::GetName() const {
    return m_name;
}
void RedisInterface::SetName(const std::string& v) {
    m_name = v;
}
const std::string& RedisInterface::GetPasswd() const {
    return m_passwd;
}
void RedisInterface::SetPasswd(const std::string& v) {
    m_passwd = v;
}
RedisInterface::Type RedisInterface::GetType() const {
    return m_type;
}
uint64_t SyncRedisInterface::GetLastActiveTime() const {
    return m_lastActiveTime;
}
void SyncRedisInterface::SetLastActiveTime(uint64_t v) {
    m_lastActiveTime = v;
}

Redis::Redis() {
    m_type = RedisInterface::Redis;
}
Redis::Redis(const std::map<std::string, std::string>& conf) {
    m_type = RedisInterface::Redis;
    auto tmp = get_value(conf, "host");
    auto pos = tmp.find(":");
    m_host = tmp.substr(0, pos);
    m_port = std::stoi(tmp.substr(pos + 1));
    m_passwd = get_value(conf, "passwd");
    m_logEnable = std::stoi(get_value(conf, "log_enable", "1"));

    tmp = get_value(conf, "timeout_com");
    if(tmp.empty()) {
        tmp = get_value(conf, "timeout");
    }
    uint64_t v = std::stoi(tmp);

    m_cmdTimeout.tv_sec = v / 1000;
    m_cmdTimeout.tv_usec = v % 1000 * 1000;
}
bool Redis::Reconnect() {
    return redisReconnect(m_context.get());
}
bool Redis::Connect(const std::string& ip, const int32_t port, const uint64_t ms) {
    m_host = ip;
    m_port = port;
    m_connectMs = ms;
    if(m_context) {
        return true;
    }
    timeval tv = {(int)ms / 1000, (int)ms % 1000 * 1000};
    auto c = redisConnectWithTimeout(ip.c_str(), port, tv);
    if(c) {
        if(m_cmdTimeout.tv_sec || m_cmdTimeout.tv_usec) {
            SetTimeout(m_cmdTimeout.tv_sec * 1000 + m_cmdTimeout.tv_usec / 1000);
        }
        m_context.reset(c, redisFree);

        if(!m_passwd.empty()) {
            auto r = (redisReply*)redisCommand(c, "auth %s", m_passwd.c_str());
            if(!r) {
                CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "auth error:("
                    << m_host << ":" << m_port << ", " << m_name << ")";
                return false;
            }
            if(r->type != REDIS_REPLY_STATUS) {
                CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "auth reply type error:" << r->type << "("
                    << m_host << ":" << m_port << ", " << m_name << ")";
                return false;
            }
            if(!r->str) {
                CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "auth reply str error: NULL("
                    << m_host << ":" << m_port << ", " << m_name << ")";
                return false;
            }
            if(strcmp(r->str, "OK") == 0) {
                return true;
            } else {
                CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "auth error: " << r->str << "("
                    << m_host << ":" << m_port << ", " << m_name << ")";
                return false;
            }
        }
        return true;
    }
    return false;
}
bool Redis::Connect() {
    return Connect(m_host, m_port, 50);
}
bool Redis::SetTimeout(uint64_t ms) {
    m_cmdTimeout.tv_sec = ms / 1000;
    m_cmdTimeout.tv_usec = ms % 1000 * 1000;
    redisSetTimeout(m_context.get(), m_cmdTimeout);
    return true;
}
ReplyPtr Redis::Cmd(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    ReplyPtr rt = Cmd(format, ap);
    va_end(ap);
    return rt;
}
ReplyPtr Redis::Cmd(const char* format, va_list ap) {
    auto r = (redisReply*)redisvCommand(m_context.get(), format, ap);
    if(!r) {
        if(m_logEnable) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "redisCommand error: (" << format << ")(" 
                << m_host << ":" << m_port << ")(" << m_name << ")";
        }
        return nullptr;
    }
    ReplyPtr rt(r, freeReplyObject);
    if(r->type != REDIS_REPLY_ERROR) {
        return rt;
    }
    if(m_logEnable) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "redisCommand error: (" << format << ")(" 
            << m_host << ":" << m_port << ")(" << m_name << ")" << ": " << r->str;
    }
    return nullptr;
}
ReplyPtr Redis::Cmd(const std::vector<std::string>& argv) {
    std::vector<const char*> v;
    std::vector<size_t> l;
    for(auto& i : argv) {
        v.push_back(i.c_str());
        l.push_back(i.size());
    }

    auto r = (redisReply*)redisCommandArgv(m_context.get(), argv.size(), &v[0], &l[0]);
    if(!r) {
        if(m_logEnable) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "redisCommandArgv error: (" << m_host << ":" << m_port << ")(" 
                << m_name << ")";
        }
        return nullptr;
    }
    ReplyPtr rt(r, freeReplyObject);
    if(r->type != REDIS_REPLY_ERROR) {
        return rt;
    }
    if(m_logEnable) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "redisCommandArgv error: (" << m_host << ":" << m_port << ")(" 
            << m_name << ")" << r->str;
    }
    return nullptr;
}
int32_t Redis::AppendCmd(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int rt = AppendCmd(format, ap);
    va_end(ap);
    return rt;
}
int32_t Redis::AppendCmd(const char* format, va_list ap) {
    return redisvAppendCommand(m_context.get(), format, ap);
}
int32_t Redis::AppendCmd(const std::vector<std::string>& argv) {
    std::vector<const char*> v;
    std::vector<size_t> l;
    for(auto& i : argv) {
        v.push_back(i.c_str());
        l.push_back(i.size());
    }
    return redisAppendCommandArgv(m_context.get(), argv.size(), &v[0], &l[0]);
}
ReplyPtr Redis::GetReply() {
    redisReply* r = nullptr;
    if(redisGetReply(m_context.get(), (void**)&r) == REDIS_OK) {
        ReplyPtr rt(r, freeReplyObject);
        return rt;
    }
    if(m_logEnable) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "redisGetReply error: (" << m_host << ":" << m_port << ")(" 
            << m_name << ")";
    }
    return nullptr;
}
RedisCluster::RedisCluster() {
    m_type = RedisInterface::Redis_Cluster;
}
RedisCluster::RedisCluster(const std::map<std::string, std::string>& conf) {
    m_type = RedisInterface::Redis_Cluster;
    m_host = get_value(conf, "host");
    m_passwd = get_value(conf, "passwd");
    m_logEnable = std::stoi(get_value(conf, "log_enable", "1"));
    auto tmp = get_value(conf, "timeout_com");
    if(tmp.empty()) {
        tmp = get_value(conf, "timeout");
    }
    uint64_t v = std::stoi(tmp);

    m_cmdTimeout.tv_sec = v / 1000;
    m_cmdTimeout.tv_usec = v % 1000 * 1000;
}
bool RedisCluster::Reconnect() {
    return true;
}
bool RedisCluster::Connect(const std::string& ip, const int32_t port, const uint64_t ms) {
    m_host = ip;
    m_port = port;
    m_connectMs = ms;
    if(m_context) {
        return true;
    }
    timeval tv = {(int)ms / 1000, (int)ms % 1000 * 1000};
    auto c = redisClusterConnectWithTimeout(ip.c_str(), tv, 0);
    if(c) {
        m_context.reset(c, redisClusterFree);
        if(!m_passwd.empty()) {
            auto r = (redisReply*)redisClusterCommand(c, "auth %s", m_passwd.c_str());
            if(!r) {
                CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "auth error:("
                    << m_host << ":" << m_port << ", " << m_name << ")";
                return false;
            }
            if(r->type != REDIS_REPLY_STATUS) {
                CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "auth reply type error:" << r->type << "("
                    << m_host << ":" << m_port << ", " << m_name << ")";
                return false;
            }
            if(!r->str) {
                CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "auth reply str error: NULL("
                    << m_host << ":" << m_port << ", " << m_name << ")";
                return false;
            }
            if(strcmp(r->str, "OK") == 0) {
                return true;
            } else {
                CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "auth error: " << r->str << "("
                    << m_host << ":" << m_port << ", " << m_name << ")";
                return false;
            }
        }
        return true;
    }
    return false;
}
bool RedisCluster::Connect() {
    return Connect(m_host, m_port, 50);
}
bool RedisCluster::SetTimeout(uint64_t ms) {
    return true;
}
ReplyPtr RedisCluster::Cmd(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    ReplyPtr rt = Cmd(format, ap);
    va_end(ap);
    return rt;
}
ReplyPtr RedisCluster::Cmd(const char* format, va_list ap) {
    auto r = (redisReply*)redisClustervCommand(m_context.get(), format, ap);
    if(!r) {
        if(m_logEnable) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "redisCommand error: (" << format << ")(" << m_host << ":" 
                << m_port << ")(" << m_name << ")";
        }
        return nullptr;
    }
    ReplyPtr rt(r, freeReplyObject);
    if(r->type != REDIS_REPLY_ERROR) {
        return rt;
    }
    if(m_logEnable) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "redisCommand error: (" << format << ")(" << m_host << ":" 
            << m_port << ")(" << m_name << ")" << ": " << r->str;
    }
    return nullptr;
}
ReplyPtr RedisCluster::Cmd(const std::vector<std::string>& argv) {
    std::vector<const char*> v;
    std::vector<size_t> l;
    for(auto& i : argv) {
        v.push_back(i.c_str());
        l.push_back(i.size());
    }

    auto r = (redisReply*)redisClusterCommandArgv(m_context.get(), argv.size(), &v[0], &l[0]);
    if(!r) {
        if(m_logEnable) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "redisCommandArgv error: (" << m_host << ":" << m_port << ")(" 
                << m_name << ")";
        }
        return nullptr;
    }
    ReplyPtr rt(r, freeReplyObject);
    if(r->type != REDIS_REPLY_ERROR) {
        return rt;
    }
    if(m_logEnable) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "redisCommandArgv error: (" << m_host << ":" << m_port << ")(" 
            << m_name << ")" << r->str;
    }
    return nullptr;
}
int32_t RedisCluster::AppendCmd(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int rt = AppendCmd(format, ap);
    va_end(ap);
    return rt;
}
int32_t RedisCluster::AppendCmd(const char* format, va_list ap) {
    return redisClustervAppendCommand(m_context.get(), format, ap);
}
int32_t RedisCluster::AppendCmd(const std::vector<std::string>& argv) {
    std::vector<const char*> v;
    std::vector<size_t> l;
    for(auto& i : argv) {
        v.push_back(i.c_str());
        l.push_back(i.size());
    }
    return redisClusterAppendCommandArgv(m_context.get(), argv.size(), &v[0], &l[0]);
}
ReplyPtr RedisCluster::GetReply() {
    redisReply* r = nullptr;
    if(redisClusterGetReply(m_context.get(), (void**)&r) == REDIS_OK) {
        ReplyPtr rt(r, freeReplyObject);
        return rt;
    }
    if(m_logEnable) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "redisGetReply error: (" << m_host << ":" << m_port << ")(" 
            << m_name << ")";
    }
    return nullptr;
}
RedisManager::RedisManager() {
    Init();
}
RedisInterface::Ptr RedisManager::Get(const std::string& name) {
    RWMutex::WRGuard guard(m_mutex);
    auto it = m_datas.find(name);
    if(it == m_datas.end()) {
        return nullptr;
    }
    if(it->second.empty()) {
        return nullptr;
    }
    auto r = it->second.front();
    it->second.pop_front();
    auto rr = dynamic_cast<SyncRedisInterface*>(r);
    if((time(0) - rr->GetLastActiveTime()) > 30) {
        if(!rr->Cmd("ping")) {
            if(!rr->Reconnect()) {
                RWMutex::WRGuard guard(m_mutex);
                m_datas[name].push_back(r);
                return nullptr;
            }
        }
    }
    rr->SetLastActiveTime(time(0));
    return std::shared_ptr<RedisInterface>(r, std::bind(&RedisManager::FreeRedis
                        ,this, std::placeholders::_1));
}
std::ostream& RedisManager::Dump(std::ostream& os) {
    os << "[RedisManager total=" << m_config.size() << "]" << std::endl;
    for(auto& i : m_config) {
        os << "    " << i.first << " :[";
        for(auto& n : i.second) {
            os << "{" << n.first << ":" << n.second << "}";
        }
        os << "]" << std::endl;
    }
    return os;
}
void RedisManager::FreeRedis(RedisInterface* r) {
    RWMutex::WRGuard guard(m_mutex);
    m_datas[r->GetName()].push_back(r);
}
void RedisManager::Init() {
    m_config = g_redis->GetValue();
    size_t done = 0;
    size_t total = 0;
    for(auto& i : m_config) {
        auto type = get_value(i.second, "type");
        auto pool = std::stoi(get_value(i.second, "pool"));
        auto passwd = get_value(i.second, "passwd");
        total += pool;
        for(int n = 0; n < pool; ++n) {
            if(type == "redis") {
                Redis* rds(new Redis(i.second));
                rds->Connect();
                rds->SetLastActiveTime(time(0));
                RWMutex::WRGuard guard(m_mutex);
                m_datas[i.first].push_back(rds);
                ++done;
            } else if(type == "redis_cluster") {
                RedisCluster* rds(new RedisCluster(i.second));
                rds->Connect();
                rds->SetLastActiveTime(time(0));
                RWMutex::WRGuard guard(m_mutex);
                m_datas[i.first].push_back(rds);
                ++done;
            } 
        }
    }

    while(done != total) {
        usleep(5000);
    }
}
ReplyPtr RedisUtil::Cmd(const std::string& name, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ReplyPtr rt = Cmd(name, fmt, ap);
    va_end(ap);
    return rt;
}
ReplyPtr RedisUtil::Cmd(const std::string& name, const char* fmt, va_list ap) {
    auto rds = RedisMgr::GetInstance().Get(name);
    if(!rds) {
        return nullptr;
    }
    return rds->Cmd(fmt, ap);
}
ReplyPtr RedisUtil::Cmd(const std::string& name, const std::vector<std::string>& args) {
    auto rds = RedisMgr::GetInstance().Get(name);
    if(!rds) {
        return nullptr;
    }
    return rds->Cmd(args);
}
ReplyPtr RedisUtil::TryCmd(const std::string& name, uint32_t count, const char* fmt, ...) {
    for(uint32_t i = 0; i < count; ++i) {
        va_list ap;
        va_start(ap, fmt);
        ReplyPtr rt = Cmd(name, fmt, ap);
        va_end(ap);

        if(rt) {
            return rt;
        }
    }
    return nullptr;
}
ReplyPtr RedisUtil::TryCmd(const std::string& name, uint32_t count, const char* fmt, va_list ap) {
    for(uint32_t i = 0; i < count; ++i) {
        ReplyPtr rt = Cmd(name, fmt, ap);
        if(rt) {
            return rt;
        }
    }
    return nullptr;
}
ReplyPtr RedisUtil::TryCmd(const std::string& name, uint32_t count, const std::vector<std::string>& args) {
    for(uint32_t i = 0; i < count; ++i) {
        ReplyPtr rt = Cmd(name, args);
        if(rt) {
            return rt;
        }
    }
    return nullptr;
}


}