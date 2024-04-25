#include "mysql.h"

namespace crazy {

static ConfigValue<std::map<std::string, std::map<std::string, std::string> >>::Ptr g_mysql_dbs
    = Config::Lookup("mysql.dbs", std::map<std::string, std::map<std::string, std::string>> {}
    , "mysql dbs");

struct MySQLThreadIniter {
    MySQLThreadIniter() {
        mysql_thread_init();
    }

    ~MySQLThreadIniter() {
        mysql_thread_end();
    }
};

bool mysql_time_to_time_t(const MYSQL_TIME& mt, time_t& ts) {
    struct tm tm;
    ts = 0;
    localtime_r(&ts, &tm);
    tm.tm_year = mt.year - 1900;
    tm.tm_mon = mt.month - 1;
    tm.tm_mday = mt.day;
    tm.tm_hour = mt.hour;
    tm.tm_min = mt.minute;
    tm.tm_sec = mt.second;
    ts = mktime(&tm);
    if(ts < 0) {
        ts = 0;
    }
    return true;
}
bool time_t_to_mysql_time(const time_t& ts, MYSQL_TIME& mt) {
    struct tm tm;
    localtime_r(&ts, &tm);
    mt.year = tm.tm_year + 1900;
    mt.month = tm.tm_mon + 1;
    mt.day = tm.tm_mday;
    mt.hour = tm.tm_hour;
    mt.minute = tm.tm_min;
    mt.second = tm.tm_sec;
    return true;
} 
static MYSQL* mysql_init(std::map<std::string, std::string>& params,
                         const int& timeout) {

    static thread_local MySQLThreadIniter s_thread_initer;

    MYSQL* mysql = ::mysql_init(nullptr);
    if(mysql == nullptr) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "mysql_init error";
        return nullptr;
    }

    if(timeout > 0) {
        mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    }
    bool close = false;
    mysql_options(mysql, MYSQL_OPT_RECONNECT, &close);
    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    int port = GetParamValue(params, "port", 0);
    std::string host = GetParamValue<std::string>(params, "host");
    std::string user = GetParamValue<std::string>(params, "user");
    std::string passwd = GetParamValue<std::string>(params, "passwd");
    std::string dbname = GetParamValue<std::string>(params, "dbname");

    if(mysql_real_connect(mysql, host.c_str(), user.c_str(), passwd.c_str()
                          ,dbname.c_str(), port, NULL, 0) == nullptr) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "mysql_real_connect(" << host
                                  << ", " << port << ", " << dbname
                                  << ") error: " << mysql_error(mysql);
        mysql_close(mysql);
        return nullptr;
    }
    return mysql;
}
static MYSQL_RES* my_mysql_query(MYSQL* mysql, const char* sql) {
    if(mysql == nullptr) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "mysql_query mysql is null";
        return nullptr;
    }

    if(sql == nullptr) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "mysql_query sql is null";
        return nullptr;
    }

    if(::mysql_query(mysql, sql)) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "mysql_query(" << sql << ") error:"
            << mysql_error(mysql);
        return nullptr;
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if(res == nullptr) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "mysql_store_result() error:"
            << mysql_error(mysql);
    }
    return res;
}
MySQLRes::MySQLRes(MYSQL_RES* res, uint32_t eno, const char* errstr) 
    :m_errno(eno)
    ,m_errstr(errstr)
    ,m_cur(nullptr)
    ,m_curlength(nullptr) {
    if(res) {
        m_data.reset(res, mysql_free_result);
    }
}
MYSQL_RES* MySQLRes::Get() const {
    return m_data.get();
}
int32_t MySQLRes::GetErrno() const {
    return m_errno;
}
const std::string& MySQLRes::GetErrorStr() const {
    return m_errstr;
}
bool MySQLRes::Foreach(data_cb cb) {
    MYSQL_ROW row;
    uint64_t fields = GetColumnCount();
    int32_t i = 0;
    while((row = mysql_fetch_row(m_data.get()))) {
        if(!cb(row, fields, i++)) {
            break;
        }
    }
    return true;
}
int32_t MySQLRes::GetDataCount() {
    return mysql_num_rows(m_data.get());
}
int32_t MySQLRes::GetColumnCount() {
    return mysql_num_fields(m_data.get());
}
int32_t MySQLRes::GetColumnByte(int idx) {
    return m_curlength[idx];
}
int32_t MySQLRes::GetColumnType(int idx) {
    return 0;
}
const std::string MySQLRes::GetColumnName(int idx) {
    return "";
}
bool MySQLRes::IsNull(int32_t idx) {
    if(m_cur[idx] == nullptr) {
        return true;
    }
    return false;
}
int8_t MySQLRes::GetInt8(int32_t idx) {
    return GetInt64(idx);
}
uint8_t MySQLRes::GetUint8(int32_t idx) {
    return GetInt64(idx);
}
int16_t MySQLRes::GetInt16(int32_t idx) {
    return GetInt64(idx);
}
uint16_t MySQLRes::GetUint16(int32_t idx) {
    return GetInt64(idx);
}
int32_t MySQLRes::GetInt32(int32_t idx) {
    return GetInt64(idx);
}
uint32_t MySQLRes::GetUint32(int32_t idx) {
    return GetInt64(idx);
}
int64_t MySQLRes::GetInt64(int32_t idx) {
    return std::atoll(m_cur[idx]);
}
uint64_t MySQLRes::GetUint64(int32_t idx) {
    return GetInt64(idx);
}
float MySQLRes::GetFloat(int32_t idx) {
    return GetDouble(idx);
}
double MySQLRes::GetDouble(int32_t idx) {
    return std::stod(m_cur[idx]);
}
std::string MySQLRes::GetString(int32_t idx) {
    return std::string {m_cur[idx], m_curlength[idx]};
}
std::string MySQLRes::GetBlob(int32_t idx) {
    return std::string {m_cur[idx], m_curlength[idx]};
}
time_t MySQLRes::GetTime(int32_t idx) {
    if(!m_cur[idx]) {
        return 0;
    }
    return Str2Time(m_cur[idx]);
}
bool MySQLRes::Next() {
    m_cur = mysql_fetch_row(m_data.get());
    m_curlength = mysql_fetch_lengths(m_data.get());
    return m_cur;
}
MySQLStmtRes::Ptr MySQLStmtRes::Create(std::shared_ptr<MySQLStmt> stmt) {
    int32_t eno = mysql_stmt_errno(stmt->GetRaw());
    const char* errstr = mysql_stmt_error(stmt->GetRaw());
    MySQLStmtRes::Ptr rt(new MySQLStmtRes(stmt, eno, errstr));
    if(eno) {
        return rt;
    }
    MYSQL_RES* res = mysql_stmt_result_metadata(stmt->GetRaw());
    if(!res) {
        return MySQLStmtRes::Ptr(new MySQLStmtRes(stmt, stmt->GetErrno()
                                 ,stmt->GetErrorStr().c_str()));
    }

    int num = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);

    rt->m_binds.resize(num);
    memset(&rt->m_binds[0], 0, sizeof(rt->m_binds[0]) * num);
    rt->m_datas.resize(num);

    for(int i = 0; i < num; ++i) {
        rt->m_datas[i].type = fields[i].type;
        switch(fields[i].type) {
#define XX(m, t)                                    \
            case m:                                 \
                rt->m_datas[i].Alloc(sizeof(t));    \
                break;
            XX(MYSQL_TYPE_TINY, int8_t);
            XX(MYSQL_TYPE_SHORT, int16_t);
            XX(MYSQL_TYPE_LONG, int32_t);
            XX(MYSQL_TYPE_LONGLONG, int64_t);
            XX(MYSQL_TYPE_FLOAT, float);
            XX(MYSQL_TYPE_DOUBLE, double);
            XX(MYSQL_TYPE_TIMESTAMP, MYSQL_TIME);
            XX(MYSQL_TYPE_DATETIME, MYSQL_TIME);
            XX(MYSQL_TYPE_DATE, MYSQL_TIME);
            XX(MYSQL_TYPE_TIME, MYSQL_TIME);
#undef XX
            default:
                rt->m_datas[i].Alloc(fields[i].length);
                break;
        }

        rt->m_binds[i].buffer_type = rt->m_datas[i].type;
        rt->m_binds[i].buffer = rt->m_datas[i].data;
        rt->m_binds[i].buffer_length = rt->m_datas[i].data_length;
        rt->m_binds[i].length = &rt->m_datas[i].length;
        rt->m_binds[i].is_null = &rt->m_datas[i].is_null;
        rt->m_binds[i].error = &rt->m_datas[i].error;
    }

    if(mysql_stmt_bind_result(stmt->GetRaw(), &rt->m_binds[0])) {
        return MySQLStmtRes::Ptr(new MySQLStmtRes(stmt, stmt->GetErrno()
                                    , stmt->GetErrorStr().c_str()));
    }

    stmt->Execute();

    if(mysql_stmt_store_result(stmt->GetRaw())) {
        return MySQLStmtRes::Ptr(new MySQLStmtRes(stmt, stmt->GetErrno()
                                    , stmt->GetErrorStr().c_str()));
    }
    return rt;
}
MySQLStmtRes::~MySQLStmtRes() {
    if(!m_errno) {
        mysql_stmt_free_result(m_stmt->GetRaw());
    }
}
int32_t MySQLStmtRes::GetErrno() const {
    return m_errno;
}
const std::string& MySQLStmtRes::GetErrorStr() const {
    return m_errstr;
}
int32_t MySQLStmtRes::GetDataCount() {
    return mysql_stmt_num_rows(m_stmt->GetRaw());
}
int32_t MySQLStmtRes::GetColumnCount() {
    return mysql_stmt_field_count(m_stmt->GetRaw());
}
int32_t MySQLStmtRes::GetColumnByte(int idx) {
    return m_datas[idx].length;
}
int32_t MySQLStmtRes::GetColumnType(int idx) {
    return m_datas[idx].type;
}
const std::string MySQLStmtRes::GetColumnName(int idx) {
    return "";
}
bool MySQLStmtRes::IsNull(int32_t idx) {
    return m_datas[idx].is_null;
}
#define XX(type) return *(type*)m_datas[idx].data
int8_t MySQLStmtRes::GetInt8(int32_t idx) {
    XX(int8_t);
}
uint8_t MySQLStmtRes::GetUint8(int32_t idx) {
    XX(uint8_t);
}
int16_t MySQLStmtRes::GetInt16(int32_t idx) {
    XX(int16_t);
}
uint16_t MySQLStmtRes::GetUint16(int32_t idx) {
    XX(uint16_t);
}
int32_t MySQLStmtRes::GetInt32(int32_t idx) {
    XX(int32_t);
}
uint32_t MySQLStmtRes::GetUint32(int32_t idx) {
    XX(uint32_t);
}
int64_t MySQLStmtRes::GetInt64(int32_t idx) {
    XX(int64_t);
}
uint64_t MySQLStmtRes::GetUint64(int32_t idx) {
    XX(uint64_t);
}
float MySQLStmtRes::GetFloat(int32_t idx) {
    XX(float);
}
double MySQLStmtRes::GetDouble(int32_t idx) {
    XX(double);
}
#undef XX
std::string MySQLStmtRes::GetString(int32_t idx) {
    return std::string{m_datas[idx].data, m_datas[idx].length};
}
std::string MySQLStmtRes::GetBlob(int32_t idx) {
    return std::string{m_datas[idx].data, m_datas[idx].length};
}
time_t MySQLStmtRes::GetTime(int32_t idx) {
    MYSQL_TIME* v = (MYSQL_TIME*)m_datas[idx].data;
    time_t ts = 0;
    mysql_time_to_time_t(*v, ts);
    return ts;
}
bool MySQLStmtRes::Next() {
    return !mysql_stmt_fetch(m_stmt->GetRaw());
}
MySQLStmtRes::MySQLStmtRes(std::shared_ptr<MySQLStmt> stmt, int32_t eno, const char* errstr) 
    :m_errno(eno)
    ,m_errstr(errstr)
    ,m_stmt(stmt){
}
MySQLStmtRes::Data::Data()
    :is_null(0)
    ,error(0)
    ,type()
    ,length(0)
    ,data_length(0)
    ,data(nullptr) {
}

MySQLStmtRes::Data::~Data() {
    if(data) {
        delete[] data;
    }
}
void MySQLStmtRes::Data::Alloc(size_t size) {
    if(data) {
        delete[] data;
    }
    data = new char[size]();
    length = size;
    data_length = size;
}
MySQL::MySQL(const std::map<std::string, std::string>& args) 
    :m_params(args)
    ,m_lastUsedTime(0)
    ,m_hasError(false)
    ,m_poolSize(10) {
}
bool MySQL::Connect() {
    if (m_mysql && !m_hasError) {
        return true;
    }
    MYSQL* m = mysql_init(m_params, 0);
    if (!m) {
        m_hasError = true;
        return false;
    }
    m_hasError = false;
    m_poolSize = GetParamValue(m_params, "pool", 5);
    m_mysql.reset(m, mysql_close);
}
bool MySQL::Ping() {
    if (!m_mysql) {
        return false;
    }
    if (mysql_ping(m_mysql.get())) {
        m_hasError = true;
        return false;
    }
    m_hasError = false;
    return true;
}
int32_t MySQL::Execute(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    auto rt = Execute(format, ap);
    va_end(ap);
    return rt;
}
int32_t MySQL::Execute(const char* format, va_list ap) {
    m_cmd = StringUtil::Formatv(format, ap);
    auto rt = ::mysql_query(m_mysql.get(), m_cmd.c_str());
    if (rt) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "cmd = " << Cmd()
            << ", error = " << GetErrorStr();
            m_hasError = true;
    } else {
        m_hasError = false;
    }
    return rt;
}
int32_t MySQL::Execute(const std::string& sql) {
    m_cmd = sql;
    auto rt = ::mysql_query(m_mysql.get(), m_cmd.c_str());
    if (rt) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "cmd = " << Cmd()
            << ", error = " << GetErrorStr();
            m_hasError = true;
    } else {
        m_hasError = false;
    }
    return rt;
}
int64_t MySQL::GetLastInsertId() {
    return mysql_insert_id(m_mysql.get());
}
MySQL::Ptr MySQL::GetMySQL() {
    return MySQL::Ptr(this);
}
std::shared_ptr<MYSQL> MySQL::GetRaw() {
    return m_mysql;
}
uint64_t MySQL::GetAffectedRows() {
    if(!m_mysql) {
        return 0;
    }
    return mysql_affected_rows(m_mysql.get());
}
SQLDataInterface::Ptr MySQL::Query(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    auto rt = Query(format, ap);
    va_end(ap);
    return rt;
}
SQLDataInterface::Ptr MySQL::Query(const char* format, va_list ap) {
    m_cmd = StringUtil::Formatv(format, ap);
    MYSQL_RES* res = my_mysql_query(m_mysql.get(), m_cmd.c_str());
    if(!res) {
        m_hasError = true;
        return nullptr;
    }
    m_hasError = false;
    SQLDataInterface::Ptr rt(new MySQLRes(res, mysql_errno(m_mysql.get())
                        ,mysql_error(m_mysql.get())));
    return rt;
}
SQLDataInterface::Ptr MySQL::Query(const std::string& sql) {
    m_cmd = sql;
    MYSQL_RES* res = my_mysql_query(m_mysql.get(), m_cmd.c_str());
    if(!res) {
        m_hasError = true;
        return nullptr;
    }
    m_hasError = false;
    SQLDataInterface::Ptr rt(new MySQLRes(res, mysql_errno(m_mysql.get())
                        ,mysql_error(m_mysql.get())));
    return rt;
}
StmtInterface::Ptr MySQL::Prepare(const std::string& stmt) {
    return MySQLStmt::Create(shared_from_this(), stmt);
}
int32_t MySQL::GetErrno() {
    if(!m_mysql) {
        return -1;
    }
    return mysql_errno(m_mysql.get());
}
std::string MySQL::GetErrorStr() {
    if(!m_mysql) {
        return "mysql is null";
    }
    const char* str = mysql_error(m_mysql.get());
    if(str) {
        return str;
    }
    return "";
}
TransactionInterface::Ptr MySQL::OpenTransaction(bool auto_commit) {
    return MySQLTransaction::Create(shared_from_this(), auto_commit);
}
const char* MySQL::Cmd() {
    return m_cmd.c_str();
}
bool MySQL::Use(const std::string& dbname) {
    if(!m_mysql) {
        return false;
    }
    if(m_dbname == dbname) {
        return true;
    }
    if(mysql_select_db(m_mysql.get(), dbname.c_str()) == 0) {
        m_dbname = dbname;
        m_hasError = false;
        return true;
    } else {
        m_dbname = "";
        m_hasError = true;
        return false;
    }
}
bool MySQL::IsNeedCheck() {
    if((time(0) - m_lastUsedTime) < 5
            && !m_hasError) {
        return false;
    }
    return true;
}
MySQLTransaction::Ptr MySQLTransaction::Create(MySQL::Ptr mysql, bool auto_commit) {
    MySQLTransaction::Ptr rt(new MySQLTransaction(mysql, auto_commit));
    if(rt->Begin()) {
        return rt;
    }
    return nullptr;
}
MySQLTransaction::~MySQLTransaction() {
    if(m_autoCommit) {
        Commit();
    } else {
        RollBack();
    }
}
bool MySQLTransaction::Begin() {
    int rt = Execute("BEGIN");
    return rt == 0;
}
bool MySQLTransaction::Commit() {
    if(m_isFinished || m_hasError) {
        return !m_hasError;
    }
    int rt = Execute("COMMIT");
    if(rt == 0) {
        m_isFinished = true;
    } else {
        m_hasError = true;
    }
    return rt == 0;
}
bool MySQLTransaction::RollBack() {
    if(m_isFinished) {
        return true;
    }
    int rt = Execute("ROLLBACK");
    if(rt == 0) {
        m_isFinished = true;
    } else {
        m_hasError = true;
    }
    return rt == 0;
}
int32_t MySQLTransaction::Execute(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    return Execute(format, ap);
}
int32_t MySQLTransaction::Execute(const char* format, va_list ap) {
    if(m_isFinished) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "transaction is finished, format=" << format;
        return -1;
    }
    int rt = m_mysql->Execute(format, ap);
    if(rt) {
        m_hasError = true;
    }
    return rt;
}
int32_t MySQLTransaction::Execute(const std::string& sql) {
    if(m_isFinished) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "transaction is finished, sql=" << sql;
        return -1;
    }
    int rt = m_mysql->Execute(sql);
    if(rt) {
        m_hasError = true;
    }
    return rt;
}
int64_t MySQLTransaction::GetLastInsertId() {
    return m_mysql->GetLastInsertId();
}
std::shared_ptr<MySQL> MySQLTransaction::GetMySQL() {
    return m_mysql;
}
bool MySQLTransaction::IsAutoCommit() const {
    return m_autoCommit;
}
bool MySQLTransaction::IsFinished() const {
    return m_isFinished;
}
bool MySQLTransaction::IsError() const {
    return m_hasError;
}
MySQLTransaction::MySQLTransaction(MySQL::Ptr mysql, bool auto_commit) 
    :m_mysql(mysql)
    ,m_autoCommit(auto_commit)
    ,m_isFinished(false)
    ,m_hasError(false) {
}
MySQLStmt::Ptr MySQLStmt::Create(MySQL::Ptr mysql, const std::string& stmt) {
    auto st = mysql_stmt_init(mysql->GetRaw().get());
    if(!st) {
        return nullptr;
    }
    if(mysql_stmt_prepare(st, stmt.c_str(), stmt.size())) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "stmt=" << stmt
            << " errno=" << mysql_stmt_errno(st)
            << " errstr=" << mysql_stmt_error(st);
        mysql_stmt_close(st);
        return nullptr;
    }
    int count = mysql_stmt_param_count(st);
    MySQLStmt::Ptr rt(new MySQLStmt(mysql, st));
    rt->m_binds.resize(count);
    memset(&rt->m_binds[0], 0, sizeof(rt->m_binds[0]) * count);
    return rt;
}
MySQLStmt::~MySQLStmt() {
    if(m_stmt) {
        mysql_stmt_close(m_stmt);
    }

    for(auto& i : m_binds) {
        if(i.buffer) {
            free(i.buffer);
        }
    }
}
int32_t MySQLStmt::Bind(int idx, const int8_t& value) {
    return BindInt8(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const uint8_t& value) {
    return BindUint8(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const int16_t& value) {
    return BindInt16(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const uint16_t& value) {
    return BindUint16(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const int32_t& value) {
    return BindInt32(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const uint32_t& value) {
    return BindUint32(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const int64_t& value) {
    return BindInt64(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const uint64_t& value) {
    return BindUint64(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const float& value) {
    return BindFloat(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const double& value) {
    return BindDouble(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const std::string& value) {
    return BindString(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const char* value) {
    return BindString(idx, value);
}
int32_t MySQLStmt::Bind(int idx, const void* value, int len) {
    return BindBlob(idx, value, len);
}
int32_t MySQLStmt::Bind(int idx) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_NULL;
    return 0;
}
int32_t MySQLStmt::BindInt8(int idx, const int8_t& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_TINY;
#define BIND_COPY(ptr, size)                \
    if(m_binds[idx].buffer == nullptr) {    \
        m_binds[idx].buffer = malloc(size); \
    }                                       \
    memcpy(m_binds[idx].buffer, ptr, size);
    BIND_COPY(&value, sizeof(value));
    m_binds[idx].is_unsigned = false;
    m_binds[idx].buffer_length = sizeof(value);
    return 0;
}
int32_t MySQLStmt::BindUint8(int idx, const uint8_t& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_TINY;
    BIND_COPY(&value, sizeof(value));
    m_binds[idx].is_unsigned = true;
    m_binds[idx].buffer_length = sizeof(value);
    return 0;
}
int32_t MySQLStmt::BindInt16(int idx, const int16_t& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_SHORT;
    BIND_COPY(&value, sizeof(value));
    m_binds[idx].is_unsigned = false;
    m_binds[idx].buffer_length = sizeof(value);
}
int32_t MySQLStmt::BindUint16(int idx, const uint16_t& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_SHORT;
    BIND_COPY(&value, sizeof(value));
    m_binds[idx].is_unsigned = true;
    m_binds[idx].buffer_length = sizeof(value);
    return 0;
}
int32_t MySQLStmt::BindInt32(int idx, const int32_t& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_LONG;
    BIND_COPY(&value, sizeof(value));
    m_binds[idx].is_unsigned = false;
    m_binds[idx].buffer_length = sizeof(value);
    return 0;
}
int32_t MySQLStmt::BindUint32(int idx, const uint32_t& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_LONG;
    BIND_COPY(&value, sizeof(value));
    m_binds[idx].is_unsigned = true;
    m_binds[idx].buffer_length = sizeof(value);
    return 0;
}
int32_t MySQLStmt::BindInt64(int idx, const int64_t& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
    BIND_COPY(&value, sizeof(value));
    m_binds[idx].is_unsigned = false;
    m_binds[idx].buffer_length = sizeof(value);
    return 0;
}
int32_t MySQLStmt::BindUint64(int idx, const uint64_t& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
    BIND_COPY(&value, sizeof(value));
    m_binds[idx].is_unsigned = true;
    m_binds[idx].buffer_length = sizeof(value);
    return 0;
}
int32_t MySQLStmt::BindFloat(int idx, const float& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_FLOAT;
    BIND_COPY(&value, sizeof(value));
    m_binds[idx].buffer_length = sizeof(value);
    return 0;
}
int32_t MySQLStmt::BindDouble(int idx, const double& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_DOUBLE;
    BIND_COPY(&value, sizeof(value));
    m_binds[idx].buffer_length = sizeof(value);
    return 0;
}
int32_t MySQLStmt::BindString(int idx, const char* value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_STRING;
#define BIND_COPY_LEN(ptr, size)                                    \
    if(m_binds[idx].buffer == nullptr) {                            \
        m_binds[idx].buffer = malloc(size);                         \
    } else if((size_t)m_binds[idx].buffer_length < (size_t)size) {  \
        free(m_binds[idx].buffer);                                  \
        m_binds[idx].buffer = malloc(size);                         \
    }                                                               \
    memcpy(m_binds[idx].buffer, ptr, size);                         \
    m_binds[idx].buffer_length = size;
    BIND_COPY_LEN(value, strlen(value));
    return 0;
}
int32_t MySQLStmt::BindString(int idx, const std::string& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_STRING;
    BIND_COPY_LEN(value.c_str(), value.size());
    return 0;
}
int32_t MySQLStmt::BindBlob(int idx, const void* value, int64_t size) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
    BIND_COPY_LEN(value, size);
    return 0;
}
int32_t MySQLStmt::BindBlob(int idx, const std::string& value) {
    idx -= 1;
    m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
    BIND_COPY_LEN(value.c_str(), value.size());
    return 0;
}
int32_t MySQLStmt::BindTime(int idx, const time_t& value) {
    return BindString(idx, Time2Str(value));
}
int32_t MySQLStmt::BindNull(int idx) {
    return Bind(idx);
}
int32_t MySQLStmt::Execute() {
    mysql_stmt_bind_param(m_stmt, &m_binds[0]);
    return mysql_stmt_execute(m_stmt);
}
int64_t MySQLStmt::GetLastInsertId() {
    return mysql_stmt_insert_id(m_stmt);
}
SQLDataInterface::Ptr MySQLStmt::Query() {
    mysql_stmt_bind_param(m_stmt, &m_binds[0]);
    return MySQLStmtRes::Create(shared_from_this());
}
int32_t MySQLStmt::GetErrno() {
    return mysql_stmt_errno(m_stmt);
}
std::string MySQLStmt::GetErrorStr() {
    const char* e = mysql_stmt_error(m_stmt);
    if(e) {
        return e;
    }
    return "";
}
MYSQL_STMT* MySQLStmt::GetRaw() const {
    return m_stmt;
}
MySQLStmt::MySQLStmt(MySQL::Ptr db, MYSQL_STMT* stmt) 
    :m_mysql(db)
    ,m_stmt(stmt){
}
MySQLManager::MySQLManager() 
    :m_maxConn(10) {
    mysql_library_init(0, nullptr, nullptr);
}
MySQLManager::~MySQLManager() {
    mysql_library_end();
    for(auto& i : m_conns) {
        for(auto& n : i.second) {
            delete n;
        }
    }
}
MySQL::Ptr MySQLManager::Get(const std::string& name) {
    Mutex::Guard guard(m_mutex);
    auto it = m_conns.find(name);
    if(it != m_conns.end()) {
        if(!it->second.empty()) {
            MySQL* rt = it->second.front();
            it->second.pop_front();
            if(!rt->IsNeedCheck()) {
                rt->m_lastUsedTime = time(0);
                return MySQL::Ptr(rt, std::bind(&MySQLManager::FreeMySQL,
                            this, name, std::placeholders::_1));
            }
            if(rt->Ping()) {
                rt->m_lastUsedTime = time(0);
                return MySQL::Ptr(rt, std::bind(&MySQLManager::FreeMySQL,
                            this, name, std::placeholders::_1));
            } else if(rt->Connect()) {
                rt->m_lastUsedTime = time(0);
                return MySQL::Ptr(rt, std::bind(&MySQLManager::FreeMySQL,
                            this, name, std::placeholders::_1));
            } else {
                CRAZY_WARN(CRAZY_ROOT_LOGGER()) << "reconnect " << name << " fail";
                return nullptr;
            }
        }
    }
    auto config = g_mysql_dbs->GetValue();
    auto sit = config.find(name);
    std::map<std::string, std::string> args;
    if(sit != config.end()) {
        args = sit->second;
    } else {
        sit = m_dbDefines.find(name);
        if(sit != m_dbDefines.end()) {
            args = sit->second;
        } else {
            return nullptr;
        }
    }
    MySQL* rt = new MySQL(args);
    if(rt->Connect()) {
        rt->m_lastUsedTime = time(0);
        return MySQL::Ptr(rt, std::bind(&MySQLManager::FreeMySQL,
                    this, name, std::placeholders::_1));
    } else {
        delete rt;
        return nullptr;
    }
}
void MySQLManager::RegisterMySQL(const std::string name, const std::map<std::string, std::string>& params) {
    Mutex::Guard guard(m_mutex);
    m_dbDefines[name] = params;
}
void MySQLManager::CheckConnection(int32_t sec) {
    time_t now = time(0);
    std::vector<MySQL*> conns;
    Mutex::Guard guard(m_mutex);
    for(auto& i : m_conns) {
        for(auto it = i.second.begin();
                it != i.second.end();) {
            if((int)(now - (*it)->m_lastUsedTime) >= sec) {
                auto tmp = *it;
                i.second.erase(it++);
                conns.push_back(tmp);
            } else {
                ++it;
            }
        }
    }
    for(auto& i : conns) {
        delete i;
    }
}
uint32_t MySQLManager::GetMaxConn() const {
    return m_maxConn;
}
void MySQLManager::SetMaxConn(uint32_t v) {
    m_maxConn = v;
}
int32_t MySQLManager::Execute(const std::string& name, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int rt = Execute(name, format, ap);
    va_end(ap);
    return rt;
}
int32_t MySQLManager::Execute(const std::string& name, const char* format, va_list ap) {
    auto conn = Get(name);
    if(!conn) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "MySQLManager::execute, get(" << name
            << ") fail, format=" << format;
        return -1;
    }
    return conn->Execute(format, ap);
}
int32_t MySQLManager::Execute(const std::string& name, const std::string& sql) {
    auto conn = Get(name);
    if(!conn) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "MySQLManager::execute, get(" << name
            << ") fail, sql=" << sql;
        return -1;
    }
    return conn->Execute(sql);
}
SQLDataInterface::Ptr MySQLManager::Query(const std::string& name, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    auto res = Query(name, format, ap);
    va_end(ap);
    return res;
}
SQLDataInterface::Ptr MySQLManager::Query(const std::string& name, const char* format, va_list ap) {
    auto conn = Get(name);
    if(!conn) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "MySQLManager::query, get(" << name
            << ") fail, format=" << format;
        return nullptr;
    }
    return conn->Query(format, ap);
}
SQLDataInterface::Ptr MySQLManager::Query(const std::string& name, const std::string& sql) {
    auto conn = Get(name);
    if(!conn) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "MySQLManager::query, get(" << name
            << ") fail, sql=" << sql;
        return nullptr;
    }
    return conn->Query(sql);
}
MySQLTransaction::Ptr MySQLManager::OpenTransaction(const std::string& name, bool auto_commit) {
    auto conn = Get(name);
    if(!conn) {
        CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "MySQLManager::openTransaction, get(" << name
            << ") fail";
        return nullptr;
    }
    MySQLTransaction::Ptr trans(MySQLTransaction::Create(conn, auto_commit));
    return trans;
}
void MySQLManager::FreeMySQL(const std::string& name, MySQL* mysql) {
    if(mysql->m_mysql) {
        Mutex::Guard guard(m_mutex);
        if(m_conns[name].size() < (size_t)mysql->m_poolSize) {
            m_conns[name].push_back(mysql);
            return;
        }
    }
    delete mysql;
}
SQLDataInterface::Ptr MySQLUtil::Query(const std::string& name, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    auto rpy = Query(name, format, ap);
    va_end(ap);
    return rpy;
}
SQLDataInterface::Ptr MySQLUtil::Query(const std::string& name, const char* format, va_list ap) {
    auto m = MySQLMgr::GetInstance().Get(name);
    if(!m) {
        return nullptr;
    }
    return m->Query(format, ap);
}
SQLDataInterface::Ptr MySQLUtil::Query(const std::string& name, const std::string& sql) {
    auto m = MySQLMgr::GetInstance().Get(name);
    if(!m) {
        return nullptr;
    }
    return m->Query(sql);
}
SQLDataInterface::Ptr MySQLUtil::TryQuery(const std::string& name, uint32_t count, const char* format, ...) {
    for(uint32_t i = 0; i < count; ++i) {
        va_list ap;
        va_start(ap, format);
        auto rpy = Query(name, format, ap);
        va_end(ap);
        if(rpy) {
            return rpy;
        }
    }
    return nullptr;
}
SQLDataInterface::Ptr MySQLUtil::TryQuery(const std::string& name, uint32_t count, const char* format, va_list ap) {
    for(uint32_t i = 0; i < count; ++i) {
        auto rpy = Query(name, format, ap);
        if(rpy) {
            return rpy;
        }
    }
    return nullptr;
}
SQLDataInterface::Ptr MySQLUtil::TryQuery(const std::string& name, uint32_t count, const std::string& sql) {
    for(uint32_t i = 0; i < count; ++i) {
        auto rpy = Query(name, sql);
        if(rpy) {
            return rpy;
        }
    }
    return nullptr;
}
int32_t MySQLUtil::Execute(const std::string& name, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    auto rpy = Execute(name, format, ap);
    va_end(ap);
    return rpy;
}
int32_t MySQLUtil::Execute(const std::string& name, const char* format, va_list ap) {
    auto m = MySQLMgr::GetInstance().Get(name);
    if(!m) {
        return -1;
    }
    return m->Execute(format, ap);
}
int32_t MySQLUtil::Execute(const std::string& name, const std::string& sql) {
    auto m = MySQLMgr::GetInstance().Get(name);
    if(!m) {
        return -1;
    }
    return m->Execute(sql);
}
int32_t MySQLUtil::TryExecute(const std::string& name, uint32_t count, const char* format, ...) {
    int rpy = 0;
    for(uint32_t i = 0; i < count; ++i) {
        va_list ap;
        va_start(ap, format);
        rpy = Execute(name, format, ap);
        va_end(ap);
        if(!rpy) {
            return rpy;
        }
    }
    return rpy;
}
int32_t MySQLUtil::TryExecute(const std::string& name, uint32_t count, const char* format, va_list ap) {
    int rpy = 0;
    for(uint32_t i = 0; i < count; ++i) {
        rpy = Execute(name, format, ap);
        if(!rpy) {
            return rpy;
        }
    }
    return rpy;
}
int32_t MySQLUtil::TryExecute(const std::string& name, uint32_t count, const std::string& sql) {
    int rpy = 0;
    for(uint32_t i = 0; i < count; ++i) {
        rpy = Execute(name, sql);
        if(!rpy) {
            return rpy;
        }
    }
    return rpy;
}

}