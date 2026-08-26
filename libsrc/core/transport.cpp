/*
 * * file name: transport.cpp
 * * description: ...
 * */

#include "transport.h"
#include <cassert>
#include "interface/channel_interface.h"
#include "interface/codec_interface.h"
#include "interface/routing_interface.h"
#include "rpc_error.h"
#include "svr_type.h"

namespace app
{
int32_t TransportInfo::Send(uint32_t dst) const
{
    assert(channel);
    assert(send_codec);

    // 框架层路由：如果routing可用，用gid哈希选路覆盖dst
    if (routing && dst != 0)
    {
        uint32_t svr_type = SvrTypeFromBusid(dst);
        uint32_t routed_dst = routing->GetSendDest(svr_type, send_codec->GetGid(), dst);
        if (routed_dst != 0)
            dst = routed_dst;
        // routed_dst==0表示路由表为空，降级使用原始dst
    }

    uint32_t send_len = 0;
    const char* send_data = send_codec->Encode(send_len);
    if (!send_data)
        return RPC_SYS_ERR;

    int32_t ret = channel->Send(dst, send_data, send_len);
    if (ret != 0)
        return RPC_CHANNEL_SEND_ERR;

    return RPC_SUCCESS;
}

}  // namespace app
