/*
 * * file name: echo_service.h
 * * description: echo_demo具体RPC方法实现
 * */

#ifndef _ECHO_SERVICE_H_
#define _ECHO_SERVICE_H_

#include "core/rpc_context.h"

namespace echo_demo
{
class EchoService
{
public:
    /// 直接把请求内容原样返回，验证最基础的request->handler->response链路
    static void EchoSync(app::RpcContext& context);

    /// 协程内再向对端发起一次EchoSync调用并等待结果，验证Pending/Awake+Yield/Resume全链路
    static void EchoCallPeer(app::RpcContext& context);
};

}  // namespace echo_demo

#endif
