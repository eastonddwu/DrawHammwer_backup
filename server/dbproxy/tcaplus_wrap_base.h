/*
 * * file name: tcaplus_wrap_base.h
 * * description: Tcaplus SDK底层封装，从ua_server移植并适配app_server协程模型。
 * *              提供Init/Proc/Finish生命周期管理、SendTcapReqCoroutine协程版请求发送、
 * *              ParseOneData回包解析等基础能力。
 * *              与ua_server版的关键差异：
 * *              1. 新增RpcCoroutine()，利用ContextController::Pending()内置的Yield/Resume
 * *              2. 新增SendTcapReqCoroutine()，供协程模式的RPC handler直接同步调用
 * *              3. 去掉Batch/List相关方法（MVP不需要）
 * *              4. 日志宏替换为APP_LOG_*
 * */

#ifndef _DB_TCAPLUS_WRAP_BASE_H_
#define _DB_TCAPLUS_WRAP_BASE_H_

#include <functional>
#include <set>
#include <vector>
// tcaplus SDK
#include "tcaplus_service/tcaplus_define.h"
#include "tcaplus_service/tcaplus_server.h"
#include "tcaplus_service/tcaplus_service_record.h"
#include "tcaplus_service/tcaplus_service_request.h"
#include "tcaplus_service/tcaplus_service_response.h"
// framework
#include "common/id_generator.h"
#include "core/context_controller.h"
#include "core/log.h"
#include "utils/db_conf.h"
#include "utils/db_error.h"

using namespace TcaplusService;

namespace dbproxy
{

struct TcapRegTable
{
    const char* table_name = nullptr;     // tcaplus集群上的业务表名（如"login"）
    const char* tdr_struct_name = nullptr; // TDR metalib中的struct名（如"tb_login"），为nullptr则等于table_name
    const unsigned char* metalib = nullptr;
};

using TcapCallback = std::function<int(int, TcaplusServiceResponse&)>;

struct TcaplusContext : public app::ClientContext
{
    TcaplusService::TcaplusServiceResponse* response = nullptr;
    TcapCallback callback = nullptr;
    int32_t result_code = -1;  // 协程模式下存储解析后的结果码
};

class TcapWrapBase
{
public:
    int Init(const TcaplusConf& tcaplus_conf, const std::vector<TcapRegTable>& reg_table,
             app::ContextController* context_ctrl, LPTLOGCATEGORYINST category);
    void Finish();

    size_t Proc();

    // 协程版请求发送：发送tcaplus请求后挂起协程，回包时自动唤醒，返回解析结果码
    template <typename T>
    int SendTcapReqCoroutine(uint64_t gid, const TcapCallback& callback, const char* table_name,
                             TCaplusApiCmds cmd, const T& data, int data_version = 0,
                             const char* field_name[] = nullptr, const unsigned field_count = 0);

    // 回调版请求发送（与ua_server一致，暂保留备用）
    template <typename T>
    int SendTcapReq(app::ServerContext* context, const TcapCallback& callback, const char* table_name,
                    TCaplusApiCmds cmd, const T& data, int data_version = 0,
                    const char* field_name[] = nullptr, const unsigned field_count = 0);

    // 从回包解析单条记录到TDR结构体
    template <typename T>
    int32_t ParseOneData(uint64_t gid, TcaplusServiceResponse& response, T& tb_data, int32_t& data_version);

protected:
    void ProcessTcaplusResponse(TcaplusServiceResponse& tcap_resp);

    // 协程版Rpc：发送请求 + Pending(Yield) + 回包时Resume + 读result_code + delete context
    int32_t RpcCoroutine(uint64_t req_id, const TcapCallback& callback,
                         TcaplusServiceRequest* request = nullptr);

    // 回调版Rpc（与ua_server一致，暂保留备用）
    int32_t Rpc(uint64_t req_id, app::ServerContext* context, const TcapCallback& callback,
                TcaplusServiceRequest* request = nullptr);

    int OnRpcCallback(int ret_code, TcaplusContext& tcaplus_context);
    void SetReqParam(TcaplusServiceRequest* request, uint64_t async_id, TCaplusApiCmds cmd,
                     int limit = -1, int offset = 0);

protected:
    // 构造顺序保证TcaplusServer先析构
    std::unique_ptr<TcaplusService::Logger> logger_;
    TcaplusService::TcaplusServer tcaplus_server_;
    app::ContextController* context_ctrl_ = nullptr;
};

// ============================================================================
// template implementations
// ============================================================================

template <typename T>
int TcapWrapBase::SendTcapReqCoroutine(uint64_t gid, const TcapCallback& callback, const char* table_name,
                                         TCaplusApiCmds cmd, const T& data, int data_version,
                                         const char* field_name[], const unsigned field_count)
{
    TcaplusServiceRequest* request = tcaplus_server_.GetRequest(table_name);
    if (!request)
    {
        APP_LOG_WARN(gid, "tcaplus_server GetRequest %s failed", table_name);
        return DB_ERR_TCAPLUS;
    }

    int32_t ret = request->Init(cmd);
    if (ret < 0)
    {
        APP_LOG_WARN(gid, "request(%d) init fail, ret=%d, last error:%s", cmd, ret, request->GetLastError());
        return DB_ERR_TCAPLUS;
    }

    uint64_t req_id = app::IDGenerator::GetInst().GenerateSeqID();
    SetReqParam(request, req_id, cmd);

    TcaplusServiceRecord* record = request->AddRecord();
    if (!record)
    {
        APP_LOG_WARN(gid, "request->AddRecord() failed. GetLastError=%s", request->GetLastError());
        return DB_ERR_TCAPLUS;
    }

    ret = record->SetData(reinterpret_cast<const char*>(&data), sizeof(data));
    if (ret < 0)
    {
        APP_LOG_WARN(gid, "record->SetData() failed. ret=%d", ret);
        return DB_ERR_TCAPLUS;
    }
    record->SetVersion(data_version);

    if (field_name)
    {
        ret = request->SetFieldNames(field_name, field_count);
        if (0 != ret)
        {
            APP_LOG_WARN(gid, "SetFieldNames error, ret=%d field_count=%u", ret, field_count);
            return DB_ERR_TCAPLUS;
        }
    }

    return RpcCoroutine(req_id, callback, request);
}

template <typename T>
int TcapWrapBase::SendTcapReq(app::ServerContext* context, const TcapCallback& callback, const char* table_name,
                               TCaplusApiCmds cmd, const T& data, int data_version,
                               const char* field_name[], const unsigned field_count)
{
    TcaplusServiceRequest* request = tcaplus_server_.GetRequest(table_name);
    if (!request)
    {
        APP_LOG_WARN(0, "tcaplus_server GetRequest %s failed", table_name);
        return DB_ERR_TCAPLUS;
    }

    int32_t ret = request->Init(cmd);
    if (ret < 0)
    {
        APP_LOG_WARN(0, "request(%d) init fail, ret=%d, last error:%s", cmd, ret, request->GetLastError());
        return DB_ERR_TCAPLUS;
    }

    uint64_t req_id = app::IDGenerator::GetInst().GenerateSeqID();
    SetReqParam(request, req_id, cmd);

    TcaplusServiceRecord* record = request->AddRecord();
    if (!record)
    {
        APP_LOG_WARN(0, "request->AddRecord() failed. GetLastError=%s", request->GetLastError());
        return DB_ERR_TCAPLUS;
    }

    ret = record->SetData(reinterpret_cast<const char*>(&data), sizeof(data));
    if (ret < 0)
    {
        APP_LOG_WARN(0, "record->SetData() failed. ret=%d", ret);
        return DB_ERR_TCAPLUS;
    }
    record->SetVersion(data_version);

    if (field_name)
    {
        ret = request->SetFieldNames(field_name, field_count);
        if (0 != ret)
        {
            APP_LOG_WARN(0, "SetFieldNames error, ret=%d field_count=%u", ret, field_count);
            return DB_ERR_TCAPLUS;
        }
    }

    return Rpc(req_id, context, callback, request);
}

template <typename T>
int32_t TcapWrapBase::ParseOneData(uint64_t gid, TcaplusServiceResponse& response, T& tb_data,
                                    int32_t& data_version)
{
    if (response.GetResult() == TcapErrCode::SVR_ERR_FAIL_RECORD_EXIST)
    {
        APP_LOG_WARN(gid, "record exist!, tcaplus_server recv response result=%d, err=%s, table(%s)",
                     response.GetResult(), response.GetLastError(), response.GetTableName());
        return DB_ERR_DATA_EXIST;
    }

    if (response.GetRecordCount() == 0)
    {
        if (response.GetResult() == TcapErrCode::TXHDB_ERR_RECORD_NOT_EXIST)
        {
            APP_LOG_DEBUG(gid, "no record, tcaplus_server recv response no record");
            return DB_ERR_NOT_DATA;
        }
        else
        {
            APP_LOG_WARN(gid, "tcaplus_server recv response result=%d, err=%s, table(%s)",
                         response.GetResult(), response.GetLastError(), response.GetTableName());
            return DB_ERR_TCAPLUS;
        }
    }

    if (response.GetResult() != 0)
    {
        if (response.GetResult() == TcapErrCode::SVR_ERR_FAIL_OUT_OF_USER_DEF_RANGE)
        {
            APP_LOG_WARN(gid, "TcapErrCode::SVR_ERR_FAIL_OUT_OF_USER_DEF_RANGE, table(%s)",
                         response.GetTableName());
            return DB_ERR_SIZE_OVER_FLOW;
        }
        if (response.GetResult() == TcapErrCode::SVR_ERR_FAIL_INVALID_VERSION)
        {
            APP_LOG_WARN(gid, "TcapErrCode::SVR_ERR_FAIL_INVALID_VERSION, table(%s)",
                         response.GetTableName());
            return DB_ERR_INVALID_VERSION;
        }
        APP_LOG_WARN(gid, "response return error code:%d, table(%s)", response.GetResult(),
                     response.GetTableName());
        return DB_ERR_TCAPLUS;
    }

    if (response.GetRecordCount() > 1)
    {
        APP_LOG_ERROR(gid, "record_count=%d while not allow recv multi-record by this method, table(%s)",
                      response.GetRecordCount(), response.GetTableName());
        return DB_ERR_TCAPLUS;
    }

    const TcaplusServiceRecord* record = nullptr;
    int ret = response.FetchRecord(record);
    if (0 != ret)
    {
        APP_LOG_WARN(gid, "get record error, ret=%d, table(%s)", ret, response.GetTableName());
        return DB_ERR_TCAPLUS;
    }

    ret = record->GetData(&tb_data, sizeof(tb_data), &data_version);
    if (0 != ret)
    {
        APP_LOG_WARN(gid, "get data from record error, ret=%d, table(%s)", ret, response.GetTableName());
        return DB_ERR_TCAPLUS;
    }

    return 0;
}

}  // namespace dbproxy

#endif
