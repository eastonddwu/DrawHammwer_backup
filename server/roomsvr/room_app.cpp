/*
 * * file name: room_app.cpp
 * * description: RoomApp::Setup/OnInit/OnTick实现，见room_app.h说明
 * */

#include "room_app.h"
#include "core/coro_mgr.h"
#include "core/log.h"
#include "core/rpc_service.h"
#include "core/svr_type.h"
#include "core/transport_type.h"
#include "dsa_rpc_meta.h"
#include "room.pb.h"
#include "room_error.h"
#include "room_mgr.h"
#include "room_rpc_meta.h"
#include "room_service.h"
#include "svr_base/default_init.h"

namespace roomsvr
{
void RoomApp::Setup(const std::string& tbus2_agent_url)
{
    tbus2_agent_url_ = tbus2_agent_url;
}

bool RoomApp::OnInit()
{
    if (!UseDefaultInit(*this, MySvrID(), tbus2_agent_url_))
        return false;

    // ---- 房间操作 handler ----
    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("CreateRoom"),
            {RoomService::CreateRoom, &CreateRoomReq::default_instance(), &CreateRoomResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register CreateRoom fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("JoinRoom"),
            {RoomService::JoinRoom, &JoinRoomReq::default_instance(), &JoinRoomResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register JoinRoom fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("LeaveRoom"),
            {RoomService::LeaveRoom, &LeaveRoomReq::default_instance(), &LeaveRoomResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register LeaveRoom fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("SetReady"),
            {RoomService::SetReady, &SetReadyReq::default_instance(), &SetReadyResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register SetReady fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("StartBattle"),
            {RoomService::StartBattle, &StartBattleReq::default_instance(), &StartBattleResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register StartBattle fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("RoomSetRole"),
            {RoomService::RoomSetRole, &RoomSetRoleReq::default_instance(), &RoomSetRoleResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomSetRole fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("RoomSetMap"),
            {RoomService::RoomSetMap, &RoomSetMapReq::default_instance(), &RoomSetMapResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomSetMap fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("AddBot"),
            {RoomService::AddBot, &AddBotReq::default_instance(), &AddBotResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register AddBot fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("RemoveBot"),
            {RoomService::RemoveBot, &RemoveBotReq::default_instance(), &RemoveBotResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RemoveBot fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("RenameRoom"),
            {RoomService::RenameRoom, &RenameRoomReq::default_instance(), &RenameRoomResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RenameRoom fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(GetRoomMethodCmd("QueryRoomList"),
                                                   {RoomService::QueryRoomList, &QueryRoomListReq::default_instance(),
                                                    &QueryRoomListResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register QueryRoomList fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("QueryPlayerRoomState"),
            {RoomService::QueryPlayerRoomState, &QueryPlayerRoomStateReq::default_instance(),
             &QueryPlayerRoomStateResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register QueryPlayerRoomState fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("UpdateMemberName"),
            {RoomService::UpdateMemberName, &UpdateMemberNameReq::default_instance(),
             &UpdateMemberNameResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register UpdateMemberName fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(GetRoomMethodCmd("RoomSendEmote"),
                                                  {RoomService::RoomSendEmote, &RoomSendEmoteReq::default_instance(),
                                                   &RoomSendEmoteResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomSendEmote fail");
        return false;
    }

    // ---- 内部 handler（dsagent→roomsvr）----
    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("NotifyDsStarted"), {RoomService::NotifyDsStarted, &NotifyDsStartedReq::default_instance(),
                                                  &NotifyDsStartedResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register NotifyDsStarted fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("NotifyDsTimeout"), {RoomService::NotifyDsTimeout, &NotifyDsTimeoutReq::default_instance(),
                                                  &NotifyDsTimeoutResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register NotifyDsTimeout fail");
        return false;
    }

    // ---- DS结算 handler ----
    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("RoomDsPlayerSettle"),
            {RoomService::RoomDsPlayerSettle, &RoomDsPlayerSettleReq::default_instance(),
             &RoomDsPlayerSettleResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomDsPlayerSettle fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("RoomDsGameFinish"),
            {RoomService::RoomDsGameFinish, &RoomDsGameFinishReq::default_instance(),
             &RoomDsGameFinishResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomDsGameFinish fail");
        return false;
    }

    // ---- 推送 handler（roomsvr→connsvr，发送方空实现）----
    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("PushRoomDetail"), {RoomService::OnPushRoomDetail, &PushRoomDetailNtf::default_instance(),
                                                 &PushRoomDetailNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushRoomDetail fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(GetRoomMethodCmd("PushRoomList"),
                                                   {RoomService::OnPushRoomList, &PushRoomListNtf::default_instance(),
                                                    &PushRoomListNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushRoomList fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("PushBattleReady"),
            {RoomService::OnPushBattleReady, &PushBattleReadyNtf::default_instance(),
             &PushBattleReadyNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushBattleReady fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("PushRoomKicked"), {RoomService::OnPushRoomKicked, &PushRoomKickedNtf::default_instance(),
                                                 &PushRoomKickedNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushRoomKicked fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetRoomMethodCmd("PushRoomEmote"), {RoomService::OnPushRoomEmote, &PushRoomEmoteNtf::default_instance(),
                                                &PushRoomEmoteNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushRoomEmote fail");
        return false;
    }

    APP_LOG_INFO(0, "RoomApp init ok, svr_id(%u), busid(%u), agent_url(%s)", MySvrID(), MySvrID(),
                 tbus2_agent_url_.c_str());
    return true;
}

size_t RoomApp::OnProc(uint64_t now_ms, bool /*stop*/)
{
    // RPC handler本身运行在协程中，不能嵌套Spawn；StartBattle先入队，由下一轮主循环立即启动DS流程。
    auto pending_flows = RoomMgr::GetInst().TakePendingDsFlows();
    for (const auto& flow : pending_flows)
    {
        bool spawned = app::CoroMgr::GetInst().Spawn(
            [flow]() { RoomService::StartDsFlow(flow.room_id, flow.host_gid, flow.battle_generation); });
        if (!spawned)
        {
            APP_LOG_ERROR(flow.host_gid, "spawn StartDsFlow fail, room_id(%llu)",
                          static_cast<unsigned long long>(flow.room_id));
            RoomService::FailStartDsFlow(flow.room_id, flow.battle_generation, kInternalError,
                                         "failed to start ds flow");
        }
    }

    // 合并发送积压的房间列表广播。这里是主协程，但PushRoomList是rsp=nullptr的
    // fire-and-forget调用，不会走Pending()/Yield()，因此无需协程上下文。
    size_t pushed = RoomService::FlushPendingRoomList(now_ms) ? 1 : 0;

    return pending_flows.size() + pushed;
}

void RoomApp::OnTick(uint64_t now_ms, uint64_t /*tick_count*/)
{
    auto timeout_infos = RoomMgr::GetInst().OnTick(now_ms);
    for (const auto& info : timeout_infos)
    {
        if (info.is_battle_timeout)
        {
            // 战斗超时：保底结算，不销毁房间
            Room* room = RoomMgr::GetInst().GetRoom(info.room_id);
            if (!room)
                continue;

            RoomService::DoGuaranteedSettle(*room, 5);  // end_reason=5: battle_timeout
            // 战斗中掉线的玩家在此统一离房（可能销毁房间，之后不可再用room指针）
            if (!RoomService::ApplyPendingLeaves(info.room_id))
            {
                room = RoomMgr::GetInst().GetRoom(info.room_id);
                if (room)
                {
                    RoomService::DoPushRoomDetail(*room);
                    RoomService::DoPushRoomList();
                }
            }

            // 销毁DS进程
            if (info.dsa_svr_id != 0)
            {
                roomsvr::DestroyDsReq destroy_req;
                destroy_req.set_room_id(info.room_id);
                destroy_req.set_reason(3);  // reason=3: battle timeout
                destroy_req.set_battle_generation(info.battle_generation);
                uint32_t destroy_cmd = dsagent::GetDsaMethodCmd("DestroyDs");
                app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, destroy_cmd, destroy_req, nullptr, nullptr,
                                               info.dsa_svr_id, 1000);
            }
        }
        else
        {
            // 空房间超时：原有行为（房间已被FreeRoom释放）
            if (info.dsa_svr_id != 0)
            {
                roomsvr::DestroyDsReq destroy_req;
                destroy_req.set_room_id(info.room_id);
                destroy_req.set_reason(1);
                destroy_req.set_battle_generation(info.battle_generation);
                uint32_t destroy_cmd = dsagent::GetDsaMethodCmd("DestroyDs");
                app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, destroy_cmd, destroy_req, nullptr, nullptr,
                                               info.dsa_svr_id, 1000);
            }
        }
    }
}

}  // namespace roomsvr
