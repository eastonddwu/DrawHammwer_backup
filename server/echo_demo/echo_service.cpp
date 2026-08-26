/*
 * * file name: echo_service.cpp
 * * description: EchoService各RPC handler实现，见echo_service.h说明
 * */

#include "echo_service.h"
#include "core/log.h"
#include "core/rpc_error.h"
#include "core/rpc_service.h"
#include "core/transport_type.h"
#include "echo.pb.h"
#include "echo_app.h"
#include "echo_rpc_meta.h"

namespace echo_demo
{
void EchoService::EchoSync(app::RpcContext& context)
{
    const EchoRequest& req = static_cast<const EchoRequest&>(context.GetReq());
    EchoResponse& rsp = static_cast<EchoResponse&>(context.GetRsp());

    APP_LOG_INFO(context.head.gid, "EchoSync recv, src(%u), req(%s)", context.head.src, req.content().c_str());

    rsp.set_content(req.content());
    context.ret_code = app::RPC_SUCCESS;
}

void EchoService::EchoCallPeer(app::RpcContext& context)
{
    const EchoRequest& req = static_cast<const EchoRequest&>(context.GetReq());
    EchoResponse& rsp = static_cast<EchoResponse&>(context.GetRsp());

    APP_LOG_INFO(context.head.gid, "EchoCallPeer recv, src(%u), calling peer(%u) with req(%s)", context.head.src,
                 EchoApp::GetInst().PeerID(), req.content().c_str());

    EchoRequest peer_req;
    peer_req.set_content(req.content());
    EchoResponse peer_rsp;

    uint32_t cmd = GetEchoMethodCmd("EchoSync");
    int32_t ret = app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, context.head.gid, cmd, peer_req, &peer_rsp, nullptr,
                                                  EchoApp::GetInst().PeerTBus2BusID(), 3000);

    context.ret_code = ret;
    if (ret == app::RPC_SUCCESS)
    {
        rsp.set_content(peer_rsp.content());
        APP_LOG_INFO(context.head.gid, "EchoCallPeer peer responded, rsp(%s)", peer_rsp.content().c_str());
    }
    else
    {
        APP_LOG_ERROR(context.head.gid, "EchoCallPeer peer rpc fail, ret(%d)", ret);
    }
}

}  // namespace echo_demo
