#ifndef __TCAPLUS_SERVICE_TCAPLUS_SQL_RESULT_H__
#define __TCAPLUS_SERVICE_TCAPLUS_SQL_RESULT_H__

#include <stdint.h>
#include "tcaplus_service_log.h"
#include "tcaplus_define.h"

namespace TcaplusCommon
{
    class SqlResultInfo;
    class RowInfo;
    class FieldInfo;
}

namespace TcaplusService 
{
    const int32_t VERSION_FOR_SQL = 1; //索引查询版本号，可用于后续的兼容性

    class Field
    {
     public:
        Field();
        ~Field();
     
     public:
        FieldTypeEnum FieldType();

        int32_t GetBool(bool& _bool);

        int32_t GetInt8(int8_t& p_int8);

        int32_t GetUInt8(uint8_t& _uint8);

        int32_t GetInt16(int16_t& p_int16);

        int32_t GetUInt16(uint16_t& _uint16);

        int32_t GetInt32(int32_t& p_int32);

        int32_t GetUInt32(uint32_t& _uint32);

        int32_t GetInt64(int64_t& p_int64);

        int32_t GetUInt64(uint64_t& _uint64);

        int32_t GetFloat(float& _float);

        int32_t GetDouble(double& _double);

        //buff: 将字符串保存到buff中，buff需要用户分配存储空间，
        //buff_len: 表示用户分配的存储空间大小
        int32_t GetString(char* buff, int32_t buff_len);

        const char* GetValue(int32_t& value_len);

    public:
        int32_t Init(Logger* logger);

        int32_t Set(TcaplusCommon::FieldInfo* field_info);

     private:
        Logger* m_logger;

        TcaplusCommon::FieldInfo* m_field_info;
    };

    class Row
    {
     public:
        Row();
        ~Row();
     
     public:
        int32_t FieldsNum();

        //第一次调用，返回第一个字段，第二次调用，返回第二个字段，以此类推，如果调用次数超出了字段数，则将一直返回最后一个字段信息
        int32_t FetchField(Field*& field);

     public:   
        int32_t Init(Logger* logger);

        int32_t Set(TcaplusCommon::RowInfo* row_info);

     private:
        Logger* m_logger;

        TcaplusCommon::RowInfo* m_row_info;

        Field m_field;
    };

    class SqlResult 
    {
     public:
        SqlResult();
        ~SqlResult();

     public:
        int32_t Init(Logger* logger);

        int32_t Result();

        SqlTypeEnum SqlType();

        int32_t Version();

        int32_t RowsNum();

        //第一次调用，返回第一条记录，第二次调用，返回第二条记录，以此类推，如果调用次数超出了记录数，则将一直返回最后一条记录
        int32_t FetchRow(Row*& row);

     public:   
        int32_t Set(int32_t result, int32_t sql_type, int32_t version, int32_t row_num, const char* value, int32_t value_len);

     private:
        Logger* m_logger;

        TcaplusCommon::SqlResultInfo* m_sql_result_info;

        Row m_row;
    };
}

#endif