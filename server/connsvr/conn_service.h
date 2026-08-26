/*
 * * file name: conn_service.h
 * * description: connsvr具体RPC方法实现
 */

#ifndef _CONN_SERVICE_H_
#define _CONN_SERVICE_H_

#include "core/rpc_context.h"

namespace connsvr
{
class ConnService
{
public:
    // ---- 登录/注册 ----
    static void Login(app::RpcContext& context);
    static void GuestLogin(app::RpcContext& context);
    static void SetUserInfo(app::RpcContext& context);

    // ---- 房间列表查询 ----
    static void RoomList(app::RpcContext& context);

    // ---- 房间操作（代理到roomsvr）----
    static void RoomCreate(app::RpcContext& context);
    static void RoomJoin(app::RpcContext& context);
    static void RoomLeave(app::RpcContext& context);
    static void RoomSetReady(app::RpcContext& context);
    static void RoomAddBot(app::RpcContext& context);
    static void RoomRemoveBot(app::RpcContext& context);
    static void RoomStartBattle(app::RpcContext& context);
    static void RoomSetRole(app::RpcContext& context);
    static void RoomSetMap(app::RpcContext& context);
    static void RoomSendEmote(app::RpcContext& context);
    static void RoomRename(app::RpcContext& context);

    // ---- 推送接收（roomsvr→connsvr→客户端）----
    static void OnPushRoomDetail(app::RpcContext& context);
    static void OnPushRoomList(app::RpcContext& context);
    static void OnPushBattleReady(app::RpcContext& context);
    static void OnPushRoomKicked(app::RpcContext& context);
    static void OnPushRoomSelecting(app::RpcContext& context);
    static void OnPushRoomBattleFailed(app::RpcContext& context);
    static void OnPushRoomEmote(app::RpcContext& context);
};

}  // namespace connsvr

#endif
