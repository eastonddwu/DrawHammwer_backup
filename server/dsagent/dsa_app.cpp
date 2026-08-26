/*
 * * file name: dsa_app.cpp
 * * description: DsaApp::Setup/OnInit/OnProc/OnTick实现
 */

#include "dsa_app.h"
#include "core/log.h"
#include "core/rpc_service.h"
#include "core/svr_type.h"
#include "core/transport_type.h"
#include "dsa_rpc_meta.h"
#include "dsa_service.h"
#include "dsc_rpc_meta.h"
#include "process_mgr.h"
#include "room.pb.h"
#include "room_rpc_meta.h"
#include "svr_base/default_init.h"
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace dsagent
{
void DsaApp::Setup(const std::string& tbus2_agent_url, uint16_t ds_listen_port, uint16_t ds_port_start,
                   uint16_t ds_port_end, const std::string& ds_client_ip,
                   const std::string& dsa_host,
                   const std::string& ds_exec_path, const std::string& ds_type,
                   const std::string& ds_map)
{
    tbus2_agent_url_ = tbus2_agent_url;
    ds_listen_port_ = ds_listen_port;
    ds_port_start_ = ds_port_start;
    ds_port_end_ = ds_port_end;
    ds_client_ip_ = ds_client_ip;
    dsa_host_ = dsa_host;
    ds_exec_path_ = ds_exec_path;
    ds_type_ = ds_type;
    ds_map_ = ds_map;
}

bool DsaApp::OnInit()
{
    if (!UseDefaultInit(*this, MySvrID(), tbus2_agent_url_))
        return false;

    // dsa_host_ 是注入给 DS 的 -DsaHost 地址，DS 用此地址回连 dsagent TCP。
    // dsagent TCP 监听在 0.0.0.0（所有网卡），所以 dsa_host_ 可以是本机任何 IP。
    // 优先选非私有段 IP（内网/公网），这样跨机部署也能连通。
    // 若配置文件中指定了 dsa_host，则使用配置值。
    if (dsa_host_.empty())
    {
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == 0)
        {
            std::string non_private_ip;  // 非私有段 IP（优先，如 21.x 腾讯内网）
            std::string private_ip;       // 私有段 IP（fallback，如 10.x/192.168.x）
            for (auto* ifa = ifaddr; ifa; ifa = ifa->ifa_next)
            {
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                    continue;
                if (ifa->ifa_flags & IFF_LOOPBACK)
                    continue;
                auto* addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                char ip[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
                if (strcmp(ip, "0.0.0.0") == 0)
                    continue;
                uint32_t host_order = ntohl(addr->sin_addr.s_addr);
                uint8_t a = (host_order >> 24) & 0xFF;
                uint8_t b = (host_order >> 16) & 0xFF;
                bool is_private = (a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168));
                if (!is_private && non_private_ip.empty())
                    non_private_ip = ip;
                else if (is_private && private_ip.empty())
                    private_ip = ip;
            }
            freeifaddrs(ifaddr);
            dsa_host_ = non_private_ip.empty() ? private_ip : non_private_ip;
        }
        if (dsa_host_.empty())
            dsa_host_ = "127.0.0.1";  // 最终 fallback
        APP_LOG_INFO(0, "auto-detected dsa_host: %s", dsa_host_.c_str());
    }

    // 初始化DS TCP channel，dsagent作为TCP server监听DS进程连接
    // 监听 0.0.0.0（所有网卡），这样 DS 用 dsa_host_（可能是内网IP）也能连上
    if (!ds_tcp_channel_.Init(MySvrID(), "0.0.0.0", ds_listen_port_))
    {
        APP_LOG_ERROR(0, "ds tcp channel init fail, ds_listen_port(%u)", ds_listen_port_);
        return false;
    }

    if (!AddTransportInfo(app::TRANSPORT_DS_TCP, {&ds_tcp_channel_, &ds_recv_codec_, &ds_send_codec_}))
    {
        APP_LOG_ERROR(0, "add DS TCP transport info fail");
        return false;
    }

    // 注册RPC方法
    if (!app::RpcService::GetInst().RegisterMethod(GetDsaMethodCmd("CreateGame"),
                                                   {DsaService::CreateGame, &roomsvr::CreateGameReq::default_instance(),
                                                    &roomsvr::CreateGameResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register CreateGame fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(GetDsaMethodCmd("DestroyDs"),
                                                   {DsaService::DestroyDs, &roomsvr::DestroyDsReq::default_instance(),
                                                    &roomsvr::DestroyDsResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register DestroyDs fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetDsaMethodCmd("DsHeartBeat"), {DsaService::DsHeartBeat, &roomsvr::DsHeartBeatReq::default_instance(),
                                             &roomsvr::DsHeartBeatResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register DsHeartBeat fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(GetDsaMethodCmd("SetDsAuth"),
                                                   {DsaService::SetDsAuth, &roomsvr::SetDsAuthReq::default_instance(),
                                                    &roomsvr::SetDsAuthResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register SetDsAuth fail");
        return false;
    }

    // 注册结算代理handler（DS→roomsvr中继）
    if (!app::RpcService::GetInst().RegisterMethod(
            roomsvr::GetRoomMethodCmd("RoomDsPlayerSettle"),
            {DsaService::RoomDsPlayerSettleProxy,
             &roomsvr::RoomDsPlayerSettleReq::default_instance(),
             &roomsvr::RoomDsPlayerSettleResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomDsPlayerSettleProxy fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            roomsvr::GetRoomMethodCmd("RoomDsGameFinish"),
            {DsaService::RoomDsGameFinishProxy,
             &roomsvr::RoomDsGameFinishReq::default_instance(),
             &roomsvr::RoomDsGameFinishResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomDsGameFinishProxy fail");
        return false;
    }

    // 注册DS查询玩家信息代理handler（DS→dbproxy中继）
    if (!app::RpcService::GetInst().RegisterMethod(
            GetDsaMethodCmd("DsGetPlayerInfo"),
            {DsaService::DsGetPlayerInfoProxy,
             &roomsvr::DsGetPlayerInfoReq::default_instance(),
             &roomsvr::DsGetPlayerInfoResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register DsGetPlayerInfoProxy fail");
        return false;
    }

    // 初始化ProcessMgr
    ProcessMgr::GetInst().SetPortRange(ds_port_start_, ds_port_end_);

    // 启动时向dscenter上报一次负载，让dscenter知道本DSA存在
    {
        roomsvr::DsaReportLoadReq report_req;
        report_req.set_dsa_svr_id(MySvrID());
        report_req.set_ds_count(0);
        report_req.set_max_ds_count(ds_port_end_ - ds_port_start_ + 1);

        uint32_t report_cmd = dscenter::GetDscMethodCmd("ReportDsaLoad");
        app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, report_cmd, report_req, nullptr, nullptr,
                                       app::kGroupAddrDsCenter, 1000);
    }

    APP_LOG_INFO(0, "DsaApp init ok, svr_id(%u), busid(%u), agent_url(%s), ds_listen_port(%u), ds_client_ip(%s), dsa_host(%s), ds_exec(%s), ds_type(%s), ds_map(%s)",
                 MySvrID(), MySvrID(), tbus2_agent_url_.c_str(), ds_listen_port_, ds_client_ip_.c_str(),
                 dsa_host_.c_str(), ds_exec_path_.c_str(), ds_type_.c_str(), ds_map_.c_str());
    return true;
}

size_t DsaApp::OnProc(uint64_t now_ms, bool stop)
{
    if (stop)
        return 0;
    // 手动驱动DS TCP channel
    return ds_tcp_channel_.Loop(option_.max_deal_pkg_num);
}

void DsaApp::OnTick(uint64_t now_ms, uint64_t /*tick_count*/)
{
    ProcessMgr::GetInst().OnTick(now_ms);

    // �?5秒向dscenter上报一次负�?
    if (now_ms - last_report_ms_ >= 5000)
    {
        last_report_ms_ = now_ms;
        roomsvr::DsaReportLoadReq report_req;
        report_req.set_dsa_svr_id(MySvrID());
        report_req.set_ds_count(static_cast<uint32_t>(ProcessMgr::GetInst().GetDSCount()));
        report_req.set_max_ds_count(ds_port_end_ - ds_port_start_ + 1);

        uint32_t report_cmd = dscenter::GetDscMethodCmd("ReportDsaLoad");
        app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, report_cmd, report_req, nullptr, nullptr,
                                       app::kGroupAddrDsCenter, 2000);
    }
}

}  // namespace dsagent
