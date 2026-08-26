/**********************************************************************
 * Copyright (c)             : 2011 - 2016 Tencent. All Rights Reserved.
 * File                      : tcaplus_service_record.h
 * TcaplusServiceApi Version : 3.18.0.
 * Description               : TCaplus Service API for record
 * modification history
 * ---------------------------------
 * Author                    : tcaplus
 * Date                      : 2016/11/25 
 * ---------------------------------
 * 
 **********************************************************************/
#ifndef __TCAPLUS_SERVICE_TCAPLUS_SERVICE_RECORD_H__
#define __TCAPLUS_SERVICE_TCAPLUS_SERVICE_RECORD_H__

#include "tcaplus_public_define.h"
#include "tcaplus_service_nonecopyable.h"
#include "comm/shtable.h"
#include "tdr_cpp_files/TdrObjectForTcaplus.h"

//#include <tdr/tdr.h>

class TCaplusKeySet;
class TCaplusValueSet_;
class ProtobufValueSet_;
namespace tcaplus_protocol_cs
{
class TCaplusUpdFieldSet;
class TCaplusNameSet;
}

class TCaplusValueSet_;
class TCaplusValueField;

namespace TCAPLUS_KV
{
class TcaplusKVApi;
};

namespace TCAPLUS_COROUTINE_PB
{
class TcaplusCoroutinePbApi;
};

namespace TCAPLUS_ASYNC_PB
{
class TcaplusAsyncPbApi;
};

namespace TCAPREST
{
class CRestHttpMsgProcessor;
};

namespace TcaplusApiTool
{
    class ShardTraversal;
    class ShardSysGet;
};
namespace Client
{
class ClientCmdSelect;
class ClientCmdDelete;
class ClientCmdOmsSelect;
}; // namespace Client


namespace TcaplusService
{

class TcaplusServiceFieldPacker;
class TcaplusServiceFieldUnpacker;
class TcaplusRecordBroker;

/**
 * 用于封装Record的属性
 */
struct RecordProperties
{
    int32_t version;            //Generic表:Record的版本,List表：Record所在List的版本
};

/**
@brief 数据记录对象类，类似于mysql中Row的概念。
*/
class TcaplusServiceRecord : public NoneCopyable
{
public:
    /**
    @brief  设置记录版本号
    @param [IN] iVersion     数据记录的版本号:  <=0 表示不关注版本号不关心版本号。具体含义如下。
                当class TcaplusServiceRequest的int SetCheckDataVersionPolicy(enum tagCHECKDATAVERSIONTYPE type)函数传入的参数type的值为CHECKDATAVERSION_AUTOINCREASE时: 表示检测记录版本号。如果class TcaplusServiceRecord的void SetVersion(IN int32_t iVersion)函数传入的参数iVersion的值<=0,则仍然表示不关心版本号不关注版本号；如果class TcaplusServiceRecord的void SetVersion(IN int32_t iVersion)函数传入的参数iVersion的值>0，那么只有当该版本号与服务器端的版本号相同时，Replace, Update, Increase, ListAddAfter, ListDelete, ListReplace, ListDeleteBatch操作才会成功同时在服务器端该版本号会自增1。
                当class TcaplusServiceRequest的int SetCheckDataVersionPolicy(enum tagCHECKDATAVERSIONTYPE type)函数传入的参数type的值为NOCHECKDATAVERSION_OVERWRITE时: 表示不检测记录版本号。如果class TcaplusServiceRecord的void SetVersion(IN int32_t iVersion)函数传入的参数iVersion的值<=0,则会把版本号1写入服务端的数据记录版本号(服务器端成功写入的数据记录的版本号最少为1)；如果class TcaplusServiceRecord的void SetVersion(IN int32_t iVersion)函数传入的参数iVersion的值>0，那么会把该版本号写入服务端的数据记录版本号。
                当class TcaplusServiceRequest的int SetCheckDataVersionPolicy(enum tagCHECKDATAVERSIONTYPE type)函数传入的参数type的值为NOCHECKDATAVERSION_AUTOINCREASE时: 表示不检测记录版本号，将服务器端的数据记录版本号自增1，若服务器端新写入数据记录则新写入的数据记录的版本号为1。
    @retval void
    */
    //Attention, 注意，对于Generic操作表示设置Record的版本，对于List操作表示设置Record
    //所在List单元的版本。
    void SetVersion(IN int32_t iVersion);

    /**
    @brief  获取记录版本号
    @retval 记录版本号
    @note 对于Generic操作表示获取Record的版本；对于List操作表示获取Record所在List的版本。
    */
    int32_t GetVersion() const;

    /**
    @brief  可视化输出记录内容
    @param [IN] buffer          缓冲区指针
    @param [IN] buffer_size     缓冲区大小
    @retval 可视化的缓冲区指针，内容以'\0'结尾
    */
    const char* Print(IN char* buffer, IN size_t buffer_size);

    /**
    @brief  基于TDR描述设置record数据
    @param [IN] data_buffer     数据缓冲区
    @param [IN] data_size       数据缓冲区大小
    @param [IN] data_version    数据版本号，参看SetVersion函数。默认值-1表示写操作时不校验版本号, 不关心版本号. data_version为<=0都表示写操作时不会校验版本号, 不关心版本号
    @param [IN] data_meta       数据缓冲区的数据对应的meta描述；NULL则自动使用request设置的默认meta信息，参看对应的SetTable函数。
    @param [IN] partkey_index_name  部分键查询的索引名称。
    @retval 0                  设置成功
    @retval <0                 设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t SetData(IN const void* data_buffer, IN size_t data_size,
        IN int32_t data_version = -1, IN LPTDRMETA data_meta = NULL,
        IN const char* partkey_index_name = NULL);

    /**
    @brief  基于TDR 2.0描述设置record数据
    @param [IN] obj            数据对象
    @param [IN] field_names    数据对象需要设置的字段列表(不可重复)；NULL则自动使用ITdrObjectForTcaplus的所有一级字段，参看ITdrObjectForTcaplus::getFirstLevelFieldNames函数。
    @param [IN] field_cnt      数据对象需要设置的字段数量；0则自动使用ITdrObjectForTcaplus的所有一级字段，参看ITdrObjectForTcaplus::getFirstLevelFieldNames函数。
    @param [IN] partkey_index_name  部分键查询的索引名称。
    @retval 0                  设置成功
    @retval <0                 设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t SetData(const apollo::ITdrObjectForTcaplus& obj, const char* field_names[] = NULL, size_t field_cnt = 0, 
        const char* partkey_index_name = NULL);

    /**
    @brief  基于TDR描述设置record数据
    @param [OUT] data_buffer    数据缓冲区；必须先通过 memset 或者 tdr_init 对其进行初始化
    @param [IN] data_size      数据缓冲区大小
    @param [OUT] data_version   数据版本号，参看GetVersion函数。
    @param [IN] data_meta       要取的数据对应的meta描述；NULL则自动使用response设置的默认meta信息，参看对应的SetTable函数。
    @retval 0                  设置成功
    @retval <0                 设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetData(OUT void* data_buffer, IN size_t data_buffer_size,
        OUT int32_t* data_version = NULL, IN LPTDRMETA data_meta = NULL) const;

    /**
    @brief  基于TDR2.0描述, 将record里的数据转换为对应的ITdrObjectForTcaplus对象
    @param [OUT] obj      ITdrObjectForTcaplus对象
    @retval 0                  转换成功
    @retval <0                 转换失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetData(OUT apollo::ITdrObjectForTcaplus& obj) const;

	/**
	   @brief  获取记录中指定字段的版本号
	   @param [IN] field_name    需要查询的字段名
	   @param [OUT] version       记录中字段的版本号
	   @retval >0				 指定字段的版本号
	   @retval <=0		      获取指定字段的版本号失败
	   */
	int GetFieldValueVersion(IN const char* field_name);


public:
    /**
    @brief  通用的key字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容，最大长度1024字节
    @param [IN] value_size           字段内容长度
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetKey(IN const char* field_name, IN const void * field_value, IN const size_t value_size);

    /**
    @brief  int8_t类型key字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetKeyInt8(IN const char* field_name, IN const int8_t field_value);

    /**
    @brief  int16_t类型key字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetKeyInt16(IN const char* field_name, IN const int16_t field_value);

    /**
    @brief  int32_t类型key字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetKeyInt32(IN const char* field_name, IN const int32_t field_value);

    /**
    @brief  int64_t类型key字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetKeyInt64(IN const char* field_name, IN const int64_t field_value);

    /**
    @brief  float类型key字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetKeyFloat(IN const char* field_name, IN const float field_value);

    /**
    @brief  double类型key字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetKeyDouble(IN const char* field_name, IN const double field_value);

    /**
    @brief  string类型key字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容，最大长度1024字节，以'\0'结尾
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetKeyStr(IN const char* field_name, IN const char* field_value);

    /**
    @brief  blob类型key字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容，最大长度1024字节
    @param [IN] value_size           字段内容长度
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetKeyBlob(IN const char* field_name, IN const char* field_value, IN const size_t value_size);

public:
    /**
    @brief  通用的value字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容，最大长度128KB(Tcapsvr>=3.24.0时,最大长度支持256KB)
    @param [IN] value_size           字段内容长度
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetValue(IN const char* field_name, IN const void * field_value, IN const size_t value_size);

    /**
    @brief  int8_t类型value字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetValueInt8(IN const char* field_name, IN const int8_t field_value);

    /**
    @brief  int16_t类型value字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetValueInt16(IN const char* field_name, IN const int16_t field_value);

    /**
    @brief  int32_t类型value字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetValueInt32(IN const char* field_name, IN const int32_t field_value);

    /**
    @brief  int64_t类型value字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetValueInt64(IN const char* field_name, IN const int64_t field_value);

    /**
    @brief  float类型value字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetValueFloat(IN const char* field_name, IN const float field_value);

    /**
    @brief  double类型value字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetValueDouble(IN const char* field_name, IN const double field_value);

    /**
    @brief  string类型value字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容，最大长度128K字节(Tcapsvr>=3.24.0时,最大长度支持256KB)，以'\0'结尾
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetValueStr(IN const char* field_name, IN const char* field_value);

    /**
    @brief  blob类型value字段内容设置
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] field_value         字段内容，最大长度128K字节(Tcapsvr>=3.24.0时,最大长度支持256KB)
                                    注意:对于Blob,系统预留了2B用于版本号建设,因此,写入时数据不要超过(最大长度-2B)
    @param [IN] value_size          字段内容长度
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int SetValueBlob(IN const char* field_name, IN const char* field_value, IN const size_t value_size);

public:
    /**
    @brief 加入要操作的Value字段名称及操作类型，若对应字段名之前已存在，则覆盖之。
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [IN] operation          	操作类型，具体参见 \link TCaplusApiOperation \endlink
    @param [IN] lower_limit         操作结果值下限，假设服务器端操作后的结果值比这个值小，则返回 TcapErrCode::SVR_ERR_FAIL_OUT_OF_USER_DEF_RANGE 并且服务器端放弃更新。
    @param [IN] upper_limit         操作结果值上限，假设服务器端操作后的结果值比这个值大，返回 TcapErrCode::SVR_ERR_FAIL_OUT_OF_USER_DEF_RANGE 并且服务器端放弃更新。
    @retval 0                       设置成功
    @retval 非0                     设置失败，具体错误参见 \link ErrorCode \endlink
    @note                           lower_limit == upper_limit 时，存储端不对操作结果进行范围检测
    */
    int AddValueOperation(IN const char* field_name, IN TCaplusApiOperation operation, IN int64_t lower_limit=0, IN int64_t upper_limit=0);

public:
    /**
    @brief  通用的key字段内容获取
    @param  [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容，最大长度1024字节
    @param  [OUT] value_size         字段内容长度
    @retval 0                        获取成功
    @retval 非0                      获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKey(IN const char* field_name, OUT void *& field_value, OUT size_t & value_size) const;

    /**
    @brief  int8_t类型的key字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value       字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKeyInt8(IN const char* field_name, OUT int8_t & field_value) const;

    /**
    @brief  int16_t类型的key字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value       字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKeyInt16(IN const char* field_name, OUT int16_t & field_value) const;

    /**
    @brief  int32_t类型的key字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [OUT] field_value        字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKeyInt32(IN const char* field_name, OUT int32_t & field_value) const;

    /**
    @brief  int64_t类型的key字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKeyInt64(IN const char* field_name, OUT int64_t & field_value) const;

    /**
    @brief  float类型的key字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKeyFloat(IN const char* field_name, OUT float & field_value) const;

    /**
    @brief  double类型的key字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKeyDouble(IN const char* field_name, OUT double & field_value) const;

    /**
    @brief  string类型的key字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param [OUT] field_value        字段内容，最大长度1024字节，以'\0'结尾
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKeyStr(IN const char* field_name, OUT const char*& field_value) const;

    /**
    @brief  blob类型的key字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容，最大长度1024字节
    @param  [OUT] value_size          字段内容长度
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKeyBlob(IN const char* field_name, OUT const char*& field_value, OUT size_t & value_size) const;

    /**
    @brief  key字段数目获取
    @retval 0                       Key字段(Key Field)数目
    */
    uint32_t GetKeyCount() const;

    /**
    @brief  key字段内容获取
    @param [IN] fieldIndex          字段下标，其有效值必须大于等于0并且小于GetKeyNum()
    @param  [OUT] field_name        字段名称，是字符串
    @param  [OUT] field_value        字段内容，最大长度1024字节
    @param  [OUT] value_size          字段内容长度
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKey(IN uint32_t fieldIndex, OUT const char*& field_name, OUT const void *& field_value, OUT size_t& fieldValueLen) const;

public:
    /**
    @brief  通用的value字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容，最大长度128K字节
    @param  [OUT] value_size          字段内容长度
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValue(IN const char* field_name, OUT void *& field_value, OUT size_t & value_size) const;

    /**
    @brief  int8_t类型的value字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValueInt8(IN const char* field_name, OUT int8_t & field_value) const;

    /**
    @brief  int16_t类型的value字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValueInt16(IN const char* field_name, OUT int16_t & field_value) const;

    /**
    @brief  int32_t类型的value字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValueInt32(IN const char* field_name, OUT int32_t & field_value) const;

    /**
    @brief  int64_t类型的value字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValueInt64(IN const char* field_name, OUT int64_t & field_value) const;

    /**
    @brief  float类型的value字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValueFloat(IN const char* field_name, OUT float & field_value) const;

    /**
    @brief  double类型的value字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValueDouble(IN const char* field_name, OUT double & field_value) const;

    /**
    @brief  string类型的value字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容，最大长度128K字节(Tcapsvr>=3.24.0时,最大长度支持256KB)，以'\0'结尾
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValueStr(IN const char* field_name, OUT const char*& field_value) const;

    /**
    @brief  string类型的value字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value       字段内容，最大长度128K字节(Tcapsvr>=3.24.0时,最大长度支持256KB)，以'\0'结尾,
                                    线上数据存在不以\0结束的字段， 这里回传二进制的长度
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValueStr(IN const char* field_name, OUT const char*& field_value, size_t& size) const;

    /**
    @brief  blob类型的value字段内容获取
    @param [IN] field_name          字段名称，最大长度32字节，以'\0'结尾
    @param  [OUT] field_value        字段内容，最大长度128K字节(Tcapsvr>=3.24.0时,最大长度支持256KB)
    @param  [OUT] value_size          字段内容长度
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValueBlob(IN const char* field_name, OUT const char*& field_value, OUT size_t & value_size) const;

    /**
    @brief    Value字段数目获取
     *    \retval 0                        Value字段(Value Field)数目
    */
    uint32_t GetValueCount() const;

    /**
    @brief   Value字段内容获取
     *   \param [IN] fieldIndex            字段下标，其有效值必须大于等于0并且小于GetValueNum()
     *   \param  [OUT] field_name        字段名称，是字符串
     *   \param  [OUT] field_value        字段内容，最大长度128K字节(Tcapsvr>=3.24.0时,最大长度支持256KB)
     *   \param  [OUT] value_size            字段内容长度
     *   \retval 0                        获取成功
     *   \retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetValue(IN int fieldIndex, OUT const char*& field_name, OUT const void *& field_value, OUT size_t& fieldValueLen) const;

    /**
    @brief  获取记录下标或索引(index)
    @retval 对于Generic类型操作返回该记录在记录数组中的下标; 对于List类型操作返回该记录在List中的索引(index)。Index值为-1则表示无效的记录下标或索。特别的，对于List表的addafter操作该方法可以获得新加list元素的index。
    */
    int32_t GetIndex() const;

    /**
    @brief  获取tdr表结构的版本号
    @retval 返回注册表时，填充的tdr表结构的版本号；注册表时，如果未指定tdr表结构则返回0
    */
    int32_t GetTdrMetaVersion() const;

	/**
	@brief	获取记录上一次的访问时间
	@retval   记录上一次的访问时间
	@note 该函数仅对Generic表的Get, Replace, TableTraverse, GetByPartKey 操作有效
	*/
//	uint64_t GetLastAccessTime() const;

    TCaplusKeySet* GetKeySet() const {return m_key;}
friend class TcaplusServiceRequest;
friend class TcaplusServiceResponse;
friend class TCAPLUS_KV::TcaplusKVApi;
friend class TCAPLUS_COROUTINE_PB::TcaplusCoroutinePbApi;
friend class TCAPLUS_ASYNC_PB::TcaplusAsyncPbApi;
friend class TCAPREST::CRestHttpMsgProcessor;
friend class LatencyDataMgr;
friend class TcaplusServiceFieldUnpacker;
friend class TcaplusServiceFieldPacker;
friend class TcaplusServiceRecordForAnalyticDumpRecord;
friend class Client::ClientCmdSelect;
friend class Client::ClientCmdDelete;
friend class Client::ClientCmdOmsSelect;
friend class TcaplusApiTool::ShardTraversal;
friend class TcaplusApiTool::ShardSysGet;

private:
    /**
    @brief  重置记录内容
    @retval void
    */
    void Reset(int cmd);

    /**
     * \param data_meta: meta of table, NULL表示与meta无关, 如TLV模式; data_meta_v2 王者使用GetDataV2接口使用
     */
    int  SetTable(const char* table_name, LPTDRMETA data_meta, LPTDRMETA data_meta_v2=NULL);
    bool CheckPairList(IN LPTDRMETA data_meta) const;
    bool CheckFieldRange(IN const char* field_name, int64_t lower_limit, int64_t upper_limit) const;
    int  MakeFieldPairList();
    bool IsCompatibleMeta(IN LPTDRMETA data_meta) const;

    int  FillRecord4Get(IN LPTDRMETAENTRY entry, IN const void* data);
    int  FillRecord4Insert(IN LPTDRMETAENTRY entry, IN const void* data);
    int  FillRecord4Increase(IN LPTDRMETAENTRY entry, IN const void* data);
    int  FillRecord4Delete(IN LPTDRMETAENTRY entry, IN const void* data);
    int  FillRecord4BatchGet(IN LPTDRMETAENTRY entry, IN const void* data);
    int  FillRecord4PartKeyGet(IN LPTDRMETAENTRY entry, IN const void* data, void* pstPartKeyIndex);
    int  FillRecord4PartKeyUpdate(IN LPTDRMETAENTRY entry, IN const void* data, void* pstPartKeyIndex);
    int  FillRecord4PartKeyDelete(IN LPTDRMETAENTRY entry, IN const void* data, void* pstPartKeyIndex);
    int  FillRecord4FieldOp(IN LPTDRMETAENTRY entry, IN const void* data);
    int  FillRecord4BatchFieldGet(IN LPTDRMETAENTRY entry, IN const void* data);
    int32_t SetDataForInsertByPartKey(const void* data_buffer, size_t data_size, int32_t data_version, LPTDRMETA data_meta,
                                      const char* partkey_index_name);
	
    int  FillRecord4Get(const apollo::ITdrObjectForTcaplus& obj, const char* field_name, const apollo::TdrFieldInfo& field_info);
    int  FillRecord4Insert(const apollo::ITdrObjectForTcaplus& obj, const char* field_name, const apollo::TdrFieldInfo& field_info);
    int  FillRecord4Increase(const apollo::ITdrObjectForTcaplus& obj, const char* field_name, const apollo::TdrFieldInfo& field_info);
    int  FillRecord4Delete(const apollo::ITdrObjectForTcaplus& obj, const char* field_name, const apollo::TdrFieldInfo& field_info);
    int  FillRecord4BatchGet(const apollo::ITdrObjectForTcaplus& obj, const char* field_name, const apollo::TdrFieldInfo& field_info);
    int  FillRecord4PartKeyGet(const apollo::ITdrObjectForTcaplus& obj, const char* field_name, const apollo::TdrFieldInfo& field_info, 
                               const char** index_field_names, const size_t index_field_cnt);
	
    void UpdateIncreaseValue(
            IN const char* field_name,
            const char* buf,
            int len,
            INOUT tcaplus_protocol_cs::TCaplusUpdFieldSet& increase_value_set,
            int& flag);

    /**
    @brief  设置SplitTableKeyBuff数据, 内部用
    @param [IN] splitTableKeyBuff  Splittablekey数据缓冲区指针
    @param [IN] splitTableKeyBuffLen Splittablekey数据缓冲区长度，支持的最大长度为TCAPLUS_MAX_KEY_FIELD_LEN
    @retval 0    设置成功
    @retval <0   失败，返回对应的错误码。
    */
    int SetSplitTableKeyBuff(IN const char* splitTableKeyBuff, IN const size_t splitTableKeyBuffLen);
    
    //Default ctor and dtor
    TcaplusServiceRecord(Logger* logger, int module_id);
    ~TcaplusServiceRecord();

    //Record_Private * GetPrivate();

    bool IsTdrDataProtocolType() const;
    bool IsPlainDataProtocolType() const;
    bool IsPBDataProtocolType() const;
    void SetDataProtocolType(int type) {m_data_protocol_type = type;}

    //inner类型不校验DataProtocolType
    int InnerSetKey(IN const char* field_name, IN const void * field_value, IN const size_t value_size);
    
    int InnerSetKeyInt8(IN const char* field_name, IN const int8_t field_value);
    
    int InnerSetKeyInt16(IN const char* field_name, IN const int16_t field_value);
    
    int InnerSetKeyInt32(IN const char* field_name, IN const int32_t field_value);
    
    int InnerSetKeyInt64(IN const char* field_name, IN const int64_t field_value);
    
    int InnerSetKeyFloat(IN const char* field_name, IN const float field_value);
    
    int InnerSetKeyDouble(IN const char* field_name, IN const double field_value);
    
    int InnerSetKeyStr(IN const char* field_name, IN const char* field_value);
    
    int InnerSetKeyBlob(IN const char* field_name, IN const char* field_value, IN const size_t value_size);
    
    int InnerSetValue(IN const char* field_name, IN const void * field_value, IN const size_t value_size);
    
    int InnerSetValueInt8(IN const char* field_name, IN const int8_t field_value);
    
    int InnerSetValueInt16(IN const char* field_name, IN const int16_t field_value);
    
    int InnerSetValueInt32(IN const char* field_name, IN const int32_t field_value);
    
    int InnerSetValueInt64(IN const char* field_name, IN const int64_t field_value);
    
    int InnerSetValueFloat(IN const char* field_name, IN const float field_value);
    
    int InnerSetValueDouble(IN const char* field_name, IN const double field_value);
    
    int InnerSetValueStr(IN const char* field_name, IN const char* field_value);
    
    int InnerSetValueBlob(IN const char* field_name, IN const char* field_value, IN const size_t value_size);
    
    int32_t InnerGetKey(IN const char* field_name, OUT void *& field_value, OUT size_t & value_size) const;
    
    int32_t InnerGetKeyInt8(IN const char* field_name, OUT int8_t & field_value) const;
    
    int32_t InnerGetKeyInt16(IN const char* field_name, OUT int16_t & field_value) const;
    
    int32_t InnerGetKeyInt32(IN const char* field_name, OUT int32_t & field_value) const;
    
    int32_t InnerGetKeyInt64(IN const char* field_name, OUT int64_t & field_value) const;
    
    int32_t InnerGetKeyFloat(IN const char* field_name, OUT float & field_value) const;
    
    int32_t InnerGetKeyDouble(IN const char* field_name, OUT double & field_value) const;
    
    int32_t InnerGetKeyStr(IN const char* field_name, OUT const char*& field_value) const;
    
    int32_t InnerGetKeyBlob(IN const char* field_name, OUT const char*& field_value, OUT size_t & value_size) const;
    
    int32_t InnerGetKey(IN uint32_t fieldIndex, OUT const char*& field_name, OUT const void *& field_value, OUT size_t& fieldValueLen) const;
    
    int32_t InnerGetValue(IN const char* field_name, OUT void *& field_value, OUT size_t & value_size) const;
    
    int32_t InnerGetValueInt8(IN const char* field_name, OUT int8_t & field_value) const;
    
    int32_t InnerGetValueInt16(IN const char* field_name, OUT int16_t & field_value) const;
    
    int32_t InnerGetValueInt32(IN const char* field_name, OUT int32_t & field_value) const;
    
    int32_t InnerGetValueInt64(IN const char* field_name, OUT int64_t & field_value) const;
    
    int32_t InnerGetValueFloat(IN const char* field_name, OUT float & field_value) const;
    
    int32_t InnerGetValueDouble(IN const char* field_name, OUT double & field_value) const;
    
    int32_t InnerGetValueStr(IN const char* field_name, OUT const char*& field_value) const;
    
    int32_t InnerGetValueBlob(IN const char* field_name, OUT const char*& field_value, OUT size_t & value_size) const;
    
    int32_t InnerGetValue(IN int fieldIndex, OUT const char*& field_name, OUT const void *& field_value, OUT size_t& fieldValueLen) const;
private:
    bool m_update_flag;			//TCAPLUS_API_INCREASE_REQ使用的标志位，lubelleli
    uint32_t m_index;
    TCaplusKeySet* m_key;
    TCaplusValueSet_* m_value;
    ProtobufValueSet_* m_pbvalue;
    tcaplus_protocol_cs::TCaplusUpdFieldSet* m_update;
    int m_cmd;

    int m_nIdlType;
    char *m_pszSplitTableKeyBuff;
    uint32_t *m_pdwSplitTableKeyBuffLen;
    const char* m_table_name;
    LPTDRMETA m_table_meta;
    LPTDRMETA m_table_meta_v2; //王者使用，GetDataV2做高低版本记录兼容
    int m_table_meta_version;

    enum { DEFAULT_PACK_BUFFER_SIZE = 1048576 };//1MB
    char* m_pack_buffer;
    size_t m_pack_buffer_size;

    Logger* m_logger;
    int m_module_id;

    TcaplusServiceFieldPacker* m_packer;
    TcaplusServiceFieldUnpacker* m_unpacker;
    TcaplusRecordBroker* m_broker;
    TcaplusServiceRequest* m_request;
    TcaplusServiceResponse* m_response;

	const char* m_index_name;

    int m_field_pair[TCAPLUS_MAX_KEY_FIELD_NUM+TCAPLUS_MAX_VALUE_FIELD_NUM][2];//表结构中的关联字段组：array:refer; union:select
    int m_field_pair_count;                                   //表结构中的关联字段组对数

    uint64_t m_last_access_time;
	struct tagSHtable* m_value_field_name_hash_table;  //初始化在request对象里做
	size_t m_hash_table_size;

	mutable bool m_call_getdata_flag;  //是否调用GetData()函数，在GetData函数开始的地方置为true，在结尾的地方置为false

    //表的数据类型， -1 invalid(不用校验)， 0 tdr, 1 PLAIN 2 PB
    //TDR/PB模式的表，业务只能调用SetData，GetData访问，调用SetKey, SetValue GetKey GetValue接口会报错
    //PLAIN(即KV模式)模式的表，业务只能调用SetKey SetValue GetKey GetValue访问; 调用SetData， GetData接口会失败
    int m_data_protocol_type;
};

}

#endif  // __TCAPLUS_SERVICE_TCAPLUS_SERVICE_RECORD_H__

