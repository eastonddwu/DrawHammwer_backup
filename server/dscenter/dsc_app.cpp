/*
 * * file name: dsc_app.cpp
 * * description: DscApp::Setup/OnInit/OnTick实现
 */

#include "dsc_app.h"
#include "core/log.h"
#include "core/rpc_service.h"
#include "core/transport_type.h"
#include "dsa_load_mgr.h"
#include "dsc_rpc_meta.h"
#include "dsc_service.h"
#include "room.pb.h"
#include "svr_base/default_init.h"

namespace dscenter
{
void DscApp::Setup(const std::string& tbus2_agent_url)
{
    tbus2_agent_url_ = tbus2_agent_url;
}

bool DscApp::OnInit()
{
    if (!UseDefaultInit(*this, MySvrID(), tbus2_agent_url_))
        return false;

    if (!app::RpcService::GetInst().RegisterMethod(
            GetDscMethodCmd("AllocDsa"),
            {DscService::AllocDsa, &roomsvr::AllocDsaReq::default_instance(), &roomsvr::AllocDsaResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register AllocDsa fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetDscMethodCmd("ReportDsaLoad"),
            {DscService::ReportDsaLoad, &roomsvr::DsaReportLoadReq::default_instance(), &roomsvr::DsaReportLoadResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register ReportDsaLoad fail");
        return false;
    }

    APP_LOG_INFO(0, "DscApp init ok, svr_id(%u), busid(%u), agent_url(%s)",
                 MySvrID(), MySvrID(), tbus2_agent_url_.c_str());
    return true;
}

void DscApp::OnTick(uint64_t now_ms, uint64_t /*tick_count*/)
{
    DsaLoadMgr::GetInst().OnTick(now_ms);
}

}  // namespace dscenter
