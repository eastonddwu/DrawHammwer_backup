/**********************************************************************
 * Copyright (c)             : 2011 - 2016 Tencent. All Rights Reserved.
 * File                      : tcaplus_service_response.h
 * TcaplusServiceApi Version : 3.18.0.
 * Description               : TCaplus Service API for response
 * modification history
 * ---------------------------------
 * Author                    : tcaplus
 * Date                      : 2016/11/25
 * ---------------------------------
 *
 **********************************************************************/
#ifndef __TCAPLUS_SERVICE_TCAPLUS_SERVICE_RESPONSE_H__
#define __TCAPLUS_SERVICE_TCAPLUS_SERVICE_RESPONSE_H__

#include "tcaplus_service_base.h"

//#include "tcaplus_record.h"
//#include "tcaplus_define.h"
#include "tcaplus_service_nonecopyable.h"
#include "tcaplus_sql_result.h"

namespace tcaplus {
    namespace doc {
        class BSONObj;
    }
}

class TCaplusValueSet;
class TCaplusKeySet;

namespace tcaplus_protocol_cs
{
class TCaplusPkg;
class PerfTest;
};  //lint !e19

namespace TCAPLUS_COROUTINE_PB
{
class TcaplusCoroutinePbApi;
};

namespace TCAPLUS_ASYNC_PB
{
class TcaplusAsyncPbApi;
};

namespace TcaplusService
{
class ClientTraversal;
class ClientDescGet;
class ClientRecordGet;
class ClientSysGet;

class TcaplusServiceRecord;
class TcaplusServiceTraverser;
struct RecordProperties;

//用于保存key字段名和类型
struct KeyInfo
{
	enum {TCAPLUS_MAX_FIELD_NAME = 32};  //该值与tcaplus_protocol_comm.h中的值相同
	char key_field_name[TCAPLUS_MAX_FIELD_NAME];  //key 字段名
	uint8_t key_field_type;  //类型
	uint8_t generate_by_key_meta;  //为1 表示该字段是系统自动产生的，为0 表示该字段不是系统自动产生的.
    uint32_t max_length;
};

//用于保存value 字段名和类型
struct ValueInfo
{
	enum {TCAPLUS_MAX_FIELD_NAME = 32};   //该值与tcaplus_protocol_comm.h中的值相同
	char value_field_name[TCAPLUS_MAX_FIELD_NAME];  //value 字段名
	uint8_t value_field_type;  // 类型
	uint8_t generate_by_value_mate; //为1 表示该字段是系统自动产生的，为0 表示该字段不是系统自动产生的.
    uint32_t max_length;
};

struct KeyValueInfo
{
	char table_name[TCAPLUS_MAX_TABLE_NAME_LEN];
	uint32_t table_type;   //表的类型，目前只支持Generic 和List两种类型的表
    int32_t svr_tdr_meta_version; // svr端所使用的表定义 tdr meta 当前版本号
	struct KeyInfo key_info[TCAPLUS_MAX_KEY_FIELD_NUM];
	uint32_t key_num;  //key 的实际个数
	struct ValueInfo value_info[TCAPLUS_MAX_VALUE_FIELD_NUM];
	uint32_t value_num;  //value字段的实际个数

    int32_t idl_type;
};



/**
@brief 响应对象类
@note 该类由于需要缓存请求消息，因此消息对象比较大，目前单个对象占用的大小约为4M，请不要在栈上定义对应的局部对象，防止栈溢出。
*/
class TcaplusServiceResponse : public TcaplusServiceBase,public NoneCopyable
{
public:
    /// 构造函数
    TcaplusServiceResponse();

    /// 析构函数
    ~TcaplusServiceResponse();

    /**
    @brief 构造函数，初始化内存资源。
    @param [IN] logger     日志对象指针，用于记录debug和error日志。若传NULL则可通过GetLastError获取最后一次发生的错误信息。
    @param [IN] module_id  业务所属的区服ID
    @retval 0  成功
    @retval <0 失败，具体错误参见 \link ErrorCode \endlink
    */
    int Construct(IN Logger* logger = NULL, IN int module_id = 0);

    /**
    @brief 资源释放
    */
    void Destruct();

    /**
    @brief  获取请求操作的表名
    @param [OUT] table_name_size      若table_name_size不为NULL，则向其所指位置写入表名长度，
    								  该长度包括结尾的'\0'。
    @retval NULL  TcaplusServiceResponse对象未初始化时返回NULL。
    @retval !NULL 指向操作的表名的指针。
    */
    const char* GetTableName(OUT size_t* table_name_size = NULL) const;

   	/** \brief  获取操作结果
   	*  \retval 0 			操作结果成功
   	*  \retval >0			操作结果警告，例如Get或删除一个不存在的数据记录则retval为TcapErrCode::SVR_ERR_FAIL_RECORD_EXIST，具体错误参见 \link ErrorCode \endlink
   	*  \retval <0			操作结果错误，例如写数据失败则retval为TcapErrCode::SVR_ERR_FAIL_WRITE_RECORD，具体错误参见 \link ErrorCode \endlink
   	*/
    int32_t GetResult() const;

   /**
    @brief  获取APPID
    @retval 非0            返回响应包中的app_id
    @retval 0              当对象未初始化或尚未解析过响应包时，默认返回0
    */
    int64_t GetAppId() const;

   /**
    @brief  获取ZoneId
    @retval 非0            返回响应包中的zond_id
    @retval 0              当对象未初始化或尚未解析过响应包时，默认返回0
    */
    int GetZoneId() const;

    /**
    @brief  获取结果操作类型
    @retval \link TCaplusApiCmds \endlink
    */
    TCaplusApiCmds GetCmd() const;

    /**
    @brief  获取请求子操作类型，对document操作才有意义
    @retval \link TCaplusApiCmds \endlink
    */
    uint32_t GetSubCmd() const;


    /**
    @brief  获取结果异步事务ID
    @retval 异步事务ID
    */
    uint64_t GetAsynID() const;


    /**
    @brief  获取结果序列号
    @retval 响应序列号
    */
    int32_t GetSequence() const;

	/**
    @brief  获取结果标识位
    @retval 返回0则代表本次请求成功, 返回了正确的响应包
    		返回非0则代表本次请求不成功, 具体错误参见 \link ErrorCode \endlink
    */
    int GetResultFlag() const;

    /**
    @brief  获取空记录自增允许标志
    @return 返回空记录自增允许标志
    */
    char GetAddableIncreaseFlag() const;

    /**
    @brief   获取响应的通用标志位
    @return  返回响应的通用标志位
    @note   有效的标志位列表及详细解释请参考 TcaplusServiceRequest::SetFlags(int)
    */
    int32_t GetFlags() const;

    /**
    @brief  获取响应中的用户缓存信息
    @param  [out] buffer_data_size  用户缓存数据大小，若buffer_data_size不为NULL，则向其所指位置写入用户缓存数据的长度。
    @retval NULL  当TcaplusServiceResponse对象未初始化时返回NULL。
    @retval !NULL 用户缓存的首指针。
    */
    const char* GetUserBuff(OUT size_t* buffer_data_size = NULL) const;

    /**
    @brief  获取响应中的性能测试数据
    @param  [out] buffer_data_size  性能测试数据大小，若buffer_data_size不为NULL，则向其所指位置写入用户缓存数据的长度。
    @retval NULL  当TcaplusServiceResponse对象未初始化时返回NULL。
    @retval !NULL 性能测试数据的首指针。
    */
    const char* GetPerfTest(OUT size_t* buffer_data_size = NULL) const;

    /**
    @brief  获取本响应中结果记录条数
    @retval 本响应中结果记录条数
    */
    int32_t GetRecordCount() const;

    /**
    @brief  获取本响应返回的第一条记录在整个结果集中的位置下标，从0开始编号
    @retval 本响应返回的第一条记录在整个结果集中的位置下标，从0开始编号，
             当GetRecordMatchOffset()+GetRecordCount() = GetRecordMatchCount()  说明已经返回符合条件的所有记录
    */
    int32_t GetRecordMatchOffset() const;

	/**
    @brief  获取整个结果中的记录条数。既包括本响应返回的记录数，也包括本响应未返回的记录数。
    @retval 记录条数
    @note 该函数只能用于以下请求：
	(1）TCAPLUS_API_GET_BY_PARTKEY_REQ（部分key查询）；
	(2）TCAPLUS_API_LIST_GETALL_REQ（list表查询所有匹配的记录）；
	(3）TCAPLUS_API_BATCH_GET_REQ（批量查询）

    @note 该函数的作用是：
       (1）当使用了SetResultLimit()来限制返回的记录时，
               使用该函数可以获取所有匹配的记录的个数，
               包括本响应返回的记录数和本响应未返回的记录数，
               而GetRecordCount()函数只能获取本响应返回的记录的个数；
       (2）当所返回的记录很多时，需要分包的时候，
               使用该函数可以获取总共的记录数，
               即多个分包所有记录数的总和，
               而GetRecordCount()函数只能返回单个分包中的(本响应中的)记录数.
    */
    int32_t GetRecordMatchCount() const;

    /**
    @brief 获取表记录的属性
    @param [OUT] RecordProperties的引用
    @retval 0   获取记录属性成功
    @retval 非0 获取记录属性失败
    @note 只适用于Generic表的单记录操作：GET, INSERT, REPLACE, UPDATE, INCREASE, DELETE
    @note 典型场景：当用户不需要response里Record的字段内容，只需要Record的属性（如：version）时，
          可以调用request->SetResultFlag(TCaplusValueFlag_NOVALUE)设置回包时不携带各字段内容， 只返回记录的属性，
          接收到回包后， 调用response->GetRecordProperties(p)即可取得Record的属性。
          显然，当回包携带字段内容时，同样可以使用此接口。
    */
    int32_t GetRecordProperties(OUT RecordProperties& properties);

    /*
    @brief 该函数仅用于索引查询时获取查询类型，目前分为记录查询和聚合查询两种类型
    @retval 返回索引查询类型，如果不是索引查询响应，则返回INVALID_SQL_TYPE
    @note 如果索引查询类型为记录查询，则需要调用FetchRecord函数来获取记录
          如果索引查询类型为聚合查询，则需要调用FetchSqlResult函数来获取记录
    */
    SqlTypeEnum GetSqlType();

    /*
    @brief 该函数仅用于索引查询类型为聚合查询时获取聚合结果
    @retval 返回聚合查询结果，返回NULL表示获取失败
    @note 如果索引查询类型为记录查询，该函数将返回NULL
          如果索引查询类型为聚合查询，则根据SqlResult类提供的函数来获取查询结果
    */
    SqlResult* FetchSqlResult();

    /**
    @brief  从结果中获取一条记录
    @param [OUT] 指向返回记录的指针
    @retval 0   获取记录成功
    @retval 非0     获取记录失败
    @note 通过此函数获取到本次操作的TcaplusServiceRecord对象后，上次操作获取到的TcaplusServiceRecord对象会被覆盖.
    */
   int32_t FetchRecord(OUT const TcaplusServiceRecord*& record);

	/** \brief  从PartkeyUpdate/PartkeyDelete结果中获取失败的记录信息
	 *  \param [out] 指向返回记录的指针
	 *  \retval 0   获取记录成功
	 *  \retval 非0     获取记录失败
	 *  注意, 通过此函数获取到本次操作的Record后，上次操作获取到的Record会被覆盖.
	 */
	int32_t FetchErrorRecord(const TcaplusServiceRecord*& record);

    /**
    @brief  重置FetchRecord的对象，以便重新从第一个record开始获取。
    */
   void ResetFetch();

    /**
    @brief    获取受影响的Records的条数.
    @retval >= 0               受影响的Records的条数.
    @retval < 0                操作失败，具体错误参见 \link ErrorCode \endlink
    @note 在当前版本中该函数仅对List类型的DeleteAll操作有效.
    */
    int32_t GetAffectedRecordNum();

    /*
    @brief  获取记录下标或索引(index)
    @retval 对于Generic类型操作返回该记录在记录数组中的下标; 对于List类型操作返回该记录在List中的索引(index)。Index值为-1则表示无效的记录下标或索引.
    */
    //int32_t GetIndex();

    /**
    @brief  是否还有更多回包用于传递所请求的结果Records。
    @retval 0          没有更多回包了
    @retval 1          还有更多回包
    @retval < 0        操作失败，具体错误参见 \link ErrorCode \endlink
    @note  该函数只支持BatchGet,ListGetAll, getbypartkey, updatebypartkey, deletebypartkey 操作
    */
    int32_t HaveMoreResPkgs();

    /**
    @brief  可视化输出请求
    @param [INOUT] buffer           缓冲区指针
    @param [IN] buffer_size      缓冲区大小
    @retval 打包的缓冲区指针，内容以'\0'结尾
    */
    const char* Print(INOUT char* buffer, IN size_t buffer_size);

    /**
    @brief  key字段数目获取.
             当TcaplusServiceResponse::FetchRecord()失败时，该函数(TcaplusServiceResponse::GetKeyCount())在Unpack成功()或UnpackHead()成功的情况可以获取Key字段数目;
             当TcaplusServiceResponse::FetchRecord()成功时，该函数(TcaplusServiceResponse::GetKeyCount())和函数TcaplusServiceRecord::GetKeyCount()的功能相同;
    @retval                      Key字段(Key Field)数目
    */
    uint32_t GetKeyCount() const;

    /**
    @brief  key字段内容获取
             当TcaplusServiceResponse::FetchRecord()失败时，该函数(TcaplusServiceResponse::GetKey())在Unpack()成功或UnpackHead()成功的情况可以获取Keykey字段内容;
             当TcaplusServiceResponse::FetchRecord()成功时，该函数(TcaplusServiceResponse::GetKey())和函数TcaplusServiceRecord::GetKey()的功能相同;
    @param [IN] fieldIndex          字段下标，其有效值必须大于等于0并且小于GetKeyNum()
    @param  [OUT] field_name        字段名称，是字符串
    @param  [OUT] field_value        字段内容，最大长度1024字节
    @param  [OUT] value_size          字段内容长度
    @retval 0                       获取成功
    @retval 非0                     获取失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t GetKey(IN uint32_t fieldIndex,
                OUT const char*& field_name, OUT const void *& field_value, OUT size_t& fieldValueLen) const;

    /**
    @brief  获取消息包长度
    @param [IN] buffer_size            消息缓存
    @param [IN] buffer_data_size       消息缓存大小
    @retval >0                  消息包长度
    @retval =0                  无法获取消息包长度
    */
    static uint32_t GetPkgLen(IN const char* buffer, IN const size_t buffer_data_size);

    /**
    @brief  将消息缓存解包到具体的结果对象
    @param [IN] buffer         消息缓冲区
    @param [IN] buffSize       缓冲区大小
    @param  [OUT] ErrStr       解包错误描述信息
    @retval 0                  解包成功
    @retval 非0                解包失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t Unpack(IN const char* buffer, IN const size_t buffer_size);

	/**
    @brief  将消息缓存解包到具体的结果对象，该处仅解包头部信息
    @param [IN] buffer        消息缓冲区
    @param [IN] buffSize      缓冲区大小
    @param  [OUT] ErrStr       解包错误描述信息
    @retval 0                  解包成功
    @retval 非0                解包失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t Pack(IN char* buffer, INOUT size_t& buffer_size);

    /**
    @brief  将消息缓存解包到具体的结果对象，该处仅解包头部信息
    @param [IN] buffer        消息缓冲区
    @param [IN] buffSize      缓冲区大小
    @param  [OUT] ErrStr       解包错误描述信息
    @retval 0                  解包成功
    @retval 非0                解包失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t UnpackHead(IN const char* buffer, IN const size_t buffer_size);

    /**
    @brief  获取最后一次记录的错误信息。只有初始化logger参数为NULL时，request才会自动生成该缓冲区；否则会自动写日志，本函数返回NULL。
    @retval 最后错误信息缓冲区。
    */
    const char* GetLastError();

    /**
    @brief  设置响应包对应的默认meta，使用TDR方式对应的接口时必须先设置默认meta。
    @param [IN] table_name      表名，必须与unpack以后的表名一致。
    @param [IN] table_tdr_meta  表的meta描述，对应于数据库中存储的记录meta描述。
    @param [IN] table_meta_v2  王者使用，该表的高版本meta描述，用于record的GetDataV2接口。
    @note  如果是从TcaplusServer中RecvResponse得到的response对象，TcaplusServer已自动调用此方法，无需用户再显式调用。
    @retval 0    成功
    @retval <0   失败，返回对应的错误码。
    */
    int SetTableMeta(IN const char* table_name, IN LPTDRMETA table_meta, LPTDRMETA table_meta_v2=NULL);

	// 用于获取表的key 与value字段名和类型
	int GetTableKeyAndValueNameAndType(KeyValueInfo& key_value_info);

	// 获取表的记录总数，只适用于TCAPLUS_API_GET_TABLE_RECORD_COUNT_REQ请求获取返回结果
	int GetTableRecordCount(int64_t& table_record_count);


	/**
    @brief  获取TCaplusPkg指针。不清楚tcaplus协议的用户请不要使用该指针，否则会引起严重后果
    @retval TCaplusPkg指针。
    */
	int GetTcaplusPackagePtr(INOUT const tcaplus_protocol_cs::TCaplusPkg*& tcaplus_pkg);

	 /**
	 @brief  获取PartkeyUpdate和PartkeyDelete符合条件的记录
	 @retval 记录条数
	 */
	int32_t GetTotalNum() const;

	 /**
	 @brief  获取PartkeyUpdate和PartkeyDelete成功的记录数
	 @retval 记录条数
	 */
	int32_t GetSucNum() const;

	 /**
	 @brief  获取PartkeyUpdate和PartkeyDelete失败的记录数
	 @retval 记录条数
	 */
	int32_t GetFailedNum() const;

	 /**
	 @brief  获取PartkeyUpdate和PartkeyDelete本条响应第一条成功记录的offset
	 @retval 记录条数
	 */
	int32_t GetOffset() const;

private:
    const char*& GetCurrentFetchBuff();

    /**
	@brief	获取回包中的SplitTableKeyBuff数据, 内部使用
	@param	[OUT] splitTableKeyBuffLen	若buffer_data_size不为NULL，则向其所指位置写入SplitTableKeyBuff缓冲区长度。
	@retval NULL  当TcaplusServiceResponse对象未初始化时返回NULL。
	@retval !NULL SplitTableKeyBuff缓冲区首指针。
	*/
    const char* GetSplitTableKeyBuff(OUT size_t* splitTableKeyBuffLen = NULL) const;

    int32_t FetchRecordFromSqlRes(const TcaplusServiceRecord*& record);

	friend class TcaplusServer;
    friend class TcaplusServiceTraverser;
	friend class TcaplusService::ClientTraversal;
	friend class TcaplusService::ClientSysGet;
	friend class TcaplusService::ClientDescGet;
	friend class TcaplusService::ClientRecordGet;
    friend class TCAPLUS_COROUTINE_PB::TcaplusCoroutinePbApi;
    friend class TCAPLUS_ASYNC_PB::TcaplusAsyncPbApi;

	size_t GetInitUseMemSize() const {return m_init_use_mem_size;}
    
    //表的数据类型， -1 invalid(不用校验)， 0 tdr, 1 PLAIN 2 PB
    void SetTableDataProtocolType(int type);
private:
    bool        m_init_called;
    bool        m_init_succeed;

    //MAX_PACK_BUFFER_SIZE(5MB)是压缩前或压缩后的最大处理包buffer长度，系统边界limitation
    //决定业务数据包生成buffer(未压缩)后长度不可能超过MAX_PACK_BUFFER_SIZE.
    //类TcaplusServiceRequest的初始化列表中将常量m_workbuffer_size_for_compresssrc赋值为
    //MAX_PACK_BUFFER_SIZE, 永不改变.
    char* m_workbuffer_for_decompressdest;
    const size_t m_workbuffer_size_for_decompressdest;

    TcaplusServiceRecord* m_record;
    uint32_t    m_fetched_record_count;

    bool        m_has_unpack_head;  // 是否已解包head部分
    bool        m_has_unpack_body;  // 是否已解包body部分

    size_t m_decode_len; //从批量信息中已经获取的字节数
    const char* m_current_fetch_buff;

    Logger*     m_logger;
    int         m_module_id;
    char*       m_err_msg;
	size_t      m_init_use_mem_size;

    SqlResult m_sql_result; //索引查询类型为聚合查询时的结果
};

}

#endif  // __TCAPLUS_SERVICE_TCAPLUS_SERVICE_RESPONSE_H__


