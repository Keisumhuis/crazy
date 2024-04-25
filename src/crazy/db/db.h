/**
 * @file db.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_DB_DB_H____
#define ____CRAZY_DB_DB_H____

#include <stdint.h>

#include <memory>
#include <string>

namespace crazy {

    class SQLDataInterface {
    public :
        using Ptr = std::shared_ptr<SQLDataInterface>;
        virtual ~SQLDataInterface() {}

        virtual int32_t GetErrno() const = 0;
        virtual const std::string& GetErrorStr() const = 0;

        virtual int32_t GetDataCount() = 0;
        virtual int32_t GetColumnCount() = 0;
        virtual int32_t GetColumnByte(int idx) = 0;
        virtual int32_t GetColumnType(int idx) = 0;
        virtual const std::string GetColumnName(int idx) = 0;

        virtual bool IsNull(int32_t idx) = 0;
        virtual int8_t GetInt8(int32_t idx) = 0;
        virtual uint8_t GetUint8(int32_t idx) = 0;
        virtual int16_t GetInt16(int32_t idx) = 0;
        virtual uint16_t GetUint16(int32_t idx) = 0;
        virtual int32_t GetInt32(int32_t idx) = 0;
        virtual uint32_t GetUint32(int32_t idx) = 0;
        virtual int64_t GetInt64(int32_t idx) = 0;
        virtual uint64_t GetUint64(int32_t idx) = 0;
        virtual float GetFloat(int32_t idx) = 0;
        virtual double GetDouble(int32_t idx) = 0;
        virtual std::string GetString(int32_t idx) = 0;
        virtual std::string GetBlob(int32_t idx) = 0;
        virtual time_t GetTime(int32_t idx) = 0;
        virtual bool Next() = 0;
    };

    class SQLUpdateInterface {
    public:
        using Ptr = std::shared_ptr<SQLUpdateInterface>;
        virtual ~SQLUpdateInterface() {}
        virtual int32_t Execute(const char* format, ...) = 0;
        virtual int32_t Execute(const std::string& sql) = 0;
        virtual int64_t GetLastInsertId() = 0;
    };

    class SQLQueryInterface {
    public:
        using Ptr = std::shared_ptr<SQLQueryInterface>;
        virtual ~SQLQueryInterface() {}
        virtual SQLDataInterface::Ptr Query(const char* format, ...) = 0;
        virtual SQLDataInterface::Ptr Query(const std::string& sql) = 0;
    };

    class StmtInterface {
    public:
        using Ptr = std::shared_ptr<StmtInterface>;
        virtual ~StmtInterface() {}
        virtual int32_t BindInt8(int idx, const int8_t& value) = 0;
        virtual int32_t BindUint8(int idx, const uint8_t& value) = 0;
        virtual int32_t BindInt16(int idx, const int16_t& value) = 0;
        virtual int32_t BindUint16(int idx, const uint16_t& value) = 0;
        virtual int32_t BindInt32(int idx, const int32_t& value) = 0;
        virtual int32_t BindUint32(int idx, const uint32_t& value) = 0;
        virtual int32_t BindInt64(int idx, const int64_t& value) = 0;
        virtual int32_t BindUint64(int idx, const uint64_t& value) = 0;
        virtual int32_t BindFloat(int idx, const float& value) = 0;
        virtual int32_t BindDouble(int idx, const double& value) = 0;
        virtual int32_t BindString(int idx, const char* value) = 0;
        virtual int32_t BindString(int idx, const std::string& value) = 0;
        virtual int32_t BindBlob(int idx, const void* value, int64_t size) = 0;
        virtual int32_t BindBlob(int idx, const std::string& value) = 0;
        virtual int32_t BindTime(int idx, const time_t& value) = 0;
        virtual int32_t BindNull(int idx) = 0;

        virtual int32_t Execute() = 0;
        virtual int64_t GetLastInsertId() = 0;
        virtual SQLDataInterface::Ptr Query() = 0;

        virtual int32_t GetErrno() = 0;
        virtual std::string GetErrorStr() = 0;
    };

    class TransactionInterface : public SQLUpdateInterface {
    public:
        using Ptr = std::shared_ptr<TransactionInterface>;
        virtual ~TransactionInterface() {}
        virtual bool Begin() = 0;
        virtual bool Commit() = 0;
        virtual bool RollBack() = 0;
    };

    class DBInterface : public SQLUpdateInterface
        , public SQLQueryInterface {
    public:
        using Ptr = std::shared_ptr<DBInterface>;
        virtual ~DBInterface() {}

        virtual StmtInterface::Ptr Prepare(const std::string& stmt) = 0;
        virtual int32_t GetErrno() = 0;
        virtual std::string GetErrorStr() = 0;
        virtual TransactionInterface::Ptr OpenTransaction(bool auto_commit) = 0;
    };
}

#endif // ! ____CRAZY_DB_DB_H____