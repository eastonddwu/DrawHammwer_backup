/*
 * * file name: dsa_service.h
 * * description: dsagent具体RPC方法实现
 */

#ifndef _DSA_SERVICE_H_
#define _DSA_SERVICE_H_

#include "core/rpc_context.h"

namespace dsagent
{
class DsaService
{
public:
    /// 创建DS游戏进程
    static void CreateGame(app::RpcContext& context);
    /// 销毁DS进程
    static void DestroyDs(app::RpcContext& context);
    /// DS心跳上报
    static void DsHeartBeat(app::RpcContext& context);
    /// 设置/更新DS的玩家认证信息
    static void SetDsAuth(app::RpcContext& context);
    /// DS查询玩家信息代理：转发到dbproxy
    static void DsGetPlayerInfoProxy(app::RpcContext& context);
    /// DS结算代理：转发到roomsvr
    static void RoomDsPlayerSettleProxy(app::RpcContext& context);
    static void RoomDsGameFinishProxy(app::RpcContext& context);
};

}  // namespace dsagent

#endif
