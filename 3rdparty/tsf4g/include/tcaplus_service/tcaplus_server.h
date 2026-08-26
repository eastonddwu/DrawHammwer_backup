/**********************************************************************
 * Copyright (c)             : 2011 - 2017 Tencent. All Rights Reserved.
 * File                      : tcaplus_server.h
 * TcaplusServiceApi Version : 3.28.0
 * Description               : TCaplus service API 客户端主类。
 * modification history
 * ---------------------------------
 * Author                    : Tcaplus
 * Date                      : 2017/09/25
 * ---------------------------------
 *
 **********************************************************************/

#ifndef __TCAPLUS_SERVICE_TCAPLUS_SERVER_H__
#define __TCAPLUS_SERVICE_TCAPLUS_SERVER_H__

#include "tcaplus_service_constant.h"
#include "tcaplus_service_nonecopyable.h"
#include <tdr/tdr.h>
#include "tcaplus_public_define.h"
#include "tcaplus_define.h"
#include <vector>
#include <map>
#include <string>

#ifndef int64_t
    #include <stdint.h>
#endif

struct tagSHtable;
struct tagTCapdirCSPkg;
struct tagMonitorMsg;

namespace TCAPNET {
    class CConnectorMgr;
	class CConnector;
}

namespace TCAPREST {
    class CRestProxyApp;
}

namespace tcaplus {
    namespace doc {
        class BSONObj;
        class Query;
    }
}

namespace tcaplus_protocol_cs
{
    class TCaplusPkg;
}

namespace TcaplusService
{

#define DEFAULT_CHECK_HEARTBEAT_INTERVAL    30  // 心跳包检查默认时间，30秒
#define MIN_CHECK_HEARTBEAT_INTERVAL        5   // 心跳包检查最小时间，低于此时限则强制改为此值 5秒

class Logger;
class DirServer;
class TcaplusServiceRequest;
class TcaplusServiceResponse;
class TcaplusServiceTraverser;
class TraverserManager;
class TcaplusRouter;
struct RouterNode;
class LatencyDataMgr;
class TcaplusServiceStatisticMgr;
class StatisticReportMgr;
class JudgeProxy;
class NetThread;
class CSocketPair;

enum SIGN_UP_FLAG {
    NOT_SIGN_UP = -1,
    SIGN_UP_SUCCESS = 0,
    SIGN_UP_FAIL = 1,
    SIGN_UP_NOT_IN_WRITE_LIST = 2
};

struct ProxyStatus
{
	enum {MAX_PROXY_URL_LEN = 256};
	char proxy_url[MAX_PROXY_URL_LEN]; //Proxy地址
	char connect_status[30];           //连接状态
	bool auth_succeed;                 //鉴权是否成功
};

struct ProxyStatusInfo
{
	enum{ MAX_PROXY_NUM = 200};        //设定为每个表至多对应200个Proxy
	char table_name[TCAPLUS_MAX_TABLE_NAME_LEN];
	int proxy_count;
	ProxyStatus proxy_status[MAX_PROXY_NUM];
};

struct Option
{
    bool check_meta;
    Option(bool c = true) : check_meta(c){}
};

enum API_DATA_PROTOCOL_TYPE {
    DATA_PROTOCOL_INVALID = -1,
    DATA_PROTOCOL_TDR = 0,
    DATA_PROTOCOL_PLAIN = 1,
    DATA_PROTOCOL_PB = 2,
};

struct RegisterTableInfo
{
    LPTDRMETA local_meta;    //注册的tdrmeta信息
    int  check_ret;          //是否已经校验, -1未校验， 0校验成功， 1校验失败
    int  svr_table_type;     //svr端记录的表类型，-1 未赋值 list generic
    int  data_protocol_type; //协议类型 -1 未赋值，0 TDR, 1 PLAIN, 2 PB
};


/// TCaplus service API的客户端主类
class TcaplusServer : public NoneCopyable
{
    friend class NetThread;
    friend class TcaplusRouter;
    friend class DirServer;
    friend class TcaplusServiceTraverser;
    friend class TCAPREST::CRestProxyApp;

public:
    ///  构造函数
    TcaplusServer();

    ///  析构函数
    virtual ~TcaplusServer();

    int ResetLogger(LPTLOGCATEGORYINST pstLogHandler);

    /**
    @brief 初始化函数
    @param [IN] logger          业务逻辑的日志对象指针。若传入NULL则不记录日志。
    @warning        logger指针所指向的日志对象生命周期必须大于TcaplusServer对象，
                    即日志对象应该在TcaplusServer::Init之前构造，在TcaplusServer::Fini之后析构。<br>
                    常见的错误是在tapp::pfnInit函数中使用tlog的日志句柄创建了临时的TLogger对象，
                    这会导致在tapp::pfnProc函数中操作TcaplusServer对象时程序crash在写日志的代码处。
    @param [IN] module_id       模块号，用于记录日志
    @param [IN] app_id          app_id，在网站注册相应服务以后，你可以得到该appid
    @param [IN] zone_id         业务所属的区服ID
    @param [IN] signature       签名/密码，在网站注册相应服务以后，你可以得到该字符串
    @param [IN] sync_timeout    sync_timeout是表示发送数据请求包和接收数据响应包时是否阻塞的意思，取值大于0表示阻塞式，取值等于0表示非阻塞式，单位为毫秒，默认值为0。
                                以函数int TcaplusServer::RecvResponse(TcaplusServiceResponse*& response)为例：当sync_timeout大于0时，若此刻有收到数据响应包
                                则函数RecvResponse()立马返回并通过输出参数response带出数据响应包，若此刻未有收到数据响应包则函数RecvResponse()会阻塞在这里一
                                直等待并不会立马返回(等待的最长时间为sync_timeout毫秒)；当sync_timeout等于0时，若此刻有收到数据响应包则函数RecvResponse()立马
                                返回并通过输出参数response带出数据响应包，若此刻未有收到数据响应包则函数RecvResponse()仍然会立马返回并且输出参数response是无意义的是NULL。
    @param [IN] net_buf_size    网络buffer的大小，单位字节，默认20M，小于20M会置位20M
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int Init(
        IN Logger* logger,
        IN int module_id,
        IN int64_t app_id,
        IN int zone_id,
        IN const char* signature,
        IN uint32_t sync_timeout = 0,
        IN uint32_t net_buf_size = 0
        );

	/**
    @brief 初始化函数
    @param [IN] logger          业务逻辑的日志对象指针。若传入NULL则不记录日志。
    @warning        logger指针所指向的日志对象生命周期必须大于TcaplusServer对象，
                    即日志对象应该在TcaplusServer::Init之前构造，在TcaplusServer::Fini之后析构。<br>
                    常见的错误是在tapp::pfnInit函数中使用tlog的日志句柄创建了临时的TLogger对象，
                    这会导致在tapp::pfnProc函数中操作TcaplusServer对象时程序crash在写日志的代码处。
    @param [IN] module_id       模块号，用于记录日志
    @param [IN] app_id          app_id，在网站注册相应服务以后，你可以得到该appid
	@param [IN] zone_list       区服ID列表
	@param [IN] zone_cnt        区服ID列表长度
    @param [IN] signature       签名/密码，在网站注册相应服务以后，你可以得到该字符串
    @param [IN] sync_timeout    当大于0时表示接收时同步等待超时时间(单位：毫秒)，为0时表示异步
    @param [IN] net_buf_size    网络buffer的大小，单位字节，默认20M，小于20M会置位20M
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
	@note 初始时, 默认zone_id为第一个区服ID zone_list[0], 请参看SetDefaultZoneId(int zone_id);
    */
    int Init(
        IN Logger* logger,
        IN int module_id,
        IN int64_t app_id,
        IN const int zone_list[],
        IN int zone_cnt,
        IN const char* signature,
        IN uint32_t sync_timeout = 0,
        IN uint32_t net_buf_size = 0
        );

    /**
    @brief 初始化函数，签名认证
    @param [IN] logger          业务逻辑的日志对象指针。若传入NULL则不记录日志。
    @warning        功能和Init一致，仅加密认证方式不同，向Tcaplus认证使用签名加密;
                    使用该接口需要确保创建APP时，必须设置签名认证方式
    @param [IN] module_id       模块号，用于记录日志
    @param [IN] app_id          app_id，在网站注册相应服务以后，你可以得到该appid
    @param [IN] zone_id         业务所属的区服ID
    @param [IN] signature       创建APP时设置的密码
                                注意：如果调用者不知道signature， Init()函数中传递NULL即可，但是必须知道一个用户名以及该用户名对应的密码，并在Init()函数之后立即调用SetUserNameAndPassword函数
    @param [IN] sync_timeout    sync_timeout是表示发送数据请求包和接收数据响应包时是否阻塞的意思，取值大于0表示阻塞式，取值等于0表示非阻塞式，单位为毫秒，默认值为0。
                                以函数int TcaplusServer::RecvResponse(TcaplusServiceResponse*& response)为例：当sync_timeout大于0时，若此刻有收到数据响应包
                                则函数RecvResponse()立马返回并通过输出参数response带出数据响应包，若此刻未有收到数据响应包则函数RecvResponse()会阻塞在这里一
                                直等待并不会立马返回(等待的最长时间为sync_timeout毫秒)；当sync_timeout等于0时，若此刻有收到数据响应包则函数RecvResponse()立马
                                返回并通过输出参数response带出数据响应包，若此刻未有收到数据响应包则函数RecvResponse()仍然会立马返回并且输出参数response是无意义的是NULL。
    @param [IN] net_buf_size    网络buffer的大小，单位字节，默认20M，小于20M会置位20M
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int SafeInit(
        IN Logger* logger,
        IN int module_id,
        IN int64_t app_id,
        IN int zone_id,
        IN const char* signature,
        IN uint32_t sync_timeout = 0,
        IN uint32_t net_buf_size = 0
        );
    
    /**
    @brief 初始化函数，签名认证
    @param [IN] logger          业务逻辑的日志对象指针。若传入NULL则不记录日志。
    @warning        功能和Init一致，仅加密认证方式不同，向Tcaplus认证使用签名加密;
                    使用该接口需要确保创建APP时，必须设置签名认证方式
    @param [IN] module_id       模块号，用于记录日志
    @param [IN] app_id          app_id，在网站注册相应服务以后，你可以得到该appid
    @param [IN] zone_list       区服ID列表
    @param [IN] zone_cnt        区服ID列表长度
    @param [IN] signature       创建APP时设置的密码
                                注意：如果调用者不知道signature， Init()函数中传递NULL即可，但是必须知道一个用户名以及该用户名对应的密码，并在Init()函数之后立即调用SetUserNameAndPassword函数
    @param [IN] sync_timeout    当大于0时表示接收时同步等待超时时间(单位：毫秒)，为0时表示异步
    @param [IN] net_buf_size    网络buffer的大小，单位字节，默认20M，小于20M会置位20M
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    @note 初始时, 默认zone_id为第一个区服ID zone_list[0], 请参看SetDefaultZoneId(int zone_id);
    */
    int SafeInit(
        IN Logger* logger,
        IN int module_id,
        IN int64_t app_id,
        IN const int zone_list[],
        IN int zone_cnt,
        IN const char* signature,
        IN uint32_t sync_timeout = 0,
        IN uint32_t net_buf_size = 0
        );
    /**
    @brief  资源释放函数
    */
    void Fini();
    /**
    @brief  设置多久未调用RecvResponse,产生错误日志，默认500，单位ms
    */
    void SetRecvIntervalTime(int64_t ms) {m_recvIntervalTime = ms; }
	/**
    @brief  dir地址转换函数
    @param [IN] dirUrl_old  目录服务器的url，形如"tcp://{ip|domain}:port"，目前支持域名或ip地址
    @param [IN] dir_url_list  域名解析的ip列表
    @param [OUT] isConv 是否执行了转换
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int DomainConvert(const char* dirUrl_old, std::vector<std::string>& dir_url_list, bool& isConv);

    /**
    @brief 添加目录服务器，通常应该在Init和之后、RegistTable以及其他函数之前调用。
    @note  3.29.0以及之后的版本，API内存中维护的Dir列表与定时拉取的Dir列表相同。
    @param [IN] dir_server_url  目录服务器的url，形如"tcp://172.25.40.181:10600"，目前只支持tcp协议。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int AddDirServerAddress(IN const char* dir_server_url);

    /**
    @brief 向默认分区注册表信息（连接dir服务器，认证，获取表路由）。应该在AddDirServerAddress之后调用。
    @note  单个TcaplusServer对象最多允许注册的表数目上限请参看常数TCAPDIR_MAX_TABLE。
    @param [IN] table_name      表名字符串。
    @param [IN] table_tdr_meta  表的meta描述，如果使用tdr方式操作request和response，则应设置该参数，否则可设为NULL。
    @param [IN] timeout_ms      网络操作超时时间，单位为毫秒，不可设置为0。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int RegistTable(
        IN const char* table_name,             // 表名
        IN LPTDRMETA table_tdr_meta,           // tdr表结构描述，非tdr格式用NULL
        IN unsigned timeout_ms
        );

	/**
    @brief 向指定分区注册表信息（连接dir服务器，认证，获取表路由）。应该在AddDirServerAddress之后调用。
    @note 单个TcaplusServer对象最多允许注册的表数目上限请参看常数TCAPDIR_MAX_TABLE。
	@param [IN] zone_id         分区ID。
	@param [IN] table_name      表名字符串。
    @param [IN] table_tdr_meta  表的meta描述，如果使用tdr方式操作request和response，则应设置该参数，否则可设为NULL。
    @param [IN] timeout_ms      网络操作超时时间，单位为毫秒，不可设置为0。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int RegistTable(
        IN const int zone_id,                  // 区名
        IN const char* table_name,             // 表名
        IN LPTDRMETA table_tdr_meta,           // tdr表结构描述，非tdr格式用NULL
        IN unsigned timeout_ms
        );

    /**
    @brief 王者使用，请在RegisterTable后使用，校验该表高版本meta的兼容性
    @note 校验高版本表meta的兼容性，校验成功后可使用SetNewTableMeta设置高版本表meta
	@param [IN] zone_id         分区ID。
	@param [IN] table_name      表名字符串。
    @param [IN] table_tdr_meta  该表高版本的meta
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int CheckNewTableMeta(
        IN const int zone_id,            // 区名
        IN const char* table_name,       // 表名
        IN LPTDRMETA tdr_meta            // 该表高版本的meta
        );

    /**
    @brief 王者使用，请在CheckNewTableMeta后使用，设置该表高版本meta
    @note 设置该表高版本meta，record的GetDataV2接口会使用高版本的meta做高低版本数据兼容
    @param [IN] zone_id         分区ID。
    @param [IN] table_name      表名字符串。
    @param [IN] table_tdr_meta  该表高版本的meta
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int SetNewTableMeta(
        IN const int zone_id,            // 区名
        IN const char* table_name,       // 表名
        IN LPTDRMETA tdr_meta            // 该表高版本的meta
        );

    /**
    @brief 连接所有表对应的tcaplus proxy服务器。若所有的proxy连通且鉴权通过，则立即返回成功；
           若到达超时时间，只要有一个proxy连通且鉴权通过，也会返回成功；否则返回超时错误。
    @param [IN] timeout_ms   网络操作超时时间，单位为毫秒。
    @param [IN] flag         暂时无用的参数，忽略。
    @retval <0   失败，返回对应的错误码。
    @retval 0    成功。至少有一个proxy连通并且鉴权通过才会返回0。
    */
    int ConnectAll(IN unsigned timeout_ms, IN int flag = 0);

    /**
    @brief 异步接口，连接所有表对应的tcaplus proxy服务器，都链接成功返回0
    @retval < 0   失败，返回对应的错误码。
    @retval > 0   认证中
    @retval =0    成功。
    */
    int IsConnectAll();

	/**
	 @brief 设置心跳包(ServiceApi发向tcaproxy的心跳包)的检查间隔
	 @param [IN] check_seconds  间隔时间，单位为s。不得低于MIN_CHECK_HEARTBEAT_INTERVAL，否则强制设置为MIN_CHECK_HEARTBEAT_INTERVAL。
	 */
	void SetCheckHeartbeatInterval(IN uint32_t check_seconds);

    /**
    @brief 获取默认分区,指定表获取对应的request对象指针。
    @note  获取的request对象是所有表共用的，而不是每个表对应一个，更不是临时生成，因此业务通常不应该保存此request对象指针，更不能delete。
           因为对于异步程序而言，一旦离开函数作用域，该对象随时有可能被其他异步操作修改掉。
    @param [IN] table_name   表名字符串。
    @retval NULL     失败，通常是因为之前并未通过RegistTable注册该表。
    @retval !NULL    成功，返回对应的request对象指针。
    */
    TcaplusServiceRequest* GetRequest(IN const char* table_name) const;

    /**
    @brief 获取指定分区,指定表对应的request对象指针。
    @note  获取的request对象是所有表共用的，而不是每个表对应一个，更不是临时生成，因此业务通常不应该保存此request对象指针，更不能delete。
    因为对于异步程序而言，一旦离开函数作用域，该对象随时有可能被其他异步操作修改掉。
	@param [IN] zone_id      分区ID。
	@param [IN] table_name   表名字符串。
    @retval NULL     失败，通常是因为之前并未通过RegistTable注册该表。
    @retval !NULL    成功，返回对应的request对象指针。
    */
    TcaplusServiceRequest* GetRequest(IN const int zone_id, IN const char* table_name) const;

    /**
    @brief 获取对应的request对象指针。
    @note  获取的request对象是所有表共用的，而不是每个表对应一个，更不是临时生成，因此业务通常不应该保存此request对象指针，更不能delete。
           因为对于异步程序而言，一旦离开函数作用域，该对象随时有可能被其他异步操作修改掉。
    @retval NULL     失败。
    @retval !NULL    成功，返回对应的request对象指针。
    */
	TcaplusServiceRequest* GetRequest() const;

    /**
    @brief  发送一个已经填写好的请求对象指针
    @param [IN] request   请求对象指针，不可为NULL。
    @retval <0   失败，返回对应的错误码。
    			 当返回的错误等于-0x1036(-4150)时，表示
    			 是由于发送缓冲区满导致发送失败。
    @retval 0    成功。
    */
    int SendRequest(IN TcaplusServiceRequest* request);

    /**
     @brief 切换请求到特定的游戏区, 并发送此请求
            等价于先调用 void TcaplusServiceRequest::SwitchAccessZone(const int zone_id)
                 再调用 int SendRequest(IN TcaplusServiceRequest* request)
     */
    int SendRequest(IN const int zone_id, IN TcaplusServiceRequest* request);

    /**
    @brief 接收响应包
    @param [OUT] response   如果收到完整的响应，则输出该响应对象的指针
    @warning    该函数返回的response指针是全局共用的，所以使用后请勿手动调用response->Destruct或者delete response。
    @retval <0   失败，返回对应的错误码。
    @retval 0    成功，但未收到完整的响应包。
    @retval 1    成功，收到1个完整的响应包，此时输出参数response为非NULL指针。
    */

    int RecvResponse(OUT TcaplusServiceResponse*& response);

    /**
    @brief 获取默认分区指定表对应的TcaplusServiceTraverser对象（即遍历对象）指针。
    @note  获取的 TcaplusServiceTraverser 对象可用于对名为table_name的表进行遍历，遍历期间无论是否收到遍历响应消息，
           建议经常通过此对象的 GetStatus 方法查询当前的遍历状态，以及时检测到遍历结束（特别是空表）或出错的情形
    @note  允许在遍历过程中再次调用这个接口获取该table_name正在进行中的遍历对象，但是不要对这个对象重复调用Start
    @param [IN] table_name   表名字符串。
    @retval NULL     失败，通常是因为之前并未通过RegistTable注册该表，或者有太多的表在同时遍历，没有更多的资源。
    @retval !NULL    成功，返回对应的TcaplusServiceTraverser对象指针。
    */
	TcaplusServiceTraverser* GetTableTraverser(IN const char* table_name) const;

	/**
    @brief 获取指定分区指定表对应的TcaplusServiceTraverser对象（即遍历对象）指针。
    @note  获取的 TcaplusServiceTraverser 对象可用于对名为table_name的表进行遍历，遍历期间无论是否收到遍历响应消息，
           建议经常通过此对象的 GetStatus 方法查询当前的遍历状态，以及时检测到遍历结束（特别是空表）或出错的情形
    @note  允许在遍历过程中再次调用这个接口获取该table_name正在进行中的遍历对象，但是不要对这个对象重复调用Start
	@param [IN] zone_id      游戏区ID。
    @param [IN] table_name   表名字符串。
    @retval NULL     失败，通常是因为之前并未通过RegistTable注册该表，或者有太多的表在同时遍历，没有更多的资源。
    @retval !NULL    成功，返回对应的TcaplusServiceTraverser对象指针。
    */
    TcaplusServiceTraverser* GetTableTraverser(IN const int zone_id, IN const char* table_name) const;

	/**
	@brief 设置默认的分区ID, 用于缺省zone_id的函数
		   如:
		   GetRequest(IN const char* table_name)
		   SendRequest(IN const char* table_name, IN const tcaplus::doc::Query& query, ...)
		   GetTableTraverser(IN const char* table_name)
		   ...
	@note  所传入的zone_id参数必需包含于TcaplusServer::Init()的zone_id或zone_id列表里
	@param [IN] zone_id  分区ID。
	@retval 0   设置成功
	@retval < 0 zone_id未找到
	 **/
	int SetDefaultZoneId(IN int zone_id);

	/**
	@brief 获取所有proxy的地址
	@note  所传入的zone_id参数必需包含于TcaplusServer::Init()的zone_id或zone_id列表里
	@param [IN] count  保存proxy_status_info数组的实际大小
	@retval = 0 获取成功
	@retval < 0 获取失败
	 **/
    int GetProxyStatus(INOUT ProxyStatusInfo proxy_status_info[], IN int proxy_status_info_size, INOUT int& count);

    /**
    @brief 获取上次使用的proxy地址
    */
    const char* GetUsedProxyUrl() const;

    /**
    @brief 获取表所连接的dir server
    @note  例如: 10.10.10.10:9999
    */
	void GetConnectedDirServerUrl(INOUT char* dir_server_url, IN int dir_server_url_len);

	/**
	 @brief 获取所有的dir server
	 */
	int GetAllDirServer(INOUT char dir_server_list[][MAX_URL_SIZE], INOUT int& dir_server_list_count);

	/**
	@brief 获取service api 的版本号，该版本号即为service api所在的发布包的版本号
	@retval 返回指向版本号的指针，例如版本号为"2.1.13.52612.x86_64_release_20131010"
	**/
	const char* GetApiVersion();

    /**
    @brief 以非tdr方式注册拉取回来的所有的表，目前仅供内部使用，用户暂时不能使用该接口
    @note 注册所有的表时，都没有传入表的meta描述
    @param [IN] timeout_ms      网络操作超时时间，单位为毫秒，该值目前不起作用。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int RegistAllTables(IN unsigned timeout_ms);

	/**
    @brief 获取默认分区所有的表名，目前仅供内部使用，用户暂时不能使用该接口
    @param [INOUT] table_name      保存所有表的表名，目前同一个业务，同一个分区下最多包含256张表。
    @param [INOUT] count    输入参数表示传入的table_name数组成员的最大个数，输出参数表示table_name数组成员的实际个数。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    @note 该函数必须在调用了AddDirServerAddress() 之后才能调用
    */
    int GetAllTableName(INOUT char table_name[][TCAPLUS_MAX_TABLE_NAME_LEN], INOUT int& count);

	/**
    @brief 获取指定分区所有的表名，目前仅供内部使用，用户暂时不能使用该接口
	@param [IN]    zone_id         分区ID
    @param [INOUT] table_name      保存所有表的表名，目前同一个业务，同一个分区下最多包含256张表。
    @param [INOUT] count    输入参数表示传入的table_name数组成员的最大个数，输出参数表示table_name数组成员的实际个数。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    @note 该函数必须在调用了AddDirServerAddress() 之后才能调用
    */
    int GetAllTableName(IN int zone_id, INOUT char table_name[][TCAPLUS_MAX_TABLE_NAME_LEN], INOUT int& count);

	/**
	@brief 用户设置自定义上报的文本内容, 业务线程调用
	@param [IN]	 text		 分区ID
	@note 该函数必须在调用了AddDirServerAddress() 之后才能调用
	*/
	void SetUserDefinedText(IN const char* text);

	/**
	@brief 弃用
	*/
	void OnUpdate();

	/**
	@brief 获取最近使用Proxy上的连接Buffer统计信息
	@param [INOUT]	 BuffStaticBetweenApiAndProxy 结构体定义信息见头文件
	@note 该函数必须在至少有一次发包行为后调用才有意义
	*/
	int GetLatestStaticBuffInfo(std::vector<BuffStaticBetweenApiAndProxy> & buff_vec);

	/**
	@brief 获取遍历器使用的Proxy上的连接Buffer统计信息
	@param [INOUT]	 BuffStaticBetweenApiAndProxy 结构体定义信息见头文件
	@param [IN]      zone_id 遍历的zone
	@param [IN]      table_name 遍历的table
	@note 该函数必须在至少有一次遍历发包行为后调用才有意义
	*/
	int GetTraverseConnBuffInfo(BuffStaticBetweenApiAndProxy& buff_static,int zone_id,const char* table_name);

    /**
     *     @brief 获得日志句柄
     *         */
    Logger* GetLogger();

    
    LatencyDataMgr* GetLatencyDataMgr();

    LPTDRMETA GetTableTdrMeta(int app_id, int zone_id, const char* table_name) const;

    int GetRegistTableCount() const { return m_total_table_count;}

    /**
     *     @brief 获取表类型
     *     @return -1 获取失败，0 获取成功
     *     @OUT tableType：-1 未赋值， 0 generic， 1 LIST
     **/
    int GetTableType(int app_id, int zone_id, const char* table_name, int& table_type) const;

     /**
     *     @brief 获取数据协议类型
     *     @return -1 获取失败，0 获取成功
     *     @OUT data_type -1 INVALID, 0 TDR, 1 PLAIN, 2 PB
     **/
    int GetTableDataProtocolType(int app_id, int zone_id, const char* table_name, int& data_type) const;

    /**
     *      @brief 获得DirServer数量
     **/
    unsigned GetDirServerNum();

    const char* GetLastErr()
    {
        return m_szLastError;
    }

    void SetLastErr(const char* err)
    {
        snprintf(m_szLastError, sizeof(m_szLastError), "%s", err);
    }
    void SetLastErrorCode(int code)
    {
        m_lastErrorCode = code;
    }

    void SetEnableTraverse(bool enable) { m_enable_traverse = enable; }
private:
    /**
    @brief restproxy数字签名时调用
    */
    int InitByRestProxy(
        IN Logger* logger,
        IN int module_id,
        IN int64_t app_id,
        IN const int zone_list[],
        IN int zone_cnt,
        IN const char* signature,
        IN uint32_t sync_timeout = 0
        );

    /**
    @brief 每分钟在主线程日志打印错误码统计，错误码在主线程解出，不宜加锁传给网络线程
    */
    void PrintErrorCodeStatistics();
	/**
    @brief 网络线程驱动函数，在网络线程中周期性调用
    */
    void NetThreadOnUpdate();

    /**
    @brief 获得网络线程专用的Buffer和Pkg
    */
    void NetThreadGetBufPkg(char* &buff, size_t &bufflen, tcaplus_protocol_cs::TCaplusPkg* &pkg);

    /**
    @brief 在网络线程中处理DirServer回包
    */
    int NetThreadProcDirMsg(const tagTCapdirCSPkg* response);

    /**
    @brief 在网络线程中统计上报数据
    */
    void NetThreadReportStatistic(int32_t app_id, int32_t zone_id, int64_t cur_time);

    /**
    @brief 在网络线程中初始化统计数据表信息
    */
	void NetThreadInitStatisticTable(int zone_id);

    /**
    @brief 在业务线程中检测游戏区下所有路由都已连接上且认证通过
    */
	int ConnectAll(TcaplusRouter* router, unsigned timeout_ms, int flag);

	/**
	@brief 获取本机IP，如果获取失败，将设置机器的ip为"127.0.0.1"
	@param  ip   输入输出参数， 用于保存本地ip
	@param len    输入参数，参数ip所指向的缓存大小
    	*/
	void GetLocalIP(INOUT char* ip, IN int len);

	int GetDirServerList();  //获取dir server列表

    int CreateAndInitRouter(IN int zone_id, OUT TcaplusRouter*& router);

    /**
     * 查询指定Zone和Table的路由结点
     */
    RouterNode* FindRouterNode(int zone_id, const char* table_name) const;

	//只给遍历请求使用,其他请求千万不要用
	int SendRequestForTraverse(int zone_id, const char* table_name,char* pack_buff,size_t pack_buff_len) const;

	/**
	 * 向m_table_routers插入新的路由结点, 若指定zone, table_name的结点已存在, 则插入失败
	 */
	int InsertRouterNode(int zone_id, const char* table_name, TcaplusRouter* router, LPTDRMETA table_meta, int subscript);

	/*
	@brief 设置单独路由
	*/
	void SetSpecialTcaproxyIPAndPort(const char* proxy_ip, int32_t proxy_port);

	/*
	@brief 撤销设置单独路由
	*/
	void ReSetSpecialTcaproxyIPAndPort();

	/*
	* 遍历m_table_routers，将存在的RouteNode节点的need_delete全部改为true，收到dir的全部表响应专用
	*/
	int TraveraslTableRouterSetNeedDelete(int32_t zone_id);

	/*
	* 遍历m_table_routers，将RouteNode节点中的need_delete为true的节点删除掉
	*/
	int TraveraslTableRouterDeleteNeedDelNode(int32_t zone_id);

	/*
	@brief 设置API与Proxy的心跳包间隔
	*/
	void SetHeartbeatInterval(uint32_t send_interval);

    void GetCombineBuf(char*& buf, int& len)
    {
        buf = m_combine_buf;
        len = m_combine_len;
    }

    ////////////////////////////////////////////////////////////////////////////////
    //
    // 1、在非cache模式
    //      a) 用户处理正常业务逻辑, 并周期性调用CanStopElegantly; 当CanStopElegantly返回的msg_count小于某个值(业务自己衡量)时进入收尾阶段
    //      b) 收尾阶段: 不再处理新的前端请求，不再向Api发送新的请求，仅调用收包和处理逻辑，直到CanStopElegantly返回true, 即可停止程序,
    //
    // 2、Cache模式:
    //      a) 用户停服前调用PrepareStop， 传入 flush_cache参数
    //      b) 若flush_cache为false，则api停止回写脏数据(脏数据在共享内存，不会丢失)， 进入收尾阶段
    //      c) 若flush_cache为true， 则api无视回写周期，全部cache表开始回写脏数据; 用户在正常处理逻辑中，加上对脏记录数的判断， 当脏记录数小于某个值后， 进入收尾阶段
    //      d) 收尾阶段: 和非Cache模式的收尾阶段一样.
    //
    // 在收尾阶段(只收包和处理收包， 预期时间在10ms级别)； 业务前端新来的请求需要自己缓存， 或不从相关通道中读新的请求; 这个是需要业务自己解决的;
    // 遍历请求需要业务自己在重启后重新遍历;

    // 是否可以停止api， msg_count为已发送请求且未处理的响应数; 通常为0返回true
    bool CanStopElegantly(int* msg_count/*还未处理完的消息数*/);

private:
    //成员变量重置
    void Reset();
    //设置dir告警错误码
    void SetDirAlarmErrorCode(int errorCode);
    //设置dir心跳上报的api hash环信息
    void ReportDirHashCircleStat(int zone_id, std::string& routeStat); 

	int32_t GetTableMetaVersion(const int zone_id, const char* table_name);

	int CompareEntryValueField(LPTDRMETA old_meta/*local meta*/,LPTDRMETA new_meta/*remote meta*/);

	LPTDRMETAENTRY CheckWhetherExistEntry(const char* entry_name, LPTDRMETA meta,OUT int &entry_offset);

	/**
    @brief 向指定分区注册表信息（连接dir服务器，认证，获取表路由）。应该在AddDirServerAddress之后调用。
    @note 单个TcaplusServer对象最多允许注册的表数目上限请参看常数TCAPDIR_MAX_TABLE。
	@param [IN] zone_id         分区ID。
	@param [IN] table_name      表名字符串。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int DeregistTable(
        IN const int zone_id,                  // 区ID
        IN const char* table_name              // 表名
        );

	/**
    @brief 将zone_cnt指定的分区数目zone_list重新注册到TcaplusServer, 方便已有区的新添加表的注册操作
    @note 单个TcaplusServer对象最多允许注册的区数目上限请参看常数CONTROL_MAX_ZONE_COUNT。
	@param [IN] zone_cnt         需要注册的分区数目。
	@param [IN] zone_list        分区列表。
    @param [IN] timeout_ms      网络操作超时时间，单位为毫秒，不可设置为0。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int RefreshZone(
        IN int zone_cnt, 
        IN const int zone_list[],
        IN unsigned timeout_ms
        );

	/**
    @brief 将新增的zone_cnt指定的分区数目zone_list注册到TcaplusServer, 方便下次表注册操作
    @note 单个TcaplusServer对象最多允许注册的区数目上限请参看常数CONTROL_MAX_ZONE_COUNT。
	@param [IN] zone_cnt         需要注册的分区数目。
	@param [IN] zone_list        分区列表。
    @param [IN] timeout_ms      网络操作超时时间，单位为毫秒，不可设置为0。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int RegistZone(
        IN int zone_cnt, 
        IN const int zone_list[],
        IN unsigned timeout_ms
        );

	/**
    @brief 将zone_cnt指定的分区数目zone_list从TcaplusServer中撤销注册
    @note 单个TcaplusServer对象最多允许撤销注册的区数目上限请参看常数CONTROL_MAX_ZONE_COUNT。
	@param [IN] zone_cnt         需要注册的分区数目。
	@param [IN] zone_list        分区列表。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int DeregistZone(
        IN int zone_cnt, 
        IN const int zone_list[]
        );

	/**
    @brief 获取当前TcaplusServer中已经注册的Zone列表
    @note 单个TcaplusServer对象最多允许注册的表数目上限请参看常数CONTROL_MAX_ZONE_COUNT。
	@param [INOUT] zone_cnt         输入表示列表的空闲长度，输出表示当前注册的分区数目。
	@param [INOUT] zone_list        输入时为空，输出时为分区列表。
    @retval <0   失败，返回对应的错误码
    @retval 0    成功
    */
    int GetRegisteredZone(
        INOUT int &zone_cnt, 
        INOUT int zone_list[]
        );

    int SetTableDataProtocolType(int zone_id, const char* table_name) const;
protected:
    bool        m_init_succeed;
    DirServer*  m_dir_server;
    int64_t     m_app_id;
	char        m_app_name[64];
    int*        m_zone_list;             // m_zone_list[i]: zone_id
    int         m_zone_cnt;
	int			m_default_zone_id;       // 默认zone_id
    Logger*     m_logger;
    LPTLOGCTX   m_default_log_ctx;
    int         m_module_id;
private:
    enum { GET_ROUTER_TIME_OUT = 2000 }; //用于Register table的获取路由的超时时间

    bool        m_init_called;
    bool        m_connect_all;
    int         m_signType; //0静态认证 1 签名认证
    char        m_signature[MAX_SIGNATURE_SIZE];
    char*       m_pack_buffer;
    size_t      m_pack_buffer_size;

    struct tagSHtable*      m_table_routers;           //维护 <zone_id table_name> --> [table_meta, TcaplusRouter*] 的关系

    TcaplusServiceRequest*  m_tcaplus_service_request;
    TcaplusServiceResponse* m_tcaplus_service_response;

    TraverserManager* m_traverser_mgr;
    int64_t m_last_send_route_req_time;

    int64_t m_call_onupdate_function_count;   // 网络线程调用函数 NetThreadOnUpdate 的次数

	TcaplusRouter** m_routers;   //TcaplusRouter[] for m_zone_list, one TcaplusRouter for one Zone

    int m_router_cnt;
    TcaplusRouter** m_router_list;

	bool m_get_dir_server_list_suc;  //smilehong add,获取dir server列表是否成功
	bool m_get_tables_and_access_suc;  //smilehong add, 获取所有表和访问点是否成功

	int64_t m_last_recv_route_res_time;  //上次收到DIR响应包的时间

	int64_t m_last_access_onupdate_time; //上次访问OnUpdate的时间

	int      m_dir_sign_up_flag;                        // 标定API向DIR注册是否成功
	uint32_t m_check_heartbeat_seconds;                 // 设置API与PROXY心跳包的检测周期
	uint32_t m_heartbeat_from_proxy_seconds;            // 设置API与PROXY心跳包的发送周期
	uint32_t m_heartbeat_from_dir_seconds;              // 设置API与DIR心跳包的发送周期
	uint32_t m_check_dir_available_period_seconds;      // 设置API检测DIR可用性的检测周期
	uint32_t m_update_dir_list_period_seconds;          // 设置API向DIR更新DIR列表的周期
	uint32_t m_update_tables_and_access_period_seconds; // 设置API向DIR更新TablesAndAccess的周期
    volatile bool m_dir_already_update_list;            // 记录API是否已经首次向DIR发送更新DIR列表请求
    volatile int64_t m_last_sent_dir_list_time;         // 记录API上次发送UpdateDirList的请求的时刻
    volatile int64_t m_last_sent_route_req_time;        // 记录API上次发送TablesAndAccess的请求的时刻

    int m_compress_threshold;

    int m_dynamic_connect_tcapdir_switch;

    uint32_t m_sync_timeout;

	int m_last_recv_response_zone_index;  //上次接收到响应的区的index

    LatencyDataMgr* m_latency_data_mgr;           //网络线程和业务线程共用的延时计算管理类
    TcaplusServiceStatisticMgr* m_statistic_mgr;  //网络线程和业务线程共用的统计数据管理类
    StatisticReportMgr* m_statistic_report_mgr;   //网络线程中用于API指标上报Gdata的管理类
    JudgeProxy* m_judge_proxy;                    //网络线程中用于判断PROXY可用性

    TCAPNET::CConnectorMgr*    m_connector_mgr;

    NetThread* m_net_thread;

	char  m_szLastError[256];   // socketpair init需要一个存放error日志的buf
    int m_lastErrorCode;

    int m_zone_tables_register_counts_sent;
    int m_zone_tables_register_counts_recv;
	bool m_dir_list_changed;

    char* m_combine_buf; // 用于网络线程回调时组包
    int   m_combine_len; // m_combine_buf的长度

    int m_total_table_count;
	std::map<int,std::map<std::string, RegisterTableInfo> > m_zone_register_table;//key:zone_id,value:tables of this zone
	bool m_need_check_table_f_local_2_remote;
	bool m_has_register_all_table;
	bool m_enable_traverse;
    int64_t m_lastRecvTime; //最近一次调用recvresponse的时间
    int64_t m_recvIntervalTime; //长时间未调用recv（默认500ms）时，打印错误日志
    int64_t m_lastPrintErrCodeTime; //最近一次打印错误码的统计
    int64_t m_maxRecvIntervalTimePerMin; //每分钟最长收包时间间隔
};

}

#endif  // __TCAPLUS_SERVICE_TCAPLUS_SERVER_H__

