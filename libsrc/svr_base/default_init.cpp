/*
 * * file name: default_init.cpp
 * * description: UseDefaultInit实现，创建TBus2Channel + PB codec并注册为default transport
 * */

#include "default_init.h"
#include "core/interface/routing_interface.h"
#include "core/log.h"
#include "core/routing/routing_mgr.h"
#include "core/rpc_service.h"
#include "core/server_core.h"
#include "core/transport_type.h"
#include "net/pb_codec.h"
#include "net/tbus2_channel.h"

namespace app
{
bool UseDefaultInit(ServerCore& svr, uint32_t busid, const std::string& agent_url)
{
    static RoutingMgr& routing_mgr = RoutingMgr::GetInst();
    static TBus2Channel tbus2_channel;
    static PbRecvCodec recv_codec;
    static PbSendCodec send_codec;

    if (!tbus2_channel.Init(busid, agent_url))
    {
        APP_LOG_ERROR(0, "UseDefaultInit: tbus2 channel init fail, busid(%u), agent_url(%s)", busid, agent_url.c_str());
        return false;
    }

    // 设置routing插件：mesh事件→RoutingMgr，TransportInfo.Send()→RoutingMgr查路由
    tbus2_channel.SetRouting(&routing_mgr);

    if (!svr.AddTransportInfo(TRANSPORT_PB_TBUSPP, {&tbus2_channel, &recv_codec, &send_codec, &routing_mgr}, true))
    {
        APP_LOG_ERROR(0, "UseDefaultInit: add default transport fail, busid(%u)", busid);
        return false;
    }

    APP_LOG_INFO(0, "UseDefaultInit: default transport ready, TRANSPORT_PB_TBUSPP, busid(%u), agent_url(%s)", busid,
                 agent_url.c_str());
    return true;
}

}  // namespace app
