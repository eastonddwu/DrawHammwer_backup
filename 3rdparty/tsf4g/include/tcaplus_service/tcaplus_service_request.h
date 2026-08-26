/**********************************************************************
 * Copyright (c)             : 2011 - 2016 Tencent. All Rights Reserved.
 * File                      : tcaplus_service_request.h
 * Description               : TCaplus Service API for request
 * modification history
 * ---------------------------------
 * Author                    : tcaplus
 * Date                      : 2016/11/25
 * ---------------------------------
 *
 **********************************************************************/
#ifndef __TCAPLUS_SERVICE_TCAPLUS_SERVICE_REQUEST_H__
#define __TCAPLUS_SERVICE_TCAPLUS_SERVICE_REQUEST_H__

#include "tcaplus_define.h"
#include "tcaplus_service_nonecopyable.h"

#include "tcaplus_service_base.h"

class TCaplusKeySet;
class TCaplusValueSet_;

namespace tcaplus_protocol_cs
{
class TCaplusPkg;
class PerfTest;
class TCaplusNameSet;
};  //lint !e19

namespace TCAPLUS_KV
{
class TcaplusKVApi;
};

namespace Client
{
class ClientServiceApi;
class ExecuteCommand;
class ClientCmdTemplate;
class ClientCmdSelect;
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

namespace DATAPROCESSOR_NS
{
class DataProcessor;
};

namespace ULOG_MGR
{
class PbRecordUtil;
};

namespace TcaplusService
{

class TcaplusServiceRecord;
class TcaplusServiceTraverser;
class ClientTraversal;
class ClientDescGet;
class ClientRecordGet;
class ClientSysGet;
class ClientForApi;

typedef int (*TimeOutCallback)(int32_t app_id, int32_t zone_id, const char* table_name, uint32_t req_cmd,
    int error_code, int64_t seq, TcaplusServiceRecord* record);

/**
@brief 请求对象类
@note 该类由于需要缓存请求消息，因此消息对象比较大，目前单个对象占用的大小约为4M，请不要在栈上定义对应的局部对象，防止栈溢出。
*/
class TcaplusServiceRequest :public TcaplusServiceBase,public NoneCopyable
{
public:
    /// 构造函数
    TcaplusServiceRequest();

    /// 析构函数
    ~TcaplusServiceRequest();

    /**
    @brief 构造函数，初始化内存资源。
    @param [IN] app_id     app_id，在网站注册相应服务以后，你可以得到该appid
    @param [IN] zone_id    业务所属的区服ID
    @param [IN] logger     日志对象指针，用于记录debug和error日志。若传NULL则可通过GetLastError获取最后一次发生的错误信息。
    @param [IN] module_id  业务所属的区服ID
    @retval 0    成功
    @retval <0   失败，返回对应的错误码。
    */
    int Construct(IN int64_t app_id, IN int zone_id,
        IN Logger* logger = NULL, IN int module_id = 0);

    /**
    @brief 资源释放
    */
    void Destruct();

    /**
    @brief 重置请求数据，以便用于下一次请求准备
    @param [IN]    cmd            请求操作类型，具体参见 \link TCaplusApiCmds \endlink
    @param [IN]    user_buffer    用户自定义信息缓冲区，作为异步信息携带给服务端，回包时原样返回
    @param [IN]    buffer_data_size     自定义缓冲区中的数据大小，支持的最大长度未1024
    @param [IN]    async_id       请求对应的异步事务ID，tcaplus会将其值不变地通过请求对应的响应消息带回来
    @param [IN]    seq             请求消息序列号，目前Tcaplus不会在Service Api侧将请求消息序列号做自动加1操作，tcaplus在服务器端也不会处理此消息序列号值，tcaplus会将此值不变地通过请求对应的响应消息带回来
    @param [IN]    result_flag    响应标志.该参数的含义和"int SetResultFlag(IN char result_flag)"中参数IN char result_flag的含义是相同的。
    @retval 0    成功
    @retval <0   失败，返回对应的错误码。
    @note    user_buffer参数中保存的自定义数据最大长度为1024
    */
    int Init(IN TCaplusApiCmds cmd,
        IN const char* user_buffer = NULL, IN const size_t buffer_data_size = 0, IN uint64_t async_id = 0,
        IN int32_t seq = 0, IN char result_flag = 0);

    /**
    @brief  获取请求操作类型
    @retval TCAPLUS_API_INVALID_REQ  未初始化时，返回此无效命令号。
    @retval \link TCaplusApiCmds \endlink
    */
    TCaplusApiCmds GetCmd() const;

    /**
    @brief  获取请求子操作类型，对document操作才有意义
    @retval TCAPLUS_API_INVALID_REQ  未初始化时，返回此无效命令号。
    @retval \link TCaplusApiCmds \endlink
    */
    uint32_t GetSubCmd() const;

    /**
    @brief  获取key信息
    */
    TCaplusKeySet* GetKeyInfo() const;

    /**
    @brief  获取超时回调函数
    */
    void* GetTimeOutCbFunc() const;

    /**
    @brief  获取超时回调函数类型，是否是cache超时回调
    */
    bool GetTimeOutCbType() const;

    /**
    @brief  设置超时回调函数
    */
    void SetTimeOutCb(void* cb, bool is_from_cache = false);
    

    /**
    @brief  设置请求操作的表名及meta
    @param [IN] table_name      操作表名，以'\0'结尾，支持的最大长度为TCAPLUS_MAX_TABLE_NAME_LEN。
    @param [IN] table_tdr_meta  表的meta描述，如果使用tdr方式操作request，则应设置该参数，否则可设为NULL。
    @note  如果是从TcaplusServer中GetRequest得到的request对象，TcaplusServer已自动调用此方法，无需用户再显式调用。
    @retval 0    成功
    @retval <0   失败，返回对应的错误码。
    */
    int SetTable(IN const char* table_name, IN LPTDRMETA table_meta = NULL);

	/**
	@brief	获取请求操作的表名
	@param [OUT] table_name_size	  若table_name_size不为NULL，则向其所指位置写入表名长度。
	@retval NULL  TcaplusServiceRequest对象未初始化时返回NULL。
	@retval !NULL 指向操作的表名的指针。
	*/
    const char* GetTableName(OUT size_t* table_name_size = NULL) const;

    /**
    @brief  切换请求操作的游戏区ID
    */
    int SwitchAccessZone(const int zone_id);

    /**
    @brief  获取请求操作的游戏区ID
    @retval 操作游戏区ID
    */
    int GetZoneId() const;

    /**
    @brief  设置用户缓存，应答将携带返回
    @param [IN] user_buffer      用户缓存指针
    @param [IN] buffer_data_size 用户缓存长度，支持的最大长度为1024字节
    @retval 0    设置成功
    @retval <0   失败，返回对应的错误码。
    */
    int SetUserBuff(IN const char* user_buffer, IN const size_t buffer_data_size);

	/**
	@brief	获取请求中的用户缓存信息
	@param	[OUT] buffer_data_size	若buffer_data_size不为NULL，则向其所指位置写入表名长度。
	@retval NULL  当TcaplusServiceRequest对象未初始化时返回NULL。
	@retval !NULL 用户缓存的首指针。
	*/
    const char* GetUserBuff(OUT size_t* buffer_data_size = NULL) const;

    /**
    @brief  设置性能测试数据，应答将携带返回
    @param [IN] perftest_buffer  性能测试数据指针
    @param [IN] buffer_data_size 性能测试数据长度，支持的最大长度为256字节
    @retval 0    设置成功
    @retval <0   失败，返回对应的错误码。
    */
    int SetPerfTest(IN const char* perftest_buffer, IN const size_t buffer_data_size);

	/**
	@brief	获取请求中的性能测试数据
	@param	[OUT] buffer_data_size	若buffer_data_size不为NULL，则向其所指位置写入表名长度。
	@retval NULL  当TcaplusServiceRequest对象未初始化时返回NULL。
	@retval !NULL 性能测试数据的首指针。
	*/
    const char* GetPerfTest(OUT size_t* buffer_data_size = NULL) const;

    /**
    @brief  设置请求对应的异步事务ID。
    @param  [IN] async_id  请求对应的异步事务ID，tcaplus会将其值不变地通过请求对应的响应消息带回来
    @retval 0    设置成功
    @retval <0   失败，返回对应的错误码。
    */
    int SetAsyncID(IN uint64_t async_id);

    /**
    @brief  获取请求异步事务ID
    @retval 异步事务ID
    */
    uint64_t GetAsynID() const;

    /**
    @brief  设置请求序列号
    @retval 0    设置成功
    @retval <0   失败，返回对应的错误码。通常因为未初始化。
    */
    int SetSequence(IN int32_t seq);

    /**
    @brief  获取请求序列号
    @retval 请求序列号
    */
    int32_t GetSequence() const;

    /**
    @brief   设置sql查询语句，只用于sql查询
    @param  sql查询语句，必须是字符串
    @retval 0  设置成功
    @retval 非0 失败
    */
    int32_t SetSql(IN const char* sql);

    /**
    @brief  设置响应标志。主要用于Generic表的insert、increase、replace、update、delete操作和list表的单个delete replace操作。
    @param  [IN] result_flag  请求标志:
    							0表示: 只需返回操作执行成功与否
    							1表示: 返回与请求字段一致
    							2表示: 须返回变更记录的所有字段最新数据
    							3表示: 须返回变更记录的所有字段旧数据

    							对于batch_get请求，该字段设置为大于0时，某个key查询记录不存在或svr端产生的其它错误时会返回对应的key，
    							从而知道是哪个key对应的记录失败了
    @retval 0    设置成功
    @retval <0   失败，返回对应的错误码。通常因为未初始化。
    */
    int SetResultFlag(IN char result_flag);

	/**
		@brief	设置响应标志。主要是本次请求成功执行后返回给前端的数据

		result_flag 的取值范围如下:

	 TCaplusValueFlag_NOVALUE = 0,			  // 不返回任何返回值
	 TCaplusValueFlag_SAMEWITHREQUEST = 1,	  // 返回同请求一致的值
	 TCaplusValueFlag_ALLVALUE = 2, 		  // 返回tcapsvr端操作后所有字段的值
	 TCaplusValueFlag_ALLOLDVALUE = 3,		  // 返回tcapsvr端操作前所有字段的值


	下面是各个支持的命令字在设置不同的result_flag下执行成功后返回给API端的数据详细情况:

	 1. TCAPLUS_API_INSERT_REQ
		 如果设置的是TCaplusValueFlag_NOVALUE, 则操作成功后不返回数据
		 如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作成功后返回和请求一致的数据
		 如果设置的是TCaplusValueFlag_ALLVALUE, 则操作成功后返回本次insert操作后的数据
		 如果设置的是TCaplusValueFlag_ALLOLDVALUE, 则操作成功后返回空数据

	 2. TCAPLUS_API_REPLACE_REQ
		 如果设置的是TCaplusValueFlag_NOVALUE, 则操作成功后不返回数据
		 如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作成功后返回和请求一致的数据
		 如果设置的是TCaplusValueFlag_ALLVALUE, 则操作成功后返回本次replace操作后的数据
		 如果设置的是TCaplusValueFlag_ALLOLDVALUE, 则操作成功后返回tcapsvr端操作前的数据, 如果tcapsvr端没有数据,即返回为空

	 3. TCAPLUS_API_UPDATE_REQ
		 如果设置的是TCaplusValueFlag_NOVALUE, 则操作成功后不返回数据
		 如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作成功后返回和请求一致的数据
		 如果设置的是TCaplusValueFlag_ALLVALUE, 则操作成功后返回本次update操作后的数据
		 如果设置的是TCaplusValueFlag_ALLOLDVALUE, 则操作成功后返回tcapsvr端操作前的数据

	 4. TCAPLUS_API_INCREASE_REQ
		 如果设置的是TCaplusValueFlag_NOVALUE, 则操作成功后不返回数据
		 如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作成功后返回和请求一致的数据
		 如果设置的是TCaplusValueFlag_ALLVALUE, 则操作成功后返回本次increase操作后的数据
		 如果设置的是TCaplusValueFlag_ALLOLDVALUE, 则操作成功后返回tcapsvr端操作前的数据, 如果tcapsvr端没有数据,即返回为空

	 5. TCAPLUS_API_DELETE_REQ
		 如果设置的是TCaplusValueFlag_NOVALUE, 则操作成功后不返回数据
		 如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作成功后返回和请求一致的数据
		 如果设置的是TCaplusValueFlag_ALLVALUE, 则操作成功后返回空数据
		 如果设置的是TCaplusValueFlag_ALLOLDVALUE, 则操作成功后返回tcapsvr端操作前的数据

	 6. TCAPLUS_API_LIST_DELETE_BATCH_REQ
		 如果设置的是TCaplusValueFlag_NOVALUE, 则操作成功后不返回数据
		 如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作成功后返回和请求一致的数据, 暂时没有实现
		 如果设置的是TCaplusValueFlag_ALLVALUE, 则操作成功后不返回数据
		 如果设置的是TCaplusValueFlag_ALLOLDVALUE, 则操作成功后返回tcapsvr端操作前的数据, 凡是本次成功删除的index对应的数据都会返回

	 7. TCAPLUS_API_LIST_ADDAFTER_REQ
		 如果设置的是TCaplusValueFlag_NOVALUE, 则操作成功后不返回数据
		 如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作成功后返回和请求一致的数据, 暂时没有实现
		 如果设置的是TCaplusValueFlag_ALLVALUE, 则操作成功后, 返回本次插入的记录和本次淘汰的数据记录
		 如果设置的是TCaplusValueFlag_ALLOLDVALUE, 则操作成功后不返回数据

	 8. TCAPLUS_API_LIST_DELETE_REQ
		 如果设置的是TCaplusValueFlag_NOVALUE, 则操作成功后不返回数据
		 如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作成功后返回和请求一致的数据, 暂时没有实现
		 如果设置的是TCaplusValueFlag_ALLVALUE, 则操作成功后返回空数据
		 如果设置的是TCaplusValueFlag_ALLOLDVALUE, 则操作成功后返回tcapsvr端listdelete前的数据

	 9. TCAPLUS_API_LIST_REPLACE_REQ
		 如果设置的是TCaplusValueFlag_NOVALUE, 则操作成功后不返回数据
		 如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作成功后返回和请求一致的数据, 暂时没有实现
		 如果设置的是TCaplusValueFlag_ALLVALUE, 则操作成功后返回tcapsvr端listreplace后的数据
		 如果设置的是TCaplusValueFlag_ALLOLDVALUE, 则操作成功后返回tcapsvr端listreplace前的数据

	 @param  [IN] result_flag  请求标志:
								 0表示: 只需返回操作执行成功与否
								 1表示: 返回与请求字段一致
								 2表示: 须返回变更记录的所有字段最新数据
								 3表示: 须返回变更记录的所有字段旧数据

								 对于batch_get请求，该字段设置为大于0时，某个key查询记录不存在或svr端产生的其它错误时会返回对应的key，
								 从而知道是哪个key对应的记录失败了
	 @retval 0	  设置成功
	 @retval <0   失败，返回对应的错误码。通常因为未初始化。

	 */

	int SetResultFlagForSuccess (char result_flag);

	/**
		@brief	设置响应标志。主要是本次请求执行失败后返回给前端的数据

		result_flag 的取值范围如下:

		TCaplusValueFlag_NOVALUE = 0,			 // 不返回任何返回值
		TCaplusValueFlag_SAMEWITHREQUEST = 1,	 // 返回同请求一致的值
		TCaplusValueFlag_ALLVALUE = 2,			 // 返回tcapsvr端操作后所有字段的值
		TCaplusValueFlag_ALLOLDVALUE = 3,		 // 返回tcapsvr端操作前所有字段的值


	   下面是各个支持的命令字在设置不同的result_flag下执行失败后返回给API端的数据详细情况:

		1. TCAPLUS_API_INSERT_REQ
			如果设置的是TCaplusValueFlag_NOVALUE, 则操作失败后不返回数据
			如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作失败后返回和请求一致的数据
			如果设置的是TCaplusValueFlag_ALLVALUE, 不合理场景
			如果设置的是TCaplusValueFlag_ALLOLDVALUE, 如果获取到了tcapsvr端的数据则返回tcpasvr端的数据,如果没有获取到tcapsvr端的数据则返回空

		2. TCAPLUS_API_REPLACE_REQ
			如果设置的是TCaplusValueFlag_NOVALUE, 则操作失败后不返回数据
			如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作失败后返回和请求一致的数据
			如果设置的是TCaplusValueFlag_ALLVALUE, 不合理场景
			如果设置的是TCaplusValueFlag_ALLOLDVALUE, 如果获取到了tcapsvr端的数据则返回tcpasvr端的数据,如果没有获取到tcapsvr端的数据则返回空

		3. TCAPLUS_API_UPDATE_REQ
			如果设置的是TCaplusValueFlag_NOVALUE, 则操作失败后不返回数据
			如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作失败后返回和请求一致的数据
			如果设置的是TCaplusValueFlag_ALLVALUE, 不合理场景
			如果设置的是TCaplusValueFlag_ALLOLDVALUE, 如果获取到了tcapsvr端的数据则返回tcpasvr端的数据,如果没有获取到tcapsvr端的数据则返回空

		4. TCAPLUS_API_INCREASE_REQ
			如果设置的是TCaplusValueFlag_NOVALUE, 则操作失败后不返回数据
			如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作失败后返回和请求一致的数据
			如果设置的是TCaplusValueFlag_ALLVALUE, 不合理场景
			如果设置的是TCaplusValueFlag_ALLOLDVALUE, 如果获取到了tcapsvr端的数据则返回tcpasvr端的数据,如果没有获取到tcapsvr端的数据则返回空

		5. TCAPLUS_API_DELETE_REQ
			如果设置的是TCaplusValueFlag_NOVALUE, 则操作失败后不返回数据
			如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作失败后返回和请求一致的数据
			如果设置的是TCaplusValueFlag_ALLVALUE, 不合理场景
			如果设置的是TCaplusValueFlag_ALLOLDVALUE, 如果获取到了tcapsvr端的数据则返回tcpasvr端的数据,如果没有获取到tcapsvr端的数据则返回空

		6. TCAPLUS_API_LIST_DELETE_BATCH_REQ
			如果设置的是TCaplusValueFlag_NOVALUE, 则操作失败后不返回数据
			如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作失败后返回和请求一致的数据, 暂时没有实现
			如果设置的是TCaplusValueFlag_ALLVALUE, 不合理场景
			如果设置的是TCaplusValueFlag_ALLOLDVALUE, 则操作成功后返回tcapsvr端操作前的数据, 凡是本次成功删除的index对应的数据都会返回

		7. TCAPLUS_API_LIST_ADDAFTER_REQ
			如果设置的是TCaplusValueFlag_NOVALUE, 则操作失败后不返回数据
			如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作失败后返回和请求一致的数据, 暂时没有实现
			如果设置的是TCaplusValueFlag_ALLVALUE, 不合理场景
			如果设置的是TCaplusValueFlag_ALLOLDVALUE, 不返回数据

		8. TCAPLUS_API_LIST_DELETE_REQ
			如果设置的是TCaplusValueFlag_NOVALUE, 则操作失败后不返回数据
			如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作失败后返回和请求一致的数据, 暂时没有实现
			如果设置的是TCaplusValueFlag_ALLVALUE, 不合理场景
			如果设置的是TCaplusValueFlag_ALLOLDVALUE, 如果获取到了tcapsvr端的数据则返回tcpasvr端的数据,如果没有获取到tcapsvr端的数据则返回空

		9. TCAPLUS_API_LIST_REPLACE_REQ
			如果设置的是TCaplusValueFlag_NOVALUE, 则操作失败后不返回数据
			如果设置的是TCaplusValueFlag_SAMEWITHREQUEST, 则操作失败后返回和请求一致的数据, 暂时没有实现
			如果设置的是TCaplusValueFlag_ALLVALUE, 不合理场景
			如果设置的是TCaplusValueFlag_ALLOLDVALUE, 如果获取到了tcapsvr端的数据则返回tcpasvr端的数据,如果没有获取到tcapsvr端的数据则返回空

		@param	[IN] result_flag  请求标志:
									0表示: 只需返回操作执行成功与否
									1表示: 返回与请求字段一致
									2表示: 须返回变更记录的所有字段最新数据
									3表示: 须返回变更记录的所有字段旧数据

									对于batch_get请求，该字段设置为大于0时，某个key查询记录不存在或svr端产生的其它错误时会返回对应的key，
									从而知道是哪个key对应的记录失败了
		@retval 0	 设置成功
		@retval <0	 失败，返回对应的错误码。通常因为未初始化。

		*/

	int SetResultFlagForFail (char result_flag);

    /**
    @brief  获取响应标志
    @return 返回响应标志
    */
    char GetResultFlag() const;

	/**
    @brief  获取响应标志, 主要是采用 SetResultFlagForSuccess设置的值
    @return 返回响应标志
    */
	char GetResultFlagForSuccess () const;

	/**
    @brief  获取响应标志, 主要是采用SetResultFlagForFail设置的值
    @return 返回响应标志
    */
	char GetResultFlagForFail () const;

    /**
    @brief  设置空记录自增允许标志。用于Generic表的increase操作。
	@param  [IN] increase_flag  空记录自增允许标志。0表示不允许。1表示允许，当记录不存在时，将按字段默认值创建新记录再自增；若无默认值则返回错误
    @retval 0    设置成功
    @retval <0   失败，返回对应的错误码。通常因为未初始化。
    */
    int SetAddableIncreaseFlag(IN char increase_flag);

    /**
    @brief  获取空记录自增允许标志
    @return 返回空记录自增允许标志
    */
    char GetAddableIncreaseFlag() const;

	/**
    @brief  设置非排序LIST满时，插入元素时，删除旧元素的模式
    @param  [in] chListShiftFlag    TCAPLUS_LIST_SHIFT_NONE: 不允许删除元素，若LIST满，插入失败；TCAPLUS_LIST_SHIFT_HEAD: 移除最前面的元素；TCAPLUS_LIST_SHIFT_TAIL: 移除最后面的元素
            如果表是排序List,必须要进行淘汰,且淘汰规则是根据字段的排序顺序进行自动制定的,用户调用该接口会失败
    @retval 0              设置成功
    @retval 非0            设置失败，具体错误参见 \link ErrorCode \endlink
	*/
	int32_t SetListShiftFlag(IN const char chListShiftFlag = TCAPLUS_API_LIST_SHIFT_HEAD);

    /**
    @brief  获取LIST元素移除模式标志
    @return 返回LIST元素移除模式标志
    */
    char GetListShiftFlag() const;

    /**
    @brief  设置请求的通用标志位，可以通过"按位或"操作同时设定多个值
    @param  [IN]  flag. 请求标志位的值
    @retval 0     设置成功
    @retval <0    失败，返回对应的错误码。通常因为未初始化。
    @note   有效的标志位包括：
    *  TCAPLUS_FLAG_FETCH_ONLY_IF_MODIFIED:
    *       "数据变更才取回"标志位。在发起读操作之前，用户代码通过 TcaplusServiceRecord::SetVersion()
    *       带上本地缓存数据的版本号，并将此标志置位，那么存储端检测到当前数据与API本地缓存的数据版本
    *       一致时，表明该记录未发生过修改，API缓存的数据是最新的，因此在响应中将不会携带实际的数据，
    *       只是返回 TcapErrCode::COMMON_INFO_DATA_NOT_MODIFIED 的错误码
    *
    *       在请求中设置了此标志位之后，收到响应后应首先通过 TcaplusServiceResponse::GetFlags() 来获知
    *       发送请求时是否设置了TCAPLUS_FLAG_FETCH_ONLY_IF_MODIFIED标志.
    *
    *       只有如下请求支持设置此标志：
    *           TCAPLUS_API_GET_REQ,
    *           TCAPLUS_API_LIST_GET_REQ,
    *           TCAPLUS_API_LIST_GETALL_REQ
    *
    *  TCAPLUS_FLAG_FETCH_ONLY_IF_EXPIRED:
    *       "数据过期才取回"标志位。在发起读操作之前，用户代码通过 SetExpireTime() 设定数据过期时间，
    *       并将此标志置位，那么存储端若检测到记录在指定时间内发生过更新，则将数据返回，
    *       否则不返回实际数据，只是返回 TcapErrCode::COMMON_INFO_DATA_NOT_MODIFIED 的错误码。
    *
    *       在请求中设置了此标志位之后，收到响应后应首先通过 TcaplusServiceResponse::GetFlags() 来获知
    *       发送请求时是否设置了 TCAPLUS_FLAG_FETCH_ONLY_IF_EXPIRED 标志.
    *
    *       只有如下请求支持设置此标志：
    *           TCAPLUS_API_BATCH_GET_REQ
    *
    *  TCAPLUS_FLAG_ONLY_READ_FROM_SLAVE
    *       设置此标志后，读请求将会直接发送给Tcapsvr Slave 节点。
    *       Tcapsvr Slave 通常比较空闲，设置此标志有助于充分利用Tcapsvr Slave 资源。
    *
    *       适用场景:
    *                              对于数据实时性要求不高的读请求，
    *                              包括generic表和list表的所有读请求以及batchget，遍历请求
	*
    *  TCAPLUS_FLAG_LIST_RESERVE_INDEX_HAVING_NO_ELEMENTS
    *       设置此标志后，List表删除最后一个元素时需要保留index和version。
    *       ListDelete ListDeleteBatch ListDeleteAll操作在删除list表最后一个元素时，
    *          设置此标志在写入新的List记录时，版本号依次增长，不会被重置为1。
    *
    *       适用场景:
    *                              业务需要确定某个表在删除最后一个元素时是否需要保留index和version
    *                              主要涉及List表的使用体验
    *
    */
    int SetFlags(int32_t flag);

    /**
    @brief  清理请求的通用标志位，可以通过"按位或"操作同时设定多个值
    @param  [IN]  flag. 请求标志位的值
    @retval 0     设置成功
    @retval <0    失败，返回对应的错误码。通常因为未初始化。
    @note   有效的标志位列表及详细解释请参考 SetFlags()
    */
    int ClearFlags(int32_t flag);

    /**
    @brief   获取请求的通用标志位
    @return  返回请求的通用标志位
    @note   有效的标志位列表及详细解释请参考 SetFlags()
    */
    int32_t GetFlags() const;

    /**
    @brief  当设置了 TCAPLUS_FLAG_FETCH_ONLY_IF_EXPIRED 标志位时，用于指定记录过期时间，目前仅仅TCAPLUS_API_BATCH_GET_REQ操作支持指定记录过期时间。
    @param  [IN]  expire_time. 过期时间，单位为秒
    @retval 0     设置成功
    @retval <0    失败，返回对应的错误码。通常因为某些操作类型(cmd)不支持这种访问方式
    */
    int32_t SetExpireTime(uint32_t expire_time);

    /**
	@brief 设置需要查询或更新的Value字段名称列表，即部分Value字段查询和更新，
			 可用于get, batchget, partkeyget, listget, listgetall, listreplace, replace, update, tabletraverse操作。
    @param [IN] field_name    需要查询或更新的字段名称列表，每个字段名称不超过32字节，名称以'\0'结尾
    @param [IN] field_count   字段名称个数
    @retval 0                 设置成功
    @retval <0                设置失败，具体错误参见 \link ErrorCode \endlink
    @note 当在更新操作(replace, update, listreplace操作)中使用SetData()函数设置数据时，
    	使用SetData()函数设置数据时，如果只需要更新部分字段，可以使用该
    	函数设定需要更新的字段，但是，注意，在进行部分字段更新时，一
    	定要先调用SetData()函数，然后再调用SetFieldNames()函数，这样才能达到部分
    	部分字段更新的功能。

    @note 注意，在使用该函数设置字段名时，字段名只能包含value字段名，
       不能包含key字段名

    */
    int32_t SetFieldNames(IN const char* field_name[], IN const unsigned field_count);

    /**
	@brief 设置需要查询的Value字段名称，即部分Value字段查询，
			 可用于get, batchget, partkeyget, listget, listgetall操作。
    @param [IN] field_name   添加的需要查询的字段名称，每个字段名称不超过32字节，名称以'\0'结尾
    @retval 0                设置成功
    @retval <0               设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t AddFieldName(IN const char* field_name);

	/** \brief  添加LIST记录的元素索引值。该函数只对于TCAPLUS_API_LIST_DELETE_BATCH_REQ有效，对于
	                                    其它Command是无效的。
	*  \param  [in] idx          LIST元素索引值。不可取值TCAPLUS_API_LIST_PRE_FIRST_INDEX，不可取值TCAPLUS_API_LIST_LAST_INDEX。
	*  \retval 0                 设置成功
	*  \retval 非0               设置失败，具体错误参见 \link ErrorCode \endlink
	*/
    int32_t AddElementIndex(IN int32_t idx);

    /**
    @brief  向请求中添加一条记录。
    @param [IN] index    用于List操作，通常>=0，表示该Record在所属List中的Index(非下标)。
            用于List表中的TCAPLUS_API_LIST_ADDAFTER_REQ，index可以取值TCAPLUS_API_LIST_PRE_FIRST_INDEX或TCAPLUS_API_LIST_LAST_INDEX命令号。
            index是辅助key，tcaplus会自动维护其唯一性,新插入的记录index会往上自增。
            当cmd是TCAPLUS_API_LIST_ADDAFTER_REQ时，表示记录插入在该index所在的记录之后(隐含约束：index对应的记录必须已存在)；
            此时index还支持以下特殊值：
    	                TCAPLUS_API_LIST_PRE_FIRST_INDEX：新元素插入在第一个元素之前
                       TCAPLUS_API_LIST_LAST_INDEX：新元素插入在最后一个元素之后
            对于Generic操作，index无意义将被忽略。
    @retval \link TcaplusRecord \endlink   返回记录指针
    @retval NULL     添加记录失败
    */
    TcaplusServiceRecord* AddRecord(IN int32_t index = -1);

    /**
    @brief  如果此请求会返回多条记录，通过此接口对返回的记录做一些限制
    @param [IN] limit       需要查询的记录条数
    @param [IN] offset      记录起始编号；若设置为负值(-N, N>0)，则从倒数第N个记录开始返回结果
    @retval 0               设置成功
    @retval 0)，则从倒数第N个记录开始返回结果。参数offset的该含义，适用于Generic类型表的TCAPLUS_API_GET_BY_PARTKEY_REQ命令字以及List类型表的TCAPLUS_API_LIST_GETALL_REQ命令字。
    @retval 0               设置成功
    @retval <0              设置失败，具体错误参见 \link ErrorCode \endlink
    @note 对于Generic类型的部分Key查询，limit表示所要获取Record的条数，offset表示所要获取Record的开始下标；
          对于List类型的GetAll操作，limit表示所要获取Record的条数，offset表示所要获取Record的开始下标，在当前版本中这些Record一定属于同一个List.
          对于Generic类型的数据表：仅对TCAPLUS_API_GET_BY_PARTKEY_REQ命令字，该函数有效；对于除TCAPLUS_API_GET_BY_PARTKEY_REQ而外的命令字，该函数无效。对于List类型的数据表：仅对TCAPLUS_API_LIST_GETALL_REQ命令字，该函数有效；对于除TCAPLUS_API_LIST_GETALL_REQ而外的命令字，该函数无效。
    */
    int32_t SetResultLimit(IN int32_t limit = -1, IN int32_t offset = 0);

    /**
    @brief  设置是否允许一个请求包可以自动响应多个应答包，仅对ListGetAll和BatchGet协议有效。
    @param [IN] multi_flag   多响应包标示，1表示允许一个请求包可以自动响应多个应答包, 0表示不允许一个请求包自动响应多个应答包
    @retval 0                设置成功
    @retval <0               设置失败，具体错误参见 \link ErrorCode \endlink
    @note	分包应答，目前只支持ListGetAll和BatchGet操作；其他操作设置该值是没有意义的，函数会返回<0的错误码。
    */
    int32_t SetMultiResponseFlag(IN char multi_flag);

    /**
    @brief  获取响应标志
    @return 返回响应标志
    @note 目前只支持ListGetAll和BatchGet操作，其他操作获取的值是没有意义的
    */
    char GetMultiResponseFlag() const;

    /**
    @brief  将请求打包成消息buffer
    @param  [INOUT] buffer        打包的缓冲区
    @param  [INOUT] buffer_size 输入buffer缓冲区的最大长度；输出打包以后buffer缓冲区实际使用的长度。
    @retval 0                  设置成功
    @retval <0                 设置失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t Pack(INOUT char* buffer, INOUT size_t& buffer_size);

    /**
    @brief 	将消息buffer解包成请求包
    @param  [INOUT] buffer        需要解包的信息buffer
    @param  [INOUT] buffer_size 输入buffer缓冲区的最大长度
    @retval 0                  解包成功
    @retval <0                 解包失败，具体错误参见 \link ErrorCode \endlink
    */
    int32_t Unpack(IN const char* buffer, INOUT size_t buffer_size);

    /**
    @brief  可视化输出请求
    @param [INOUT] buffer          缓冲区指针
    @param [IN] buffer_size     缓冲区大小
    @retval 打包的缓冲区指针，内容以'\0'结尾
    */
    const char* Print(INOUT char* buffer, IN size_t buffer_size);

    /**
    @brief  获取最后一次记录的错误信息。只有初始化logger参数为NULL时，request才会自动生成该缓冲区；否则会自动写日志，本函数返回NULL。
    @retval 最后错误信息缓冲区。
    */
    const char* GetLastError();

    /**
    @brief  设置签名信息，仅用于TCAPLUS_API_APP_SIGNUP_REQ命令号
    @param [IN] type          鉴权类型，目前只支持0，表示静态密码
    @param [IN] signature     签名信息字符串，长度不能超过TCAPDIR_SIGNATURE_LEN。
    @retval 0 设置成功
    @retval <0 失败，返回对应的错误码
    */
    int SetSignature(int type, const char* signature);

	/**
    @brief  设置记录版本的检查类型
    @param [IN] type         版本检测类型，取值可以为:
                 CHECKDATAVERSION_AUTOINCREASE: 表示检测记录版本号。如果class TcaplusServiceRecord的void SetVersion(IN int32_t iVersion)函数传入的参数iVersion的值<=0,则仍然表示不关心版本号不关注版本号；如果class TcaplusServiceRecord的void SetVersion(IN int32_t iVersion)函数传入的参数iVersion的值>0，那么只有当该版本号与服务器端的版本号相同时，Replace, Update, Increase, ListAddAfter, ListDelete, ListReplace, ListDeleteBatch操作才会成功同时在服务器端该版本号会自增1。
                 NOCHECKDATAVERSION_OVERWRITE: 表示不检测记录版本号。如果class TcaplusServiceRecord的void SetVersion(IN int32_t iVersion)函数传入的参数iVersion的值<=0,则会把版本号1写入服务端的数据记录版本号(服务器端成功写入的数据记录的版本号最少为1)；如果class TcaplusServiceRecord的void SetVersion(IN int32_t iVersion)函数传入的参数iVersion的值>0，那么会把该版本号写入服务端的数据记录版本号。
                 NOCHECKDATAVERSION_AUTOINCREASE: 表示不检测记录版本号，将服务器端的数据记录版本号自增1，若服务器端新写入数据记录则新写入的数据记录的版本号为1。
			默认类型为CHECKDATAVERSION_AUTOINCREASE。
			当SetCheckDataVersionPolicy设置为CHECKDATAVERSION_AUTOINCREASE，而SetResultFlag设置为2返回所有字段值时，
			insert主键冲突，响应消息会返回最新记录；increase replace update list_delete list_replace list_batch_delete在
			版本号不正确出错时，响应消息会返回最新记录。
    @retval 0 设置成功
    @retval <0 失败，返回对应的错误码
    @note 此函数只适合Replace, Update, Increase, ListAddAfter, ListDelete, ListReplace, ListDeleteBatch操作
    */
    int SetCheckDataVersionPolicy(enum tagCHECKDATAVERSIONTYPE type);

    /**
        @brief  设置是否需要对该请求做压缩的开关和是否需要对该请求的响应做压缩的开关。
                         目前仅支持用于Generic表的get, batchget操作(命令字)。对于不支持的操作(命令字)
                         该函数的执行是无害空操作虽然其会返回错误提示ServiceApi用户。对于不支
                         持的操作(命令字)ServiceApi一定是不会压缩解压消息的。
                   注意: 该函数的作用域仅仅在于当前1条request消息及其对应的response消息，
                         当要处理新的request消息 时需要再次调用该函数.在处理1条request消息时如果
                         没有调用该函数则相当于req_compress_switch是SWITCH_OFF的并且resp_compress_switch是SWITCH_OFF的.
        @param  [IN] req_compress_switch, 可取值SWITCH_OFF或SWITCH_ON。
                            SWITCH_OFF表示不需要对该请求做压缩; SWITCH_ON表示需要对该请求做压缩.
                     [IN] resp_compress_switch, 可取值SWITCH_OFF或SWITCH_ON。
                            SWITCH_OFF表示不需要对该请求的响应做压缩; SWITCH_ON表示需要对该请求的响应做压缩.
        @retval 0    设置成功
        @retval != 0   设置失败，返回对应的错误码。
       */
    int32_t SetCompressSwitch(IN enum tagSWITCH req_compress_switch,
                          IN enum tagSWITCH resp_compress_switch);

     /**
         @brief 获取是否需要对该请求做压缩的开关值以及是否需要对该请求的响应做压缩
                          的开关值。
                   目前仅支持用于Generic表的get, batchget操作(命令字)。对于不支持的操作(命令字)
                         该函数会返回错误当然输出参数的值也是无意义的。对于不支持的
                         操作(命令字)ServiceApi一定是不会压缩解压消息的。
         @param  [OUT] req_compress_switch, 值可能为SWITCH_OFF或SWITCH_ON。
                               SWITCH_OFF表示不需要对该请求做压缩; SWITCH_ON表示需要对该请求做压缩.
                      [OUT] resp_compress_switch, 值可能为SWITCH_OFF或SWITCH_ON。
                               SWITCH_OFF表示不需要对该请求的响应做压缩; SWITCH_ON表示需要对该请求的响应做压缩.
         @retval 0    获取成功
         @retval != 0   获取失败，返回对应的错误码。
        */
     int32_t GetCompressSwitch(OUT enum tagSWITCH& req_compress_switch,
                                      OUT enum tagSWITCH& resp_compress_switch) const;

	/**
	   @brief  设置是否采用更快的方式执行SetKey(), SetValue(), AddFieldName()。
	   @param [IN] quicker_way_flag    是否采用更快的方式。当设置quicker_way_flag为true时，SetKey(), SetValue(), AddFieldName()函数中的strcmp()函数
		       的调用将大大减少; 当设置quicker_way_flag为false时，SetKey(), SetValue(), AddFieldName()函数中的strcmp()函数的调用会被严格执行，以保证会对重复的字段做去重处理。quicker_way_flag的默认值为false，即不采用更快的方式。注意：对于SetData()函数，无论quicker_way_flag的值是true还是flase，SetData()函数中都是采用更快的方式进行处理，因为tdr可以保证字段不会重复。
	   @note 当采用更快的方式时，由于减少了strcmp()函数的调用，所以用户在调用SetKey(), SetValue(), AddFieldName()函数时，需要保证传入的字段
			没有重复，因为更快的方式是不会对重复的字段做去重处理的。
			该函数调用一次永久生效，调用多次，按最后一次的调用生效。
	 */
	void SetQuickerWayFlag(IN bool quicker_way_flag);

	/**
	   @brief  获取是否采用更快方式的标志
	   @retval  是否采用更快方式的标志
	   */
	bool NeedQuickerWay();

	/**
	   @brief  设置是否已经调用AddFieldName函数，在BatchGet请求包设置的时候用到
	   @param [IN] call_addfieldname_flag   设置是否已经调用AddFieldName函数
	   @note 该函数仅供内部调用，外部用户无需关心
	 */
	void SetCallAddFieldNameFlag(bool call_addfieldname_flag);


	/**
	   @brief  获取TCaplusPkg指针。不清楚tcaplus协议的用户请不要使用该指针，否则会引起严重后果
	  @param [INOUT] 保存TCaplusPkg指针。
	  @note 该函数仅供内部调用，外部用户无需关心
   	*/
	void GetTcaplusPackagePtr(INOUT const void*& tcaplus_pkg);

	/**
	   @brief  设置当前表在统计数组中的下标
	   @param [IN] subscript 表在统计数组中的下标
	   @note 该函数仅供内部调用，外部用户无需关心也不应该调用
	 */
	void SetTableSubscript(int16_t subscript);

	/**
	   @brief  获取当前表在统计数组中的下标
	   @retval  返回当前表在统计数组中的下标
	   @note 该函数仅供内部调用，外部用户无需关心
	 */
	int16_t GetTableSubscript();

    size_t GetReqSendSize() const {return m_send_size;};

private:
    int CopyNameList(tcaplus_protocol_cs::TCaplusNameSet& name_set, const char* field_name[], const unsigned field_count);
    static uint32_t GetAllowRecordNum(uint32_t cmd);
    //void SetRequest(Request_Private *);
    int CopyValueName(TCaplusValueSet_& value_name_set, const char* field_name[], const uint32_t field_count);  //smile add

    int ManipulateFlags(int32_t flags, bool clear);

    /**
    @brief  设置SplitTableKeyBuff数据, 内部用
    @param [IN] splitTableKeyBuff  Splittablekey数据缓冲区指针
    @param [IN] splitTableKeyBuffLen Splittablekey数据缓冲区长度，支持的最大长度为TCAPLUS_MAX_KEY_FIELD_LEN
    @retval 0    设置成功
    @retval <0   失败，返回对应的错误码。
    */
    int SetSplitTableKeyBuff(IN const char* splitTableKeyBuff, IN const size_t splitTableKeyBuffLen);

	/**
	@brief	获取请求中的SplitTableKeyBuff数据, 内部使用
	@param	[OUT] splitTableKeyBuffLen	若buffer_data_size不为NULL，则向其所指位置写入SplitTableKeyBuff缓冲区长度。
	@retval NULL  当TcaplusServiceRequest对象未初始化时返回NULL。
	@retval !NULL SplitTableKeyBuff缓冲区首指针。
	*/
    const char* GetSplitTableKeyBuff(OUT size_t* splitTableKeyBuffLen = NULL) const;

    friend class TcaplusRouter; // TcaplusRouter类需要通过m_pkg的key算出hash code
    friend class TcaplusServiceTraverser;
	friend class TcaplusServiceRecord;
	friend class TcaplusServer;
	friend class TcaplusService::ClientTraversal;
	friend class TcaplusService::ClientDescGet;
	friend class TcaplusService::ClientRecordGet;
	friend class TcaplusService::ClientSysGet;
    friend class TcaplusService::ClientForApi;
	friend class TCAPLUS_KV::TcaplusKVApi;
    friend class Client::ClientServiceApi;
    friend class TCAPLUS_COROUTINE_PB::TcaplusCoroutinePbApi;
    friend class TCAPLUS_ASYNC_PB::TcaplusAsyncPbApi;
    friend class TCAPREST::CRestHttpMsgProcessor;
	friend class Client::ExecuteCommand;
    friend class Client::ClientCmdTemplate;
    friend class Client::ClientCmdSelect;
	friend class DATAPROCESSOR_NS::DataProcessor;
    friend class ULOG_MGR::PbRecordUtil;

	/**
	@brief  获取BatchGet请求中value字段的个数
	@retval	 BatchGet请求中value字段的个数
	@note 该函数仅供内部调用，外部用户无需关心
	*/
	uint32_t GetBatchGetValueFieldNum();

	/**zhe调用，false表示没有调用过
	   @note 该函数仅供内部调用，外部用户无需关心
	 */
	bool GetCallAddFieldNameFlag();

	size_t GetInitUseMemSize() const {return m_init_use_mem_size;}

    void ResetReqSendSize()      { m_send_size = 0;}

    //表的数据类型， -1 invalid(不用校验)， 0 tdr, 1 PLAIN 2 PB
    void SetTableDataProtocolType(int type);
    
private:
    bool        m_init_called;
    bool        m_init_succeed;

	char        m_key_buff[TCAPLUS_MAX_KEY_ALL_FIELDS_LEN];
	char        m_value_buff[TCAPLUS_MAX_VALUE_ALL_FIELDS_LEN];

    //MAX_PACK_BUFFER_SIZE(5MB)是压缩前或压缩后的最大处理包buffer长度，系统边界limitation
    //决定业务数据包生成buffer(未压缩)后长度不可能超过MAX_PACK_BUFFER_SIZE.
    //类TcaplusServiceRequest的初始化列表中将常量m_workbuffer_size_for_compresssrc赋值为
    //MAX_PACK_BUFFER_SIZE, 用不改变.
    char*       m_workbuffer_for_compresssrc;
    const size_t m_workbuffer_size_for_compresssrc;

    TcaplusServiceRecord* m_record;
    uint32_t    m_record_count;

    Logger*     m_logger;
    int         m_module_id;
    char*       m_err_msg;

    int64_t     m_app_id;
    int         m_zone_id;

	//是否采用更快的方式来执行SetData(), SetValue(), SetKey(), AddFieldName函数
	bool        m_quicker_way_flag;
	bool        m_call_addfieldname_flag; //是否已经调用AddFieldName()函数，主要用于BatchGet请求包设置

	int16_t     m_table_subscript; //当前表在表统计数组中的下标

	char*       m_temp_field_buff;   //这里该变量只是为CopyValueName里交互数据使用的,因为需要TCAPLUS_MAX_VALUE_FIELD_LEN,故在堆上
	bool        m_is_sort_list_table;//是否为sort_list_table
	size_t      m_init_use_mem_size;

    bool        m_is_cache_cb; //是否是cache超时回调
    void*       m_cb;          //超时回调函数
    size_t      m_send_size;   //发送数据大小
};

}

#endif  // __TCAPLUS_SERVICE_TCAPLUS_SERVICE_REQUEST_H__


