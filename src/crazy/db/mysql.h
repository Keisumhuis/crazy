/**
 * @file mysql.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_DB_MYSQL_H____
#define ____CRAZY_DB_MYSQL_H____

#include <mysql/mysql.h>
#include <stdarg.h>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "crazy/config.h"
#include "crazy/logger.h"
#include "crazy/mutex.h"
#include "crazy/singleton.h"
#include "db.h"

namespace crazy {

    bool mysql_time_to_time_t(const MYSQL_TIME& mt, time_t& ts);
    bool time_t_to_mysql_time(const time_t& ts, MYSQL_TIME& mt);

    class MySQLRes : public SQLDataInterface {
    public:
        using Ptr = std::shared_ptr<MySQLRes>;
        using data_cb = std::function<bool (MYSQL_ROW, uint64_t, int32_t)>;
        MySQLRes(MYSQL_RES* res, uint32_t eno, const char* errstr);

        MYSQL_RES* Get() const;
        virtual int32_t GetErrno() const override;
        virtual const std::string& GetErrorStr() const override;

        bool Foreach(data_cb cb);

        virtual int32_t GetDataCount() override;
        virtual int32_t GetColumnCount() override;
        virtual int32_t GetColumnByte(int idx) override;
        virtual int32_t GetColumnType(int idx) override;
        virtual const std::string GetColumnName(int idx) override;

        virtual bool IsNull(int32_t idx) override;
        virtual int8_t GetInt8(int32_t idx) override;
        virtual uint8_t GetUint8(int32_t idx) override;
        virtual int16_t GetInt16(int32_t idx) override;
        virtual uint16_t GetUint16(int32_t idx) override;
        virtual int32_t GetInt32(int32_t idx) override;
        virtual uint32_t GetUint32(int32_t idx) override;
        virtual int64_t GetInt64(int32_t idx) override;
        virtual uint64_t GetUint64(int32_t idx) override;
        virtual float GetFloat(int32_t idx) override;
        virtual double GetDouble(int32_t idx) override;
        virtual std::string GetString(int32_t idx) override;
        virtual std::string GetBlob(int32_t idx) override;
        virtual time_t GetTime(int32_t idx) override;
        virtual bool Next() override;
    private:
        int32_t m_errno;
        std::string m_errstr;
        MYSQL_ROW m_cur;
        uint64_t* m_curlength;
        std::shared_ptr<MYSQL_RES> m_data;
    };

    class MySQLStmt;
    class MySQLStmtRes : public SQLDataInterface {
    friend class MySQLStmt;
    public:
        using Ptr = std::shared_ptr<MySQLStmtRes>;
        static MySQLStmtRes::Ptr Create(std::shared_ptr<MySQLStmt> stmt);
        ~MySQLStmtRes();

        virtual int32_t GetErrno() const override;
        virtual const std::string& GetErrorStr() const override;
        virtual int32_t GetDataCount() override;
        virtual int32_t GetColumnCount() override;
        virtual int32_t GetColumnByte(int idx) override;
        virtual int32_t GetColumnType(int idx) override;
        virtual const std::string GetColumnName(int idx) override;

        virtual bool IsNull(int32_t idx) override;
        virtual int8_t GetInt8(int32_t idx) override;
        virtual uint8_t GetUint8(int32_t idx) override;
        virtual int16_t GetInt16(int32_t idx) override;
        virtual uint16_t GetUint16(int32_t idx) override;
        virtual int32_t GetInt32(int32_t idx) override;
        virtual uint32_t GetUint32(int32_t idx) override;
        virtual int64_t GetInt64(int32_t idx) override;
        virtual uint64_t GetUint64(int32_t idx) override;
        virtual float GetFloat(int32_t idx) override;
        virtual double GetDouble(int32_t idx) override;
        virtual std::string GetString(int32_t idx) override;
        virtual std::string GetBlob(int32_t idx) override;
        virtual time_t GetTime(int32_t idx) override;
        virtual bool Next() override;
    private: 
        MySQLStmtRes(std::shared_ptr<MySQLStmt> stmt, int32_t eno, const char* errstr);
        struct Data {
            Data(); 
            ~Data();
            void Alloc(size_t size);
            my_bool is_null;
            my_bool error;
            enum_field_types type;
            uint64_t length;
            int32_t data_length;
            char* data;
        };
        
    private:
        int32_t m_errno;
        std::string m_errstr;
        std::shared_ptr<MySQLStmt> m_stmt;
        std::vector<MYSQL_BIND> m_binds;
        std::vector<Data> m_datas;
    };

    class MySQLManager;
    class MySQL : public DBInterface
        , public std::enable_shared_from_this<MySQL> {
    friend class MySQLManager;
    public:
        using Ptr = std::shared_ptr<MySQL>;
        MySQL(const std::map<std::string, std::string>& args);

        bool Connect();
        bool Ping();

        virtual int32_t Execute(const char* format, ...) override;
        int32_t Execute(const char* format, va_list ap);
        virtual int32_t Execute(const std::string& sql) override;
        virtual int64_t GetLastInsertId() override;
        MySQL::Ptr GetMySQL();
        std::shared_ptr<MYSQL> GetRaw();

        uint64_t GetAffectedRows();

        virtual SQLDataInterface::Ptr Query(const char* format, ...) override;
        SQLDataInterface::Ptr Query(const char* format, va_list ap);
        virtual SQLDataInterface::Ptr Query(const std::string& sql) override;

        virtual StmtInterface::Ptr Prepare(const std::string& stmt) override;
        virtual int32_t GetErrno() override;
        virtual std::string GetErrorStr() override;
        virtual TransactionInterface::Ptr OpenTransaction(bool auto_commit) override;

        template <typename... Args>
        int32_t ExecStmt(const char* stmt, Args&&... args);
        template <typename... Args>
        SQLDataInterface::Ptr QueryStmt(const char* stmt, Args&&... args);
        const char* Cmd();
        bool Use(const std::string& dbname);
    private:
        bool IsNeedCheck();
    private:
        std::map<std::string, std::string> m_params;
        std::shared_ptr<MYSQL> m_mysql;
        std::string m_cmd;
        std::string m_dbname;
        uint64_t m_lastUsedTime;
        bool m_hasError;
        int32_t m_poolSize;
    };

    class MySQLTransaction : public TransactionInterface {
    public:
        using Ptr = std::shared_ptr<MySQLTransaction>;
        static MySQLTransaction::Ptr Create(MySQL::Ptr mysql, bool auto_commit);
        ~MySQLTransaction();

        virtual bool Begin() override;
        virtual bool Commit() override;
        virtual bool RollBack() override;

        virtual int32_t Execute(const char* format, ...) override;
        int32_t Execute(const char* format, va_list ap);
        virtual int32_t Execute(const std::string& sql) override;
        virtual int64_t GetLastInsertId() override;
        std::shared_ptr<MySQL> GetMySQL();

        bool IsAutoCommit() const;
        bool IsFinished() const;
        bool IsError() const;
    private:
        MySQLTransaction(MySQL::Ptr mysql, bool auto_commit);
    private:
        MySQL::Ptr m_mysql;
        bool m_autoCommit;
        bool m_isFinished;
        bool m_hasError;
    };

    class MySQLStmt : public StmtInterface
        , std::enable_shared_from_this<MySQLStmt> {
    public:
        using Ptr = std::shared_ptr<MySQLStmt>;
        static MySQLStmt::Ptr Create(MySQL::Ptr mysql, const std::string& stmt);

        ~MySQLStmt();
        int32_t Bind(int idx, const int8_t& value);
        int32_t Bind(int idx, const uint8_t& value);
        int32_t Bind(int idx, const int16_t& value);
        int32_t Bind(int idx, const uint16_t& value);
        int32_t Bind(int idx, const int32_t& value);
        int32_t Bind(int idx, const uint32_t& value);
        int32_t Bind(int idx, const int64_t& value);
        int32_t Bind(int idx, const uint64_t& value);
        int32_t Bind(int idx, const float& value);
        int32_t Bind(int idx, const double& value);
        int32_t Bind(int idx, const std::string& value);
        int32_t Bind(int idx, const char* value);
        int32_t Bind(int idx, const void* value, int len);
        int32_t Bind(int idx);

        virtual int32_t BindInt8(int idx, const int8_t& value) override;
        virtual int32_t BindUint8(int idx, const uint8_t& value) override;
        virtual int32_t BindInt16(int idx, const int16_t& value) override;
        virtual int32_t BindUint16(int idx, const uint16_t& value) override;
        virtual int32_t BindInt32(int idx, const int32_t& value) override;
        virtual int32_t BindUint32(int idx, const uint32_t& value) override;
        virtual int32_t BindInt64(int idx, const int64_t& value) override;
        virtual int32_t BindUint64(int idx, const uint64_t& value) override;
        virtual int32_t BindFloat(int idx, const float& value) override;
        virtual int32_t BindDouble(int idx, const double& value) override;
        virtual int32_t BindString(int idx, const char* value) override;
        virtual int32_t BindString(int idx, const std::string& value) override;
        virtual int32_t BindBlob(int idx, const void* value, int64_t size) override;
        virtual int32_t BindBlob(int idx, const std::string& value) override;
        virtual int32_t BindTime(int idx, const time_t& value) override;
        virtual int32_t BindNull(int idx) override;

        virtual int32_t Execute() override;
        virtual int64_t GetLastInsertId() override;
        virtual SQLDataInterface::Ptr Query() override;

        virtual int32_t GetErrno() override;
        virtual std::string GetErrorStr() override;

        MYSQL_STMT* GetRaw() const;
    private:
        MySQLStmt(MySQL::Ptr db, MYSQL_STMT* stmt);
    private:
        MySQL::Ptr m_mysql;
        MYSQL_STMT* m_stmt;
        std::vector<MYSQL_BIND> m_binds;
    };

    class MySQLManager {
    public:
        using Ptr = std::shared_ptr<MySQLManager>;
        MySQLManager();
        ~MySQLManager();

        MySQL::Ptr Get(const std::string& name);
        void RegisterMySQL(const std::string name, const std::map<std::string, std::string>& params);
        void CheckConnection(int32_t sec = 30);
        uint32_t GetMaxConn() const;
        void SetMaxConn(uint32_t v);

        int32_t Execute(const std::string& name, const char* format, ...);
        int32_t Execute(const std::string& name, const char* format, va_list ap);
        int32_t Execute(const std::string& name, const std::string& sql);

        SQLDataInterface::Ptr Query(const std::string& name, const char* format, ...);
        SQLDataInterface::Ptr Query(const std::string& name, const char* format, va_list ap);
        SQLDataInterface::Ptr Query(const std::string& name, const std::string& sql);

        MySQLTransaction::Ptr OpenTransaction(const std::string& name, bool auto_commit);
    private:
        void FreeMySQL(const std::string& name, MySQL* mysql);
    private:
        uint32_t m_maxConn;
        Mutex m_mutex;
        std::map<std::string, std::list<MySQL*>> m_conns;
        std::map<std::string, std::map<std::string, std::string>> m_dbDefines;
    };

    class MySQLUtil {
    public:
        static SQLDataInterface::Ptr Query(const std::string& name, const char* format, ...);
        static SQLDataInterface::Ptr Query(const std::string& name, const char* format, va_list ap);
        static SQLDataInterface::Ptr Query(const std::string& name, const std::string& sql);

        static SQLDataInterface::Ptr TryQuery(const std::string& name, uint32_t count, const char* format, ...);
        static SQLDataInterface::Ptr TryQuery(const std::string& name, uint32_t count, const char* format, va_list ap);
        static SQLDataInterface::Ptr TryQuery(const std::string& name, uint32_t count, const std::string& sql);

        static int32_t Execute(const std::string& name, const char* format, ...);
        static int32_t Execute(const std::string& name, const char* format, va_list ap);
        static int32_t Execute(const std::string& name, const std::string& sql);
    
        static int32_t TryExecute(const std::string& name, uint32_t count, const char* format, ...);
        static int32_t TryExecute(const std::string& name, uint32_t count, const char* format, va_list ap);
        static int32_t TryExecute(const std::string& name, uint32_t count, const std::string& sql);
    };

    using MySQLMgr = Singleton<MySQLManager>;

    template<size_t N, typename... Args>
    struct MySQLBinder {
        static int Bind(std::shared_ptr<MySQLStmt> stmt) { return 0; }
    };

    template<typename... Args>
    int BindX(MySQLStmt::Ptr stmt, Args&... args) {
        return MySQLBinder<1, Args...>::Bind(stmt, args...);
    }

    template<typename... Args>
    int32_t MySQL::ExecStmt(const char* stmt, Args&&... args) {
        auto st = MySQLStmt::Create(shared_from_this(), stmt);
        if(!st) {
            return -1;
        }
        int rt = BindX(st, args...);
        if(rt != 0) {
            return rt;
        }
        return st->Execute();
    }

    template<class... Args>
    SQLDataInterface::Ptr MySQL::QueryStmt(const char* stmt, Args&&... args) {
        auto st = MySQLStmt::Create(shared_from_this(), stmt);
        if(!st) {
            return nullptr;
        }
        int rt = BindX(st, args...);
        if(rt != 0) {
            return nullptr;
        }
        return st->Query();
    }

    template<size_t N, typename Head, typename... Tail>
    struct MySQLBinder<N, Head, Tail...> {
        static int Bind(MySQLStmt::Ptr stmt
                        ,const Head&, Tail&...) {
            static_assert(sizeof...(Tail) < 0, "invalid type");
            return 0;
        }
    };

#define XX(type, type2) \
    template<size_t N, typename... Tail>                                \
    struct MySQLBinder<N, type, Tail...> {                              \
        static int Bind(MySQLStmt::Ptr stmt                             \
                        , type2 value                                   \
                        , Tail&... tail) {                              \
            int rt = stmt->Bind(N, value);                              \
            if(rt != 0) {                                               \
                return rt;                                              \
            }                                                           \
            return MySQLBinder<N + 1, Tail...>::Bind(stmt, tail...);    \
        }                                                               \
    };

    XX(char*, char*);
    XX(const char*, char*);
    XX(std::string, std::string&);
    XX(int8_t, int8_t&);
    XX(uint8_t, uint8_t&);
    XX(int16_t, int16_t&);
    XX(uint16_t, uint16_t&);
    XX(int32_t, int32_t&);
    XX(uint32_t, uint32_t&);
    XX(int64_t, int64_t&);
    XX(uint64_t, uint64_t&);
    XX(float, float&);
    XX(double, double&);
#undef XX
}

#endif // ! ____CRAZY_DB_MYSQL_H____