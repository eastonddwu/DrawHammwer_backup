/**********************************************************************
 * Copyright (c)             : 2011 - 2016 Tencent. All Rights Reserved.
 * File                      : tcaplus_define.h
 * TcaplusServiceApi Version : 3.18.0.
 * Description               : TCaplus Service API for define const object
 * modification history
 * ---------------------------------
 * Author                    : tcaplus
 * Date                      : 2016/11/25
 * ---------------------------------
 *
 **********************************************************************/
#ifndef _TCAPLUS_DEFINE_H
#define _TCAPLUS_DEFINE_H

#include <stddef.h>
#include <time.h>
#include <sys/types.h>
#include "pal/pal.h"
#include "tlog/tlog.h"
#include "tcaplus_error_code.h"

#ifdef __cplusplus
namespace TcaplusService
{
#endif

#if !defined(_WIN32) && !defined(_WIN64)
    #include <stdint.h>
    #include <inttypes.h>
#else
/*    typedef  signed char  int8_t;
    typedef  short int16_t;
    typedef  int   int32_t;
    typedef unsigned char  uint8_t;
    typedef unsigned short uint16_t;
    typedef unsigned int   uint32_t;

    #if _MSC_VER >= 1300
        typedef unsigned long long 	uint64_t;
        typedef long long 	int64_t;
    #else
        typedef unsigned __int64	uint64_t;
        typedef __int64	int64_t;
    #endif*/
#endif

/** \brief 最大记录条数 */
#define MAX_RECORD_NUM                  1024

/** \brief 一个批量查询里最大的Partkey数目 */
#define MAX_PARTKEY_GET_NUM_IN_ONE_BATCH                  10

/** \brief 输入参数类型 */
#ifndef IN
#define IN
#endif


/** \brief 输出参数类型 */
#ifndef OUT
#define OUT
#endif

/** \brief 输入输出参数类型 */
#ifndef INOUT
#define INOUT
#endif

/** \brief 操作类型定义 */
enum TCaplusApiCmds
{
    /** \brief 无效的请求 */
    TCAPLUS_API_INVALID_REQ             = 0x0000,

    /** \brief 无效的应答 */
	TCAPLUS_API_INVALID_RES             = -0x0001,

	/** \brief 插入请求 */
    TCAPLUS_API_INSERT_REQ              = 0x0001,

    /** \brief 插入应答 */
    TCAPLUS_API_INSERT_RES              = 0x0002,

    /** \brief 替换/插入请求 */
    TCAPLUS_API_REPLACE_REQ             = 0x0003,

    /** \brief 替换/插入应答 */
    TCAPLUS_API_REPLACE_RES             = 0x0004,

    /** \brief 增量更新请求 */
    TCAPLUS_API_INCREASE_REQ            = 0x0005,

    /** \brief 增量更新应答 */
    TCAPLUS_API_INCREASE_RES            = 0x0006,

    /** \brief 单条查询请求 */
    TCAPLUS_API_GET_REQ                 = 0x0007,

    /** \brief 单条查询应答 */
    TCAPLUS_API_GET_RES                 = 0x0008,

    /** \brief 删除请求 */
    TCAPLUS_API_DELETE_REQ              = 0x0009,

    /** \brief 删除应答 */
    TCAPLUS_API_DELETE_RES              = 0x000a,

    /** \brief 查询List所有元素请求 */
	TCAPLUS_API_LIST_GETALL_REQ         = 0x000b,

	/** \brief 查询List所有元素应答 */
	TCAPLUS_API_LIST_GETALL_RES         = 0x000c,

	/** \brief 删除List所有元素请求 */
	TCAPLUS_API_LIST_DELETEALL_REQ      = 0x000d,

	/** \brief 删除List所有元素应答 */
	TCAPLUS_API_LIST_DELETEALL_RES      = 0x000e,

	/** \brief 删除List多个元素请求 */
	TCAPLUS_API_LIST_DELETE_BATCH_REQ   = 0x0041,

	/** \brief 删除List多个元素应答 */
	TCAPLUS_API_LIST_DELETE_BATCH_RES   = 0x0042,

	/** \brief 查询List单个元素请求 */
	TCAPLUS_API_LIST_GET_REQ            = 0x000f,

	/** \brief 查询List单个元素应答 */
	TCAPLUS_API_LIST_GET_RES            = 0x0010,

	/** \brief 插入List元素请求 */
	TCAPLUS_API_LIST_ADDAFTER_REQ       = 0x0011,

	/** \brief 插入List元素应答 */
	TCAPLUS_API_LIST_ADDAFTER_RES       = 0x0012,

	/** \brief 删除List单个元素请求 */
	TCAPLUS_API_LIST_DELETE_REQ         = 0x0013,

	/** \brief 删除List单个元素应答 */
	TCAPLUS_API_LIST_DELETE_RES         = 0x0014,

	/** \brief 替换List单个元素请求 */
	TCAPLUS_API_LIST_REPLACE_REQ        = 0x0015,

	/** \brief 替换List单个元素应答 */
	TCAPLUS_API_LIST_REPLACE_RES        = 0x0016,

	/** \brief 批量查询请求 */
    TCAPLUS_API_BATCH_GET_REQ           = 0x0017,

    /** \brief 批量查询应答 */
    TCAPLUS_API_BATCH_GET_RES           = 0x0018,

    /** \brief 部分Key查询请求 */
	TCAPLUS_API_GET_BY_PARTKEY_REQ      = 0x0019,

	/** \brief 部分Key查询应答 */
	TCAPLUS_API_GET_BY_PARTKEY_RES      = 0x001a,

    /** \brief 更新请求 */
    TCAPLUS_API_UPDATE_REQ             = 0x001d,

    /** \brief 更新应答 */
    TCAPLUS_API_UPDATE_RES             = 0x001e,

	TCAPLUS_API_METADATA_GET_REQ           = 0x001b,

	TCAPLUS_API_METADATA_GET_RES           = 0x001c,

    TCAPLUS_API_APP_SIGNUP_REQ = 51, // 服务化应用身份认证请求
    TCAPLUS_API_APP_SIGNUP_RES = 52, // 服务化应用身份认证响应
    TCAPLUS_API_HEART_BEAT_REQ = 53, // 心跳检查请求
    TCAPLUS_API_HEART_BEAT_RES = 54, // 心跳检查响应
    TCAPLUS_API_NOTIFY_STOP_REQ = 67, // tcaproxy通知客户端即将停止运行
    TCAPLUS_API_NOTIFY_STOP_RES = 68, // 客户端响应tcaproxy，表示暂时不再发送请求

    /** \brief 表遍历请求 */
    TCAPLUS_API_TABLE_TRAVERSE_REQ = 0x0045,

    /** \brief 表遍历响应 */
    TCAPLUS_API_TABLE_TRAVERSE_RES = 0x0046,

    /** \brief 表遍历前获取shard list请求 */
    TCAPLUS_API_GET_SHARD_LIST_REQ = 0x0047,

    /** \brief 表遍历前获取shard list响应 */
    TCAPLUS_API_GET_SHARD_LIST_RES = 0x0048,

    /** \brief 批量Partkey查询请求,已废弃!!!!!!!!!! */
    TCAPLUS_API_BATCH_GET_BY_PARTKEY_REQ = 0x0049,

    /** \brief 批量Partkey查询响应,已废弃!!!!!!!!!! */
    TCAPLUS_API_BATCH_GET_BY_PARTKEY_RES = 0x004a,

        /** \brief Document 操作请求 */
    TCAPLUS_API_DOCUMENT_OPERATION_REQ            = 0x004b,

        /** \brief Document 操作响应 */
    TCAPLUS_API_DOCUMENT_OPERATION_RES            = 0x004c,

       /** \brief Partkey update请求 */
    TCAPLUS_API_UPDATE_BY_PARTKEY_REQ           = 0x004d,

       /** \brief Partkey update响应 */
    TCAPLUS_API_UPDATE_BY_PARTKEY_RES          = 0x004e,

	   /** \brief Partkey delete请求 */
    TCAPLUS_API_DELETE_BY_PARTKEY_REQ           = 0x004f,

       /** \brief Partkey delete响应 */
    TCAPLUS_API_DELETE_BY_PARTKEY_RES          = 0x0050,

   /** \brief 带有相同Partkey的批量insert请求*/
    TCAPLUS_API_INSERT_BY_PARTKEY_REQ          = 0x0051,

   /** \brief 带有相同Partkey的批量insert响应 */
    TCAPLUS_API_INSERT_BY_PARTKEY_RES          = 0x0052,

    /** \brief table的记录总数请求 */
    TCAPLUS_API_GET_TABLE_RECORD_COUNT_REQ     = 0x0053,

    /** \brief table的记录总数响应 */
    TCAPLUS_API_GET_TABLE_RECORD_COUNT_RES     = 0x0054,

	/**\brief List table的遍历请求*/
	TCAPLUS_API_LIST_TABLE_TRAVERSE_REQ = 0x0057,

	/**\brief List table的遍历响应*/
	TCAPLUS_API_LIST_TABLE_TRAVERSE_RES = 0x0058,

    /** \brief protobuf部分字段获取请求 */
    TCAPLUS_API_PB_FIELD_GET_REQ           = 0x0067,

    /** \brief protobuf部分字段获取响应 */
    TCAPLUS_API_PB_FIELD_GET_RES           = 0x0068,

    /** \brief protobuf部分字段更新请求 */
    TCAPLUS_API_PB_FIELD_SET_REQ           = 0x0069,

    /** \brief protobuf部分字段更新响应 */
    TCAPLUS_API_PB_FIELD_SET_RES           = 0x006a,

    /** \brief protobuf部分字段自增请求 */
    TCAPLUS_API_PB_FIELD_INC_REQ           = 0x006b,

    /** \brief protobuf部分字段自增响应 */
    TCAPLUS_API_PB_FIELD_INC_RES           = 0x006c,

    /** \brief protobuf部分字段自增请求 */
    TCAPLUS_API_PB_BATCH_FIELD_GET_REQ           = 0x0075,

    /** \brief protobuf部分字段自增响应 */
    TCAPLUS_API_PB_BATCH_FIELD_GET_RES           = 0x0076,
    
	/** \brief 索引查询请求 */
    TCAPLUS_API_SQL_REQ           = 0x0081,

    /** \brief 索引查询响应 */
    TCAPLUS_API_SQL_RES           = 0x0082,

    /** \brief List table的index replace 请求, 仅供内部使用 */
    TCAPLUS_API_LIST_INDEX_MOVEREPLACE_REQ           = 0x1077,

    /** \brief List table的index replace 响应, 仅供内部使用 */
    TCAPLUS_API_LIST_INDEX_MOVEREPLACE_RES           = 0x1078,

    /** \brief List table的element replace 请求, 仅供内部使用 */
    TCAPLUS_API_LIST_ELEMENT_MOVEREPLACE_REQ           = 0x1079,

    /** \brief List table的element replace 响应, 仅供内部使用 */
    TCAPLUS_API_LIST_ELEMENT_MOVEREPLACE_RES           = 0x107a,

		/**\brief API的最大值，为了能够匹配系统内部请求*/
    TCAPLUS_API_MAX_NUM = 0xffff,

    /**还有部分内部用参数在internal里面，不放在这里*/
};

/** \brief 错误码定义 */
#ifdef __cplusplus
enum ErrorCode
{

    ERR_SUCCESS                         = TcapErrCode::GEN_ERR_SUC,

    // Srv层面错误码
    ERR_FAIL                            = TcapErrCode::GEN_ERR_ERR, // 操作失败
    ERR_ROUTE_MSG                       = TcapErrCode::SVR_ERR_FAIL_ROUTE,  // 消息路由失败
    ERR_TIME_OUT                        = TcapErrCode::SVR_ERR_FAIL_TIMEOUT,  // 记录操作超时
    ERR_SHORT_BUFF                      = TcapErrCode::SVR_ERR_FAIL_SHORT_BUFF,  // 消息缓冲区太小
    ERR_SYSTEM_BUSY                     = TcapErrCode::SVR_ERR_FAIL_SYSTEM_BUSY,  // 系统忙
    ERR_RECORD_EXIST                    = TcapErrCode::SVR_ERR_FAIL_RECORD_EXIST,  // 记录已经存在
    ERR_RECORD_NOT_EXIST                = TcapErrCode::TXHDB_ERR_RECORD_NOT_EXIST,  // 记录不存在
    ERR_INVALID_FIELD_NAME              = TcapErrCode::SVR_ERR_FAIL_INVALID_FIELD_NAME,  // 字段名称错误
    ERR_VALUE_OVER_MAX_LEN              = TcapErrCode::SVR_ERR_FAIL_VALUE_OVER_MAX_LEN,  // 超过字段最大长度
    ERR_INVALID_FIELD_TYPE              = TcapErrCode::SVR_ERR_FAIL_INVALID_FIELD_TYPE,  // 无效的字段类型
    ERR_SYNC_WRITE                      = TcapErrCode::SVR_ERR_FAIL_SYNC_WRITE,  // 记录缓写数据库错误
    ERR_WRITE_RECORD                    = TcapErrCode::SVR_ERR_FAIL_WRITE_RECORD,  // 写记录到数据引擎失败
    ERR_DELETE_RECORD                   = TcapErrCode::SVR_ERR_FAIL_DELETE_RECORD,  // 删除记录到数据引擎失败
    ERR_DATA_ENGINE                     = TcapErrCode::SVR_ERR_FAIL_DATA_ENGINE,  // 数据引擎错误
    ERR_INVALID_VERSION                 = TcapErrCode::SVR_ERR_FAIL_INVALID_VERSION,  // 版本信息不匹配
    ERR_RESULT_OVERFLOW                 = TcapErrCode::SVR_ERR_FAIL_RESULT_OVERFLOW,  // 结果溢出
    ERR_INVALID_OPERATION               = TcapErrCode::SVR_ERR_FAIL_INVALID_OPERATION,  // 更新的操作类型不正确
    ERR_SYSTEM_ERROR                    = TcapErrCode::SVR_ERR_FAIL_SYSTEM_ERROR,  // 系统错误
    ERR_INVALID_SUBSCRIPT               = TcapErrCode::SVR_ERR_FAIL_INVALID_SUBSCRIPT,  // 无效的记录编号
    ERR_INVALID_INDEX                   = TcapErrCode::SVR_ERR_FAIL_INVALID_INDEX,  // 无效的记录索引
    ERR_OVER_MAXE_FIELD_NUM             = TcapErrCode::SVR_ERR_FAIL_OVER_MAXE_FIELD_NUM,  // 超过允许的最大字段个数
    ERR_MISS_KEY_FIELD                  = TcapErrCode::SVR_ERR_FAIL_MISS_KEY_FIELD,  // 缺少key字段

    // API层面错误码
    ERR_OVER_MAX_KEY_FIELD_NUM          = TcapErrCode::API_ERR_OVER_MAX_KEY_FIELD_NUM,  // 超过允许的key字段个数
    ERR_OVER_MAX_VALUE_FIELD_NUM        = TcapErrCode::API_ERR_OVER_MAX_VALUE_FIELD_NUM,  // 超过允许的value字段个数
    ERR_OVER_MAX_FIELD_NAME_LEN         = TcapErrCode::API_ERR_OVER_MAX_FIELD_NAME_LEN,  // 超过允许的字段名称长度
    ERR_OVER_MAX_FIELD_VALUE_LEN        = TcapErrCode::API_ERR_OVER_MAX_FIELD_VALUE_LEN,  // 超过允许的字段内容长度
    ERR_FIELD_NOT_EXSIST                = TcapErrCode::API_ERR_FIELD_NOT_EXSIST,  // 字段不存在
    ERR_FIELD_TYPE_NOT_MATCH            = TcapErrCode::API_ERR_FIELD_TYPE_NOT_MATCH,  // 字段类型不匹配其对应的长度
    ERR_PARAMETER_INVALID               = TcapErrCode::API_ERR_PARAMETER_INVALID,  // 参数无效
    ERR_OPERATION_TYPE_NOT_MATCH        = TcapErrCode::API_ERR_OPERATION_TYPE_NOT_MATCH,  // 操作与请求类型不匹配
    ERR_PACK_MESSAGE                    = TcapErrCode::API_ERR_PACK_MESSAGE,  // 打包请求消息错误
    ERR_UNPACK_MESSAGE                  = TcapErrCode::API_ERR_UNPACK_MESSAGE,  // 解包应答消息错误
    ERR_PACKAGE_NOT_UNPACKED            = TcapErrCode::API_ERR_PACKAGE_NOT_UNPACKED,  // 消息未解包
    ERR_OVER_MAX_RECORD_NUM             = TcapErrCode::API_ERR_OVER_MAX_RECORD_NUM,  // 超过允许的最大记录条数
    ERR_INVALID_COMMAND                 = TcapErrCode::API_ERR_INVALID_COMMAND,  // 无效的请求类型
    ERR_NO_MORE_RECORD                  = TcapErrCode::API_ERR_NO_MORE_RECORD,  // 没有剩余记录
    ERR_OVER_KEY_FIELD_NUM              = TcapErrCode::API_ERR_OVER_KEY_FIELD_NUM,  // 超过当前实际存在的key字段个数
    ERR_OVER_VALUE_FIELD_NUM            = TcapErrCode::API_ERR_OVER_VALUE_FIELD_NUM,  // 超过当前实际存在的value字段个数
    ERR_OVER_MAX_PKG_SIZE               = TcapErrCode::API_ERR_OVER_MAX_PKG_SIZE,     // 超过最大包大小限制(用户数据不能超过256k)

    ERR_OBJ_NEED_INIT                   = TcapErrCode::API_ERR_OBJ_NEED_INIT,  // 对象使用前应该初始化
    ERR_INVALID_DATA_SIZE               = TcapErrCode::API_ERR_INVALID_DATA_SIZE,  // 数据大小不正确或不一致
    ERR_INVALID_ARRAY_COUNT             = TcapErrCode::API_ERR_INVALID_ARRAY_COUNT,  // 数组长度字段meta信息无效
    ERR_INVALID_UNION_SELECT            = TcapErrCode::API_ERR_INVALID_UNION_SELECT,  // 联合体的选择字段无效
    ERR_MISS_PRIMARY_KEY                = TcapErrCode::API_ERR_MISS_PRIMARY_KEY,  // 缺少主键字段
    ERR_UNSUPPORT_FIELD_TYPE            = TcapErrCode::API_ERR_UNSUPPORT_FIELD_TYPE,  // 不支持此数据类型的字段
    ERR_ARRAY_BUFFER_IS_SMALL           = TcapErrCode::API_ERR_ARRAY_BUFFER_IS_SMALL,  // 打包/解包数组时提供的缓冲区太小
    ERR_IS_NOT_WHOLE_PACKAGE            = TcapErrCode::API_ERR_IS_NOT_WHOLE_PACKAGE,  // 提供的网络包不是一个完整包
    ERR_MISS_PAIR_FIELD                 = TcapErrCode::API_ERR_MISS_PAIR_FIELD,  // 缺少关联字段(变长数组及其长度字段，union及其选择字段)
    ERR_GET_META_ENTRY                  = TcapErrCode::API_ERR_GET_META_ENTRY,  // 获取meta中的字段信息失败
    ERR_GET_ARRAY_META                  = TcapErrCode::API_ERR_GET_ARRAY_META,  // 获取数组元素的meta信息失败
    ERR_GET_ENTRY_META                  = TcapErrCode::API_ERR_GET_ENTRY_META,  // 获取字段的meta信息失败
    ERR_INCOMPATIBLE_META               = TcapErrCode::API_ERR_INCOMPATIBLE_META,  // 指定的meta与表结构的meta不兼容
    ERR_PACK_ARRAY_DATA                 = TcapErrCode::API_ERR_PACK_ARRAY_DATA,  // 打包数组元素失败
    ERR_PACK_UNION_DATA                 = TcapErrCode::API_ERR_PACK_UNION_DATA,  // 打包联合元素失败
    ERR_PACK_STRUCT_DATA                = TcapErrCode::API_ERR_PACK_STRUCT_DATA,  // 打包结构元素失败
    ERR_UNPACK_ARRAY_DATA               = TcapErrCode::API_ERR_UNPACK_ARRAY_DATA,  // 解包数组元素失败
    ERR_UNPACK_UNION_DATA               = TcapErrCode::API_ERR_UNPACK_UNION_DATA,  // 打包联合元素失败
    ERR_UNPACK_STRUCT_DATA              = TcapErrCode::API_ERR_UNPACK_STRUCT_DATA,  // 解包结构元素失败
    ERR_INVALID_INDEX_NAME              = TcapErrCode::API_ERR_INVALID_INDEX_NAME,  // 表结构的index名称无效
    ERR_MISS_PARTKEY_FIELD              = TcapErrCode::API_ERR_MISS_PARTKEY_FIELD,  // 缺少部分key字段
    ERR_ALLOCATE_MEMORY                 = TcapErrCode::API_ERR_ALLOCATE_MEMORY,  // 分配内存失败
    ERR_GET_META_SIZE                   = TcapErrCode::API_ERR_GET_META_SIZE,  // 获取meta大小失败
    ERR_MISS_BINARY_VERSION             = TcapErrCode::API_ERR_MISS_BINARY_VERSION,  // binary字段缺少版本信息
    ERR_INVALID_INCREASE_FIELD          = TcapErrCode::API_ERR_INVALID_INCREASE_FIELD,  // 请求做自增/自减运算的字段无效
    ERR_INVALID_RESULT_FLAG             = TcapErrCode::API_ERR_INVALID_RESULT_FLAG,  // ResultFlag无效, ResultFlag是用于标示应答结果中是否需要携带Value字段值的
    ERR_OVER_MAX_LIST_INDEX_NUM         = TcapErrCode::API_ERR_OVER_MAX_LIST_INDEX_NUM,  // 超过允许的LIST元素个数
    ERR_INVALID_OBJ_STATE               = TcapErrCode::API_ERR_INVALID_OBJ_STATUE,  // 对象处于不正确的状态
    ERR_INVALID_REQUEST                 = TcapErrCode::API_ERR_INVALID_REQUEST,  // 内部无法获取请求对象
    ERR_INVALID_SHARD_LIST              = TcapErrCode::API_ERR_INVALID_SHARD_LIST,  // 不正确的shard列表
    ERR_TABLE_NAME_MISSING              = TcapErrCode::API_ERR_TABLE_NAME_MISSING,  // 找不到表名
	ERR_SOCKET_SEND_BUFF_IS_FULL        = TcapErrCode::API_ERR_SOCKET_SEND_BUFF_IS_FULL,  // 发送缓冲区满

    // CENTER层面错误码
    ERR_TABLE_ALREADY_EXIST             = TcapErrCode::CENTER_ERR_TABLE_ALREADY_EXIST,  // 表已经存在
    ERR_TABLE_NOT_EXIST                 = TcapErrCode::CENTER_ERR_TABLE_NOT_EXIST,  // 表不存在

	ERR_INVALID_MAGIC                   = TcapErrCode::API_ERR_INVALID_MAGIC,  //magic错误

};
#endif

/** \brief 更新操作类型定义 */
enum TCaplusApiOperation
{
    /** \brief 增量加操作 */
    TCAPLUS_API_OP_PLUS     = 1,

    /** \brief 增量减操作 */
    TCAPLUS_API_OP_MINUS    = 2,
};

/** \brief LIST元素index特殊位置 */
enum TCaplusListIndexExtra
{
    TCAPLUS_API_LIST_PRE_FIRST_INDEX = -2, // 插入元素位置在最前面
    TCAPLUS_API_LIST_LAST_INDEX = -1, // 插入元素位置在最后面
};

/** \brief LIST满时元素移除方式 */
enum TCaplusListShiftFlag
{
    TCAPLUS_API_LIST_SHIFT_NONE = 0, // 不移除元素
    TCAPLUS_API_LIST_SHIFT_HEAD = 1, // 移除最前面的元素
    TCAPLUS_API_LIST_SHIFT_TAIL = 2, // 移除最后面的元素
};

/** \brief 索引查询类型 */
enum SqlTypeEnum
{
    INVALID_SQL_TYPE = 0, //非法查询
    RECORD_SQL_QUERY_TYPE = 1, //记录查询, select * from test where XXX
    AGGREGATIONS_SQL_QUERY_TYPE = 2, //聚合查询, select sum(level) from test where XXX
};

/** \brief 字段类型，目前主要用于索引查询 */
enum FieldTypeEnum
{
    TYPE_INVALID = 0,
    TYPE_BOOL = 1,
    TYPE_INT8 = 2,
    TYPE_UINT8 = 3,
    TYPE_INT16 = 4,
    TYPE_UINT16 = 5,
    TYPE_INT32 = 6,
    TYPE_UINT32 = 7,
    TYPE_INT64 = 8,
    TYPE_UINT64 = 9,
    TYPE_FLOAT = 10,
    TYPE_DOUBLE = 11,
    TYPE_STRING = 12,
    TYPE_END = 13,
};

static const size_t USE_MAX_URL_SIZE = 1024;//关联tcaplus_service_constant.h中的MAX_URL_SIZE
/** \brief ServiceApi与Proxy之间的Buffer使用情况统计*/
struct BuffStaticBetweenApiAndProxy
{
    unsigned int send_data_size;//单个连接发送Buffer中的数据大小
	unsigned int recv_data_size;//单个连接接收Buffer中的数据大小
	int          send_buff_left_rate;//单个连接发送Buffer可用率
	int          recv_buff_left_rate;//单个连接接收Buffer可用率
#if defined(_WIN32) || defined(_WIN64)
	char         server_url[1024];//proxy的url地址  这里只为了用户能更直观的告警，大家讨论下这个要不要放出去 TODO By kenny
#else
	char         server_url[USE_MAX_URL_SIZE];//proxy的url地址  这里只为了用户能更直观的告警，大家讨论下这个要不要放出去 TODO By kenny
#endif
};

#ifdef __cplusplus
}
#endif

#endif  /* _TCAPLUS_DEFINE_H */


