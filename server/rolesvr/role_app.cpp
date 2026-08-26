/*
 * * file name: role_app.cpp
 * * description: RoleApp::Setup/OnInit实现，见role_app.h说明
 * */

#include "role_app.h"
#include "core/log.h"
#include "core/rpc_service.h"
#include "core/transport_type.h"
#include "role.pb.h"
#include "role_rpc_meta.h"
#include "role_service.h"
#include "svr_base/default_init.h"

namespace rolesvr
{
void RoleApp::Setup(const std::string& tbus2_agent_url)
{
    tbus2_agent_url_ = tbus2_agent_url;
}

bool RoleApp::OnInit()
{
    // MySvrID() 在 --conf-file 模式下已是完整 busid（如 0x04010001）；
    // 命令行模式下 main.cpp 已在调用 Init() 前完成 kRoleGroupBase | svr_id 的 OR。
    if (!UseDefaultInit(*this, MySvrID(), tbus2_agent_url_))
        return false;

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoleMethodCmd("Login"),
            {RoleService::Login, &LoginReq::default_instance(), &LoginResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register Login fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoleMethodCmd("SetUserInfo"),
            {RoleService::SetUserInfo, &SetUserInfoReq::default_instance(), &SetUserInfoResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register SetUserInfo fail");
        return false;
    }

    APP_LOG_INFO(0, "RoleApp init ok, svr_id(%u), busid(%u), agent_url(%s)",
                 MySvrID(), MySvrID(), tbus2_agent_url_.c_str());
    return true;
}

}  // namespace rolesvr
