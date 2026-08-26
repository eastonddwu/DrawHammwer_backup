/*
 * * file name: echo_app.cpp
 * * description: EchoApp::Setup/OnInit实现，见echo_app.h说明
 * */

#include "echo_app.h"
#include "core/log.h"
#include "core/rpc_service.h"
#include "core/transport_type.h"
#include "echo.pb.h"
#include "echo_rpc_meta.h"
#include "echo_service.h"
#include "svr_base/default_init.h"

namespace echo_demo
{
void EchoApp::Setup(uint16_t listen_port, uint32_t peer_id, const std::string& peer_ip, uint16_t peer_port,
                     const std::string& tbus2_agent_url)
{
    listen_port_ = listen_port;
    peer_id_ = peer_id;
    peer_ip_ = peer_ip;
    peer_port_ = peer_port;
    tbus2_agent_url_ = tbus2_agent_url;
}

bool EchoApp::OnInit()
{
    // TCP不设为default transport：框架只自动驱动default channel，TCP改为在OnProc()里手动Loop()
    if (!channel_.Init(MySvrID(), "0.0.0.0", listen_port_))
    {
        APP_LOG_ERROR(0, "channel init fail, svr_id(%u), listen_port(%u)", MySvrID(), listen_port_);
        return false;
    }
    channel_.AddPeer(peer_id_, peer_ip_, peer_port_);

    if (!AddTransportInfo(app::TRANSPORT_TCP_PB, {&channel_, &recv_codec_, &send_codec_}))
    {
        APP_LOG_ERROR(0, "add transport info fail");
        return false;
    }

    // tbus2 transport为可选项：依赖本地tbus2 agent进程，agent不存在/连接失败时只记WARN，
    // 不影响echo_demo原有TCP流程可用性
    // busid不能直接用MySvrID()：tbus2 namesrv保留gid=0(0.0.0.0)为无效组，需落在非零group(见kTBus2GroupBase)
    if (!UseDefaultInit(*this, kTBus2GroupBase | MySvrID(), tbus2_agent_url_))
    {
        APP_LOG_WARN(0, "tbus2 init fail, svr_id(%u), agent_url(%s), continue without tbus2 transport",
                     MySvrID(), tbus2_agent_url_.c_str());
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetEchoMethodCmd("EchoSync"),
            {EchoService::EchoSync, &EchoRequest::default_instance(), &EchoResponse::default_instance()}))
    {
        APP_LOG_ERROR(0, "register EchoSync fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            GetEchoMethodCmd("EchoCallPeer"),
            {EchoService::EchoCallPeer, &EchoRequest::default_instance(), &EchoResponse::default_instance()}))
    {
        APP_LOG_ERROR(0, "register EchoCallPeer fail");
        return false;
    }

    APP_LOG_INFO(0, "EchoApp init ok, svr_id(%u), listen_port(%u), peer_id(%u), peer(%s:%u)", MySvrID(), listen_port_,
                 peer_id_, peer_ip_.c_str(), peer_port_);
    return true;
}

size_t EchoApp::OnProc(uint64_t now_ms, bool stop)
{
    // TCP不是default transport，这里手动驱动收包，跟default channel使用一样的批量上限
    if (stop)
        return 0;
    return channel_.Loop(option_.max_deal_pkg_num);
}

}  // namespace echo_demo
