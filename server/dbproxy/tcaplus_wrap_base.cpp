/*
 * * file name: tcaplus_wrap_base.cpp
 * * description: Tcaplus SDK底层封装实现，从ua_server移植并适配app_server协程模型
 * */

#include "tcaplus_wrap_base.h"
#include "core/rpc_error.h"
#include "db_error.h"

using namespace TcaplusService;

namespace dbproxy
{

static const int kCheckHeartbeatInterval = 120000;  // ms
static const int MAX_PROCESS_PKG_ONCE = 300;

int TcapWrapBase::Init(const TcaplusConf& tcaplus_conf, const std::vector<TcapRegTable>& reg_table,
                       app::ContextController* context_ctrl, LPTLOGCATEGORYINST category)
{
    logger_.reset(new TLogger(category));
    int ret = tcaplus_server_.Init(logger_.get(), tcaplus_conf.module_id, tcaplus_conf.app_id,
                                   tcaplus_conf.zone_id, tcaplus_conf.signature.c_str());
    if (ret < 0)
    {
        APP_LOG_ERROR(0, "tcaplus_server init fail, ret=%d", ret);
        return DB_ERR_TCAPLUS;
    }
    tcaplus_server_.SetCheckHeartbeatInterval(kCheckHeartbeatInterval);

    if (tcaplus_conf.dir_url.empty())
    {
        APP_LOG_ERROR(0, "tcaplus dir url empty");
        return DB_ERR_TCAPLUS;
    }

    for (size_t i = 0; i < tcaplus_conf.dir_url.size(); i++)
    {
        ret = tcaplus_server_.AddDirServerAddress(tcaplus_conf.dir_url[i].c_str());
        if (ret < 0)
        {
            APP_LOG_ERROR(0, "tcaplus_server AddDirServerAddress %s fail, ret=%d",
                          tcaplus_conf.dir_url[i].c_str(), ret);
            return DB_ERR_TCAPLUS;
        }
    }

    if (reg_table.empty())
    {
        APP_LOG_ERROR(0, "tcaplus register table empty");
        return DB_ERR_TCAPLUS;
    }

    for (auto&& node : reg_table)
    {
        const char* tdr_name = node.tdr_struct_name ? node.tdr_struct_name : node.table_name;
        LPTDRMETA table_meta = tdr_get_meta_by_name((LPTDRMETALIB)node.metalib, tdr_name);
        if (table_meta == NULL)
        {
            APP_LOG_ERROR(0, "tdr_get_meta_by_name fail, table=%s", node.table_name);
            return DB_ERR_TCAPLUS;
        }
        ret = tcaplus_server_.RegistTable(node.table_name, table_meta, 10000);
        if (ret < 0)
        {
            APP_LOG_ERROR(0, "tcaplus_server RegistTable %s fail, ret=%d", node.table_name, ret);
            return DB_ERR_TCAPLUS;
        }
    }

    ret = tcaplus_server_.ConnectAll(10000, 0);
    if (0 < ret)
    {
        APP_LOG_ERROR(0, "tcaplus_server ConnectAll fail, ret=%d", ret);
        return DB_ERR_TCAPLUS;
    }

    context_ctrl_ = context_ctrl;
    return 0;
}

void TcapWrapBase::Finish()
{
    tcaplus_server_.Fini();
}

size_t TcapWrapBase::Proc()
{
    size_t process_num = 0;
    tcaplus_server_.OnUpdate();
    for (int i = 0; i < MAX_PROCESS_PKG_ONCE; i++)
    {
        TcaplusService::TcaplusServiceResponse* response = nullptr;
        int ret = tcaplus_server_.RecvResponse(response);
        if (ret > 0 && response)
        {
            process_num++;
            ProcessTcaplusResponse(*response);
        }
        else if (ret == 0)
        {
            break;
        }
        else
        {
            APP_LOG_ERROR(0, "RecvResponse err, ret=%d", ret);
            break;
        }
    }
    return process_num;
}

void TcapWrapBase::ProcessTcaplusResponse(TcaplusServiceResponse& tcap_resp)
{
    const uint64_t req_id = tcap_resp.GetAsynID();
    TcaplusContext* tcaplus_context = static_cast<TcaplusContext*>(context_ctrl_->Awake(req_id, app::RPC_SUCCESS));
    if (!tcaplus_context)
    {
        APP_LOG_WARN(0, "unknown tcaplus response|req_id=%lu", req_id);
        return;
    }

    tcaplus_context->response = &tcap_resp;
    tcaplus_context->Run();
}

void TcapWrapBase::SetReqParam(TcaplusServiceRequest* request, uint64_t async_id, TCaplusApiCmds cmd, int limit,
                                int offset)
{
    request->SetAsyncID(async_id);
    request->SetSequence(2);

    static const std::set<uint32_t> flagset = {
        TCAPLUS_API_INSERT_REQ,  TCAPLUS_API_REPLACE_REQ, TCAPLUS_API_INCREASE_REQ,
        TCAPLUS_API_UPDATE_REQ,  TCAPLUS_API_DELETE_REQ,  TCAPLUS_API_LIST_DELETE_REQ,
        TCAPLUS_API_LIST_REPLACE_REQ, TCAPLUS_API_LIST_DELETE_BATCH_REQ,
        TCAPLUS_API_LIST_ADDAFTER_REQ, TCAPLUS_API_PB_BATCH_FIELD_GET_REQ};

    if (flagset.count(cmd))
        request->SetResultFlag(2);

    request->SetResultLimit(limit, offset);
    request->SetAddableIncreaseFlag(1);
    request->SetMultiResponseFlag(1);

    if (cmd != TCAPLUS_API_GET_REQ && cmd != TCAPLUS_API_LIST_GET_REQ && cmd != TCAPLUS_API_BATCH_GET_REQ &&
        cmd != TCAPLUS_API_LIST_GETALL_REQ && cmd != TCAPLUS_API_GET_BY_PARTKEY_REQ &&
        cmd != TCAPLUS_API_BATCH_GET_BY_PARTKEY_REQ && cmd != TCAPLUS_API_INSERT_REQ &&
        cmd != TCAPLUS_API_DELETE_REQ && cmd != TCAPLUS_API_LIST_DELETEALL_REQ)
        request->SetCheckDataVersionPolicy(CHECKDATAVERSION_AUTOINCREASE);
}

int32_t TcapWrapBase::RpcCoroutine(uint64_t req_id, const TcapCallback& callback,
                                    TcaplusServiceRequest* request)
{
    if (request)
    {
        int ret = tcaplus_server_.SendRequest(request);
        if (ret < 0)
        {
            APP_LOG_WARN(0, "tcaplus_server SendRequest failed. ret=%d", ret);
            return DB_ERR_TCAPLUS;
        }
    }

    TcaplusContext* tcaplus_context = new TcaplusContext;
    tcaplus_context->response = nullptr;
    tcaplus_context->callback = callback;
    tcaplus_context->result_code = -1;

    // ContextController::Pending()在协程模式(task.blocking_fun==nullptr)下:
    // 1. 设置callback -> 内部调用我们的lambda存result_code
    // 2. 设置recycle = coro->Resume()
    // 3. 调用coro->Yield()挂起
    // 回包时ProcessTcaplusResponse -> Awake() -> Run() -> callback(存result_code) -> Resume()
    // 协程恢复后，tcaplus_context仍然存活（框架在协程模式不delete它），我们手动读result_code再delete
    int32_t ret = context_ctrl_->Pending(req_id, 5000, tcaplus_context,
                                         {[=](int32_t ret_code, app::ServerContext*) {
                                              if (ret_code != app::RPC_SUCCESS)
                                              {
                                                  APP_LOG_WARN(0, "tcaplus rpc fail: seq_id=%lu, ret=%d",
                                                               req_id, ret_code);
                                              }
                                              tcaplus_context->result_code = OnRpcCallback(ret_code, *tcaplus_context);
                                          }});

    if (ret != app::RPC_SUCCESS)
    {
        APP_LOG_ERROR(0, "pending fail: seq_id=%lu, ret=%d", req_id, ret);
        delete tcaplus_context;
        return ret;
    }

    // 协程已被Resume恢复，result_code已由callback设置
    ret = tcaplus_context->result_code;
    delete tcaplus_context;
    return ret;
}

int32_t TcapWrapBase::Rpc(uint64_t req_id, app::ServerContext* context, const TcapCallback& callback,
                           TcaplusServiceRequest* request)
{
    if (request)
    {
        int ret = tcaplus_server_.SendRequest(request);
        if (ret < 0)
        {
            APP_LOG_WARN(0, "tcaplus_server SendRequest failed. ret=%d", ret);
            return DB_ERR_TCAPLUS;
        }
    }

    TcaplusContext* tcaplus_context = new TcaplusContext;
    tcaplus_context->response = nullptr;
    tcaplus_context->callback = callback;

    int32_t ret = context_ctrl_->Pending(req_id, 5000, tcaplus_context,
                                         {[=](int32_t ret_code, app::ServerContext*) {
                                              if (ret_code != app::RPC_SUCCESS)
                                              {
                                                  APP_LOG_WARN(0, "tcaplus rpc fail: seq_id=%lu. ret=%d", req_id,
                                                               ret_code);
                                              }
                                              context->ret_code = OnRpcCallback(ret_code, *tcaplus_context);
                                          },
                                          [=]() { delete tcaplus_context; }});

    if (ret != app::RPC_SUCCESS)
    {
        APP_LOG_ERROR(0, "pending fail: seq_id=%lu, ret=%d", req_id, ret);
        delete tcaplus_context;
    }

    return ret;
}

int TcapWrapBase::OnRpcCallback(int ret_code, TcaplusContext& tcaplus_context)
{
    static TcaplusServiceResponse dummy;
    auto response = tcaplus_context.response ? tcaplus_context.response : &dummy;
    return tcaplus_context.callback(ret_code, *response);
}

}  // namespace dbproxy
