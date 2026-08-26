/**********************************************************************
 * Copyright (c)             : 2011 - 2016 Tencent. All Rights Reserved.
 * File                      : tcaplus_service_traverser.h
 * TcaplusServiceApi Version : 3.18.0.
 * Description               : TCaplus Service API for traverser
 * modification history
 * ---------------------------------
 * Author                    : tcaplus
 * Date                      : 2016/11/25
 * ---------------------------------
 *
 **********************************************************************/
#ifndef _TCAPLUS_SERVICE_TABLE_TRAVERSER_
#define _TCAPLUS_SERVICE_TABLE_TRAVERSER_

#include <tdr/tdr.h>
#include "tcaplus_define.h"
#include "tcaplus_public_define.h"
#include "tcaplus_service_nonecopyable.h"

#define TCAPLUS_MAX_USERBUFF_LEN_BY_USER 1024

namespace tcaplus_protocol_cs
{
    class TCaplusNameSet;
    class RouteKeySet;
	class TCaplusPkg;
};

namespace TCAPLUS_KV
{
class TcaplusKVApi;
};

namespace Client
{
class ClientCmdOmsSelect;
class ClientCmdOmsSelect;
class ClientCmdSelect;
class ClientCmdDump;
} 

namespace TcaplusService
{

class TcaplusServiceResponse;
class TcaplusServer;
class Logger;
class TraverserManager;

class TcaplusServiceTraverser : public NoneCopyable
{
public:
    enum State
    {
        ST_IDLE          = 1,
        ST_READY         = 2,
        ST_NORMAL        = 4,
        ST_STOP          = 8,
        ST_RECOVERABLE   = 16,
        ST_UNRECOVERABLE = 32,
    };

    /**
     * @brief  启动遍历
     * @retval 0   启动遍历成功
     * @retval <0  启动遍历失败， \link ErrorCode \endlink
     */
    int Start();

    /**
     * @brief  停止遍历
     * @note   在任何状态均可以调用Stop停止遍历
     * @retval 0   停止遍历成功，注意此后这个对象不能再使用
     * @retval <0  停止遍历失败，返回的错误码具体参考 \link ErrorCode \endlink
     */
    int Stop();

    /**
     * @brief  继续一个中断的遍历
     * @note   目前只支持在NORMAL或RECOVERABLE状态执行Resume
     * @retval 0   成功地启动一个中断的遍历
     * @retval <0  启动一个中断的遍历失败，返回的错误码具体参考 \link ErrorCode \endlink
     */
    int Resume();


    /**
     * @brief 设置遍历表的类型
     * @note  此接口可以不使用，这种情况下，将默认为gerneic表
     * @param  [in] type  表的类型(0:generic表, 1:list表)
     */
    void SetTableType(int type);

   /**
    * @brief 对遍历记录的总数进行限制
    * @note  此接口可以不使用，这种情况下，将返回表中所有记录
    * @param  [in] limit  最多遍历的记录条数。若limit为-1表示将返回表中所有记录。若没有调用此函数，则相当于调用了SetLimit(-1)，将返回表中所有记录
    */
    void SetLimit(IN int limit);

    /**
     * @brief 对遍历时单次响应返回的记录数进行限制
     * @note  此接口不再有任何意义，相当于空操作, deprecated
     * @param  [in] batch_limit  单次响应最多返回的记录条数
     */
#if defined(_WIN32) || defined(_WIN64)
	void SetBatchLimit(IN int batch_limit);
#else
    void SetBatchLimit(IN int batch_limit) __attribute__((deprecated));
#endif

	/**
	 * @brief 设置遍历时期望返回的Value字段列表，数组方式
	 * @param [in] field_name Value字段列表，不要在field_name中包含key字段，否则会导致遍历失败；每个字段名称不超过 32 字节，名称以 '\0' 结尾
	 * @param [in] field_count	Value字段名称的个数
	 * @retval 0	设置成功
	 * @retval 1	设置失败，具体错误参考	\link ErrorCode \endlink
	 */
    int SetFieldNames(IN const char* field_name[], IN const unsigned field_count);

    /**
     * @brief 设置遍历时期望返回的值的字段列表，tdr方式
     * @param [in] table_meta 表的 meta 描述，与最初表定义的描述一致
     * @retval 0    设置成功
     * @retval 1    设置失败，具体错误参考  \link ErrorCode \endlink
     */
    int SetFieldNames(IN LPTDRMETA table_meta);

    /**
     * @brief 获取当前的遍历状态
     * @note  初次获取遍历对象，Start之前，对象处于 ST_READY 状态，也只有在 ST_READY 状态才能调用Start成功
     * @note  Start启动遍历之后、遍历未完成之前，只有处于 ST_NORMAL 状态才表明遍历过程正常。
     *        若进入 ST_RECOVERABLE 状态，可调用 Resume() 恢复一个中断的遍历，但不能频繁调用，以避免突发大量消息；
     *        若进入 ST_UNRECOVERABLE，应调用 Stop() 结束遍历。
     * @note  遍历完成之后，处于 ST_IDLE 状态，也应该调用 Stop() 结束遍历，释放资源。
     * @note  调用 Stop() 停止遍历之后，不能再持有这个对象，对该对象再进行操作，其行为是未定义的
     * @return  返回当前的遍历状态
     */
    State GetState();

    /**
     * @brief 设置遍历的路由范围，左闭右开。如果begin_index和end_index都为-1，则表示遍历所有的shard（默认如此）。
     * 表路由的总范围:[0,10000)
     * @param [in] begin_index 路由开始位置
     * @param [in] end_index   路由结束位置
     * @retval 0    设置成功
     * @retval -1   设置失败
     */
    int SetRange(IN int32_t begin_index, IN int32_t end_index);

	/**
	* @brief 对于Generic表, 设置遍历时每一个请求对应的应答包数目，不是应答记录数目，最大为100.
	* 对于List表, 设置遍历时每一个请求对应的所有应答包中list key的数目, 不是应答包数目，最大为100.
	* @param [in] num ，表示：对于Generic表，num为一个请求对应的应答包数目；对于List表，num为一个请求对应的应答List key数目.
	* @retval 0--->设置成功
	* @retval -1--->设置失败
	*/
    int SetResNumPerReq(IN uint32_t num);

	/**
	* @brief 设置是否只从slave节点上遍历
	* @param [in] flag
	*			true : 不管服务器配置如何，只从Tcpasvr Slave遍历。
	*			flase: 受服务器读分流配置的影响，既可能从Tcapsvr Master遍历也可能从Tcapsvr Slave遍历。
	*/
    void SetOnlyReadFromSlave(bool flag);

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

friend class TraverserManager;
friend class TCAPLUS_KV::TcaplusKVApi;
friend class Client::ClientCmdOmsSelect;
friend class Client::ClientCmdSelect;
friend class Client::ClientCmdDump;
private:
    TcaplusServiceTraverser();
    ~TcaplusServiceTraverser();

    int Init(Logger* logger, TcaplusServer* server, int module_id,int32_t app_id);
    int Reset(const int zone_id, const char *table_name,int index,int32_t app_id);
	int SendGetShardListRequest();
    int SendTraverseRequest();
    int SendTraverseListRequest();
    int OnRecvResponse(const TcaplusServiceResponse* response, OUT bool& drop,int index);
    void PrintToLog();
    void CopyRouteKeySet(tcaplus_protocol_cs::RouteKeySet* dst, const tcaplus_protocol_cs::RouteKeySet* src);
    int SetAsyncID(IN uint64_t async_id); //设置async id, 非0值生效
    /**
	* @brief 继续遍历
	* @note 调用这个函数的初始状态为ST_BUSY,在该函数中会继续校验收发包的Buffer是否符合预期,如果依旧繁忙
	*       则不会发送新的请求,直到收发Buffer空闲下来
	*/
	int ContinueTraverse();

    /**
	* @brief 获取shardid
	* @note restful遍历需设置limit，调用这个函数获取本次结束时所处的sharcid
	*/
    int32_t GetShardId()
    {
        return m_shard_list[m_shard_cur_idx];
    }

    /**
	* @brief 设置shardid
	* @note restful遍历需设置limit，调用这个函数设置上次结束时所处的sharcid，以便继续开始
	*/
    void SetShardId(int32_t id)
    {
        m_shard_id = id;
    }

    /**
	* @brief 获取m_offset
	* @note restful遍历需设置m_offset，调用这个函数获取本次结束时所处的m_offset
	*/
    uint64_t GetOffset()
    {
        return m_offset;
    }
    
    /**
	* @brief 设置m_offset
	* @note restful遍历需设置m_offset，调用这个函数设置上次结束时所处的m_offset，以便继续开始
	*/
    void SetOffset(uint64_t off)
    {
        m_offset = off;
    }

    /**
	* @brief 设置async id, 非0值生效
	* @note 用于唯一标识一个请求，例如restful接口中用于标识一个http链接
	*/

	/*
	* 通过遍历响应包返回的svrId和本地保存的svrId校验，检查是否发生了主备切换。
	*/
	int CheckIfSwitchMS(const char * resCurSrvID);

	int CopyNameList(TCaplusApiCmds cmd,tcaplus_protocol_cs::TCaplusNameSet& name_set,
    	const char* field_name[], const unsigned field_count);
	int32_t SetFieldNames(TCaplusApiCmds cmd,tcaplus_protocol_cs::TCaplusNameSet& name_set,
		const char* field_name[], const unsigned field_count);

	bool SetTCaplusPkgHead(TCaplusApiCmds cmd,tcaplus_protocol_cs::TCaplusPkg* &pkg);

private:

    static const int32_t NO_LIMIT = -1;

	uint32_t		m_idx;			                  //遍历器ID, 用于窜包控制
	uint32_t		m_request_idx;	                  //请求的ID, 每发送一个请求就自增1, 用于窜包控制

    int             m_table_type;
	int             m_zone_id;
    char            m_table_name[TCAPLUS_MAX_TABLE_NAME_LEN];
    tcaplus_protocol_cs::TCaplusNameSet* m_nameset;
    int64_t         m_limit;                          //遍历记录数上限
    int64_t         m_record_traversed_cnt;
    uint64_t        m_offset;                         //已遍历的记录数
    int32_t         m_key_completed;
    int32_t         m_shard_completed;
    int             m_shard_cnt;
    int32_t         m_shard_list[TCAPLUS_MAX_SHARD_ID_PER_TABLE];
    int32_t         m_shard_cur_idx;
    int32_t         m_shard_id;                       // 仅仅用于初始化后设置遍历起始位置。
    int64_t         m_list_traversed_cnt;
    bool            m_req_outstanding;
    time_t          m_last_send_req_time;
    State           m_state;
    Logger*         m_logger;
    int             m_module_id;
    TcaplusServer*  m_server;
    int32_t         m_begin_index;
    int32_t         m_end_index;
    uint32_t        m_continus_error_times;           // 连续收到错误包次数
    uint64_t        m_request_seq;                    // 请求sequence，用于控制丢包时重发
    uint64_t        m_expect_receive_seq;             // 期待接收的下一个应答sequence
    tcaplus_protocol_cs::RouteKeySet* m_route_key_set;

	/* m_response_num_per_req, 仅对Generic表有效。*/
    uint32_t        m_response_num_per_req;           // 每个请求的应答数目，默认是1个
    bool			m_only_read_from_slave;

    /* m_total_key_limit, m_batch_key_limit， m_key_traversed_cnt，和m_key_traversed_cnt_cur_request仅对List表有效。*/
    int64_t         m_total_key_limit;                //遍历KEY上限
    uint32_t        m_batch_key_limit;                //每个请求所返回的KEY上限，默认是1个
    uint64_t        m_key_traversed_cnt;              //已遍历的KEY数
    uint32_t        m_key_traversed_cnt_cur_request;  //当前请求已遍历的KEY数
    uint64_t        m_async_id;

    int             m_index;//观察者中该Traverse对象的索引位置
	char			m_shard_cur_svr_id[MAX_STR_LEN_SS];// 当前shard遍历开始后，会将svr_id带回，保存。

	char            m_user_buff[TCAPLUS_MAX_USERBUFF_LEN_BY_USER];    // 在遍历表时, 设置的userbuff
	int32_t         m_user_buff_length;                               // 在遍历表时, 设置的userbufflength

	int32_t         m_app_id;
	bool            m_need_send_next_request;

public:
		bool m_is_busy;  //遍历器是否处于busy状态 1:是busy,不自动发这个请求 0:不是busy

};

inline TcaplusServiceTraverser::State operator|(TcaplusServiceTraverser::State a, TcaplusServiceTraverser::State b)
{
    return static_cast<TcaplusServiceTraverser::State>(static_cast<int>(a) | static_cast<int>(b));
}

inline TcaplusServiceTraverser::State operator&(TcaplusServiceTraverser::State a, TcaplusServiceTraverser::State b)
{
    return static_cast<TcaplusServiceTraverser::State>(static_cast<int>(a) & static_cast<int>(b));
}


} // TcaplusService

#endif // _TCAPLUS_SERVICE_TABLE_TRAVERSER_

