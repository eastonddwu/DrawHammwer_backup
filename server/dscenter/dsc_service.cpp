/*
 * * file name: dsc_service.cpp
 * * description: DscService各RPC handler实现
 */

#include "dsc_service.h"
#include "core/log.h"
#include "core/rpc_error.h"
#include "dsa_load_mgr.h"
#include "room.pb.h"

namespace dscenter
{

void DscService::AllocDsa(app::RpcContext& context)
{
    const auto& req = static_cast<const roomsvr::AllocDsaReq&>(context.GetReq());
    uint64_t room_id = req.room_id();

    APP_LOG_INFO(0, "AllocDsa recv, room_id(%llu)", static_cast<unsigned long long>(room_id));

    auto& rsp = static_cast<roomsvr::AllocDsaResp&>(context.GetRsp());

    uint32_t dsa_svr_id = DsaLoadMgr::GetInst().AllocDsa();
    if (dsa_svr_id == 0)
    {
        APP_LOG_WARN(0, "AllocDsa no available DSA, room_id(%llu)", static_cast<unsigned long long>(room_id));
        rsp.set_ret_code(1); // 无可用DSA
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    rsp.set_ret_code(0);
    rsp.set_dsa_svr_id(dsa_svr_id);
    context.ret_code = app::RPC_SUCCESS;

    APP_LOG_INFO(0, "AllocDsa ok, room_id(%llu), dsa_svr_id(0x%08X)",
                 static_cast<unsigned long long>(room_id), dsa_svr_id);
}

void DscService::ReportDsaLoad(app::RpcContext& context)
{
    const auto& req = static_cast<const roomsvr::DsaReportLoadReq&>(context.GetReq());

    APP_LOG_INFO(0, "ReportDsaLoad recv, dsa_svr_id(0x%08X), ds_count(%u/%u)",
                 req.dsa_svr_id(), req.ds_count(), req.max_ds_count());

    DsaLoadMgr::GetInst().UpdateLoad(req.dsa_svr_id(), req.ds_count(), req.max_ds_count());

    auto& rsp = static_cast<roomsvr::DsaReportLoadResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

}  // namespace dscenter
