/*
 * * file name: room_service.cpp
 * * description: RoomService各RPC handler实现
 *
 * 核心流程：
 * CreateRoom: 创建Waiting房间 → 推送Detail+List
 * JoinRoom: 加入房间 → 推送Detail+List
 * LeaveRoom: 离开房间 → 房主转移/销毁 → 推送Detail+List+Kicked
 * SetReady: 设置准备状态 → 推送Detail
 * StartBattle: 验证 → AllocDsa → CreateGame → 推送BattleReady+List
 * RenameRoom: 改名 → 推送Detail+List
 * NotifyDsStarted: DS就绪 → 生成token → SetDsAuth → PushBattleReady+List
 */

#include "room_service.h"
#include "common/clock.h"
#include "common/text_util.h"
#include "core/log.h"
#include "core/rpc_error.h"
#include "core/rpc_service.h"
#include "core/svr_type.h"
#include "core/transport_type.h"
#include "dsa_rpc_meta.h"
#include "dsc_rpc_meta.h"
#include "room.pb.h"
#include "room_error.h"
#include "room_mgr.h"
#include "room_rpc_meta.h"

#include <cstdlib>

namespace roomsvr
{
// 房间列表广播的最小间隔。房间列表是O(房间数 × 在线数)的全局广播，而触发它的动作
// （建房/加入/离开/准备/改名/切图/加减人机…）都是高频操作，短时间内多次触发得到的
// 快照内容几乎一样。合并到100ms一次：对"大厅房间列表"这种展示型数据延迟无感知，
// 但高并发下能把广播次数削减一个量级。
static constexpr uint64_t kRoomListPushIntervalMs = 100;

// 有房间列表变更待推送
static bool g_room_list_dirty = false;
// 上次真正发出房间列表广播的时间
static uint64_t g_last_room_list_push_ms = 0;

static uint64_t GenerateRandomToken()
{
    uint64_t hi = static_cast<uint64_t>(rand()) << 32;
    uint64_t lo = static_cast<uint64_t>(rand());
    return hi | lo | 1;  // 确保 non-zero
}

// ============================================================
// 推送辅助函数
// ============================================================

void RoomService::DoPushRoomDetail(const Room& room)
{
    PushRoomDetailNtf ntf;
    room.BuildDetailSnapshot(ntf);
    // 目标：房间内所有成员
    for (const auto& m : room.members())
    {
        if (!m.b_is_bot)
            ntf.add_target_gids(m.gid);
    }

    uint32_t push_cmd = GetRoomMethodCmd("PushRoomDetail");
    app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, push_cmd, ntf, nullptr, nullptr, app::kGroupAddrConnSvr,
                                   1000);
}

void RoomService::DoPushRoomList()
{
    // 只置脏，真正的发送在FlushPendingRoomList()里按间隔合并
    g_room_list_dirty = true;
}

bool RoomService::FlushPendingRoomList(uint64_t now_ms)
{
    if (!g_room_list_dirty)
        return false;

    // 时钟回拨或首次调用时不阻塞推送
    if (g_last_room_list_push_ms != 0 && now_ms >= g_last_room_list_push_ms &&
        now_ms - g_last_room_list_push_ms < kRoomListPushIntervalMs)
    {
        return false;
    }

    g_room_list_dirty = false;
    g_last_room_list_push_ms = now_ms;

    PushRoomListNtf ntf;
    RoomMgr::GetInst().BuildRoomListSnapshot(ntf);

    uint32_t push_cmd = GetRoomMethodCmd("PushRoomList");
    app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, push_cmd, ntf, nullptr, nullptr, app::kGroupAddrConnSvr,
                                   1000);
    return true;
}

void RoomService::DoPushKicked(const std::vector<uint64_t>& gids, uint64_t room_id, const std::string& reason)
{
    PushRoomKickedNtf ntf;
    ntf.set_room_id(room_id);
    ntf.set_reason(reason);
    for (uint64_t gid : gids)
        ntf.add_target_gids(gid);

    uint32_t push_cmd = GetRoomMethodCmd("PushRoomKicked");
    app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, push_cmd, ntf, nullptr, nullptr, app::kGroupAddrConnSvr,
                                   1000);
}

void RoomService::DoPushBattleFailed(const Room& room, int32_t reason, const std::string& message)
{
    PushRoomBattleFailedNtf ntf;
    ntf.set_room_id(room.room_id());
    ntf.set_reason(reason);
    ntf.set_message(message);
    for (const auto& m : room.members())
    {
        if (!m.b_is_bot)
            ntf.add_target_gids(m.gid);
    }

    uint32_t push_cmd = GetRoomMethodCmd("PushRoomBattleFailed");
    app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, push_cmd, ntf, nullptr, nullptr, app::kGroupAddrConnSvr,
                                   1000);
}

void RoomService::DoPushRoomEmote(const Room& room, uint64_t sender_gid, uint32_t emote_id)
{
    PushRoomEmoteNtf ntf;
    ntf.set_room_id(room.room_id());
    ntf.set_sender_gid(sender_gid);
    ntf.set_emote_id(emote_id);
    // 目标：房间内全部真人，含发送者（Bot无连接，不入列表）
    for (const auto& m : room.members())
    {
        if (!m.b_is_bot)
            ntf.add_target_gids(m.gid);
    }

    uint32_t push_cmd = GetRoomMethodCmd("PushRoomEmote");
    app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, push_cmd, ntf, nullptr, nullptr, app::kGroupAddrConnSvr,
                                   1000);
}

// ============================================================
// 房间操作 handler
// ============================================================

void RoomService::CreateRoom(app::RpcContext& context)
{
    const auto& req = static_cast<const CreateRoomReq&>(context.GetReq());
    uint64_t gid = req.gid();

    APP_LOG_INFO(gid, "CreateRoom recv, gid(%llu), room_name(%s), max_players(%u), map_id(%u)",
                 static_cast<unsigned long long>(gid), req.room_name().c_str(), req.max_players(), req.map_id());

    auto& rsp = static_cast<CreateRoomResp&>(context.GetRsp());
    if (req.max_players() > 8)
    {
        rsp.set_ret_code(kInvalidMaxPlayers);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    uint32_t max_players = req.max_players() == 0 ? 8 : req.max_players();
    uint32_t map_id = req.map_id() == 0 ? kDefaultMapId : req.map_id();
    if (!IsValidMapId(map_id))
    {
        rsp.set_ret_code(kInvalidMapId);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 房间名：空/纯空白 → 后台生成 "{创建者昵称}的房间"；非空 → trim后须1~11个Unicode码点
    std::string host_name = app::text::TrimAsciiWhitespace(req.display_name());
    std::string room_name = app::text::TrimAsciiWhitespace(req.room_name());
    if (room_name.empty())
    {
        if (host_name.empty())
        {
            // 昵称异常为空时拒建房：不生成"的房间"，也不用gid冒充昵称
            APP_LOG_WARN(gid, "CreateRoom reject, empty room_name and empty display_name");
            rsp.set_ret_code(kInvalidName);
            context.ret_code = app::RPC_SUCCESS;
            return;
        }
        // 昵称已在connsvr侧过8字闸门，默认名合计不超过11字；历史超长昵称只告警不拒绝
        room_name = host_name + app::text::kDefaultRoomNameSuffix;
        if (app::text::CountUtf8CodePoints(room_name) > app::text::kMaxRoomNameLen)
        {
            APP_LOG_WARN(gid, "default room_name oversized, display_name(\"%s\"), room_name(\"%s\")", host_name.c_str(),
                         room_name.c_str());
        }
    }
    else if (app::text::CountUtf8CodePoints(room_name) > app::text::kMaxRoomNameLen)
    {
        APP_LOG_INFO(gid, "CreateRoom reject, room_name too long(\"%s\")", room_name.c_str());
        rsp.set_ret_code(kInvalidName);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 创建Waiting房间（不分配DS）
    Room* room = RoomMgr::GetInst().AddRoom(gid, room_name, max_players, map_id, host_name);
    if (!room)
    {
        rsp.set_ret_code(kAlreadyInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 推送Detail给创建者 + List给在线玩家
    DoPushRoomDetail(*room);
    DoPushRoomList();

    rsp.set_ret_code(kOk);
    rsp.set_room_id(room->room_id());
    context.ret_code = app::RPC_SUCCESS;

    APP_LOG_INFO(gid, "CreateRoom ok, room_id(%llu)", static_cast<unsigned long long>(room->room_id()));
}

void RoomService::JoinRoom(app::RpcContext& context)
{
    const auto& req = static_cast<const JoinRoomReq&>(context.GetReq());
    uint64_t gid = req.gid();
    uint64_t room_id = req.room_id();

    APP_LOG_INFO(gid, "JoinRoom recv, gid(%llu), room_id(%llu)", static_cast<unsigned long long>(gid),
                 static_cast<unsigned long long>(room_id));

    auto& rsp = static_cast<JoinRoomResp&>(context.GetRsp());

    // 已在其他房间中
    if (RoomMgr::GetInst().GetGidRoom(gid) != 0)
    {
        rsp.set_ret_code(kAlreadyInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(kRoomNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 开战流程启动后冻结成员列表，仅WAITING允许加入
    if (room->state() == ROOM_STATE_DESTROYED)
    {
        rsp.set_ret_code(kRoomNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (room->state() != ROOM_STATE_WAITING)
    {
        rsp.set_ret_code(kRoomInBattle);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    auto now_ts = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    int ret = room->AddMember(gid, now_ts, false, req.display_name());
    if (ret == 1)
    {
        rsp.set_ret_code(kAlreadyInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    else if (ret == 2)
    {
        rsp.set_ret_code(kRoomFull);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    RoomMgr::GetInst().AddGidRoomMapping(gid, room_id);
    // 该gid可能带着上一局战斗中掉线留下的待离房标记，重新进房即视为已回归
    room->CancelPendingLeave(gid);

    // 推送Detail给房间内所有人 + List给在线玩家
    DoPushRoomDetail(*room);
    DoPushRoomList();

    rsp.set_ret_code(kOk);
    context.ret_code = app::RPC_SUCCESS;

    APP_LOG_INFO(gid, "JoinRoom ok, room_id(%llu), member_count(%u)", static_cast<unsigned long long>(room_id),
                 room->member_count());
}

void RoomService::QueryRoomList(app::RpcContext& context)
{
    APP_LOG_INFO(0, "QueryRoomList recv");

    auto& rsp = static_cast<QueryRoomListResp&>(context.GetRsp());
    rsp.set_ret_code(kOk);

    for (const auto& [id, room] : RoomMgr::GetInst().GetRooms())
    {
        if (room->state() == ROOM_STATE_DESTROYED)
            continue;
        auto* brief = rsp.add_rooms();
        room->BuildBriefInfo(*brief);
    }

    context.ret_code = app::RPC_SUCCESS;
    APP_LOG_INFO(0, "QueryRoomList ok, room_count(%d)", rsp.rooms_size());
}

void RoomService::QueryPlayerRoomState(app::RpcContext& context)
{
    const auto& req = static_cast<const QueryPlayerRoomStateReq&>(context.GetReq());
    uint64_t gid = req.gid();

    auto& rsp = static_cast<QueryPlayerRoomStateResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    rsp.set_in_room(false);
    rsp.set_room_id(0);
    rsp.set_in_battle(false);

    uint64_t room_id = RoomMgr::GetInst().GetGidRoom(gid);
    if (room_id != 0)
    {
        Room* room = RoomMgr::GetInst().GetRoom(room_id);
        if (room)
        {
            rsp.set_in_room(true);
            rsp.set_room_id(room_id);
            rsp.set_in_battle(room->in_battle());
        }
    }

    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::UpdateMemberName(app::RpcContext& context)
{
    const auto& req = static_cast<const UpdateMemberNameReq&>(context.GetReq());
    uint64_t gid = req.gid();

    auto& rsp = static_cast<UpdateMemberNameResp&>(context.GetRsp());
    rsp.set_ret_code(kOk);
    context.ret_code = app::RPC_SUCCESS;

    // 不在房间中：不算错误，直接返回（会话昵称已在connsvr更新）
    uint64_t room_id = RoomMgr::GetInst().GetGidRoom(gid);
    Room* room = room_id != 0 ? RoomMgr::GetInst().GetRoom(room_id) : nullptr;
    if (!room)
        return;

    if (!room->SetMemberDisplayName(gid, req.display_name()))
        return;

    APP_LOG_INFO(gid, "UpdateMemberName ok, room_id(%llu), display_name(\"%s\"), is_host(%d)",
                 static_cast<unsigned long long>(room_id), req.display_name().c_str(), room->IsHost(gid) ? 1 : 0);

    // 成员名进Detail(2001)，房主名进List(2000)的host_display_name；房间名不动
    DoPushRoomDetail(*room);
    DoPushRoomList();
}

void RoomService::LeaveRoom(app::RpcContext& context)
{
    const auto& req = static_cast<const LeaveRoomReq&>(context.GetReq());
    uint64_t gid = req.gid();
    uint64_t room_id = req.room_id();

    // room_id=0 时通过gid查找所在房间
    if (room_id == 0)
        room_id = RoomMgr::GetInst().GetGidRoom(gid);

    APP_LOG_INFO(gid, "LeaveRoom recv, gid(%llu), room_id(%llu)", static_cast<unsigned long long>(gid),
                 static_cast<unsigned long long>(room_id));

    auto& rsp = static_cast<LeaveRoomResp&>(context.GetRsp());

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(kRoomNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (!room->HasMember(gid))
    {
        rsp.set_ret_code(kNotInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // DS创建期间冻结成员列表，避免CreateGame快照与房间成员不一致。
    if (room->state() == ROOM_STATE_DS_CREATING || room->state() == ROOM_STATE_DS_READY)
    {
        rsp.set_ret_code(kRoomInBattle);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 战斗进行中：不立即移除成员，否则最后一名真人掉线会连房间带战绩一起销毁，
    // 且冻结名单/DS AuthMap 仍握着该gid。改为打待离房标记，战斗结束后统一补做离房。
    if (room->state() == ROOM_STATE_IN_BATTLE)
    {
        room->MarkPendingLeave(gid);
        // 成员保留在房间内(战绩/冻结名单需要)，但必须解除 gid→room 归属：
        // 否则该玩家重新登录后建房/进房会被判 kAlreadyInRoom，一直卡到战斗超时。
        RoomMgr::GetInst().RemoveGidRoomMapping(gid);
        APP_LOG_INFO(gid, "LeaveRoom deferred (in battle), room_id(%llu), gid(%llu), gid->room mapping released",
                     static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(gid));
        rsp.set_ret_code(kOk);
        rsp.set_room_destroyed(false);
        rsp.set_new_host_gid(0);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 移除成员
    room->RemoveMember(gid);
    RoomMgr::GetInst().RemoveGidRoomMapping(gid);

    bool room_destroyed = false;
    uint64_t new_host_gid = 0;

    if (room->real_player_count() == 0)
    {
        // 最后一名真人离开，销毁房间并清理Bot
        // 如果有DS进程，通知dsagent销毁
        if (room->dsa_svr_id() != 0)
        {
            DestroyDsReq destroy_req;
            destroy_req.set_room_id(room_id);
            destroy_req.set_reason(0);
            destroy_req.set_battle_generation(room->battle_generation());
            uint32_t destroy_cmd = dsagent::GetDsaMethodCmd("DestroyDs");
            app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, gid, destroy_cmd, destroy_req, nullptr, nullptr,
                                           room->dsa_svr_id(), 1000);
        }

        // 先推Kicked给离开者，再销毁（销毁后room指针失效）
        DoPushKicked({gid}, room_id, "room_destroyed");
        RoomMgr::GetInst().FreeRoom(room_id);
        room_destroyed = true;

        // 推送RoomList（房间消失）
        DoPushRoomList();
    }
    else
    {
        // 房主离开需要转移
        if (gid == room->host_gid())
        {
            new_host_gid = room->MigrateHost();
            APP_LOG_INFO(gid, "host left, migrated to new_host(%llu)", static_cast<unsigned long long>(new_host_gid));
        }

        // 推送Detail给剩余成员（包含新房主信息）
        DoPushRoomDetail(*room);
        // 推送List（人数变化）
        DoPushRoomList();
        // 推送Kicked给离开者
        DoPushKicked({gid}, room_id, "leave");
    }

    rsp.set_ret_code(kOk);
    rsp.set_room_destroyed(room_destroyed);
    rsp.set_new_host_gid(new_host_gid);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::SetReady(app::RpcContext& context)
{
    const auto& req = static_cast<const SetReadyReq&>(context.GetReq());
    uint64_t gid = req.gid();
    uint64_t room_id = req.room_id();

    if (room_id == 0)
        room_id = RoomMgr::GetInst().GetGidRoom(gid);

    APP_LOG_INFO(gid, "SetReady recv, gid(%llu), room_id(%llu), b_ready(%d)", static_cast<unsigned long long>(gid),
                 static_cast<unsigned long long>(room_id), req.b_ready());

    auto& rsp = static_cast<SetReadyResp&>(context.GetRsp());

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(kRoomNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (!room->HasMember(gid))
    {
        rsp.set_ret_code(kNotInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (room->state() != ROOM_STATE_WAITING)
    {
        rsp.set_ret_code(kRoomInBattle);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    room->SetReady(gid, req.b_ready());

    // 推送Detail
    DoPushRoomDetail(*room);

    rsp.set_ret_code(kOk);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::StartBattle(app::RpcContext& context)
{
    const auto& req = static_cast<const StartBattleReq&>(context.GetReq());
    uint64_t gid = req.gid();
    uint64_t room_id = req.room_id();

    if (room_id == 0)
        room_id = RoomMgr::GetInst().GetGidRoom(gid);

    APP_LOG_INFO(gid, "StartBattle recv, gid(%llu), room_id(%llu)", static_cast<unsigned long long>(gid),
                 static_cast<unsigned long long>(room_id));

    auto& rsp = static_cast<StartBattleResp&>(context.GetRsp());

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(kRoomNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 校验：仅房主
    if (!room->IsHost(gid))
    {
        rsp.set_ret_code(kNotHost);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 校验：不在战斗中 / 选人中 / DS分配中（仅 WAITING 可开战）
    if (room->in_battle() || room->state() != ROOM_STATE_WAITING)
    {
        rsp.set_ret_code(kAlreadyInBattle);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 校验：至少1名真人，且真人+Bot总人数 >= 2
    if (room->real_player_count() < 1 || room->member_count() < 2)
    {
        rsp.set_ret_code(kNotEnoughPlayers);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 校验：全员准备
    if (!room->AllReady())
    {
        rsp.set_ret_code(kNotAllReady);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 开战边界固化角色并立即冻结房间成员；主循环下一轮启动DS创建协程
    room->FinalizeBattleRoles();
    room->set_state(ROOM_STATE_DS_CREATING);
    uint64_t battle_generation = room->BeginBattleGeneration();
    RoomMgr::GetInst().EnqueueDsFlow(room_id, gid, battle_generation);

    rsp.set_ret_code(kOk);
    context.ret_code = app::RPC_SUCCESS;

    APP_LOG_INFO(gid, "StartBattle ok -> DS_CREATING, room_id(%llu)", static_cast<unsigned long long>(room_id));
}

void RoomService::AddBot(app::RpcContext& context)
{
    const auto& req = static_cast<const AddBotReq&>(context.GetReq());
    auto& rsp = static_cast<AddBotResp&>(context.GetRsp());
    uint64_t room_id = req.room_id();
    if (room_id == 0)
        room_id = RoomMgr::GetInst().GetGidRoom(req.gid());

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(kRoomNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (!room->HasMember(req.gid()))
    {
        rsp.set_ret_code(kNotInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (!room->IsHost(req.gid()))
    {
        rsp.set_ret_code(kNotHost);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (room->state() != ROOM_STATE_WAITING)
    {
        rsp.set_ret_code(kRoomInBattle);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    auto now_ts = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    std::string bot_id;
    uint32_t slot_index = 0;
    if (room->AddBot(now_ts, &bot_id, &slot_index) != 0)
    {
        rsp.set_ret_code(kRoomFull);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    rsp.set_ret_code(kOk);
    rsp.set_bot_id(bot_id);
    rsp.set_slot_index(slot_index);
    DoPushRoomDetail(*room);
    DoPushRoomList();
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::RemoveBot(app::RpcContext& context)
{
    const auto& req = static_cast<const RemoveBotReq&>(context.GetReq());
    auto& rsp = static_cast<RemoveBotResp&>(context.GetRsp());
    uint64_t room_id = req.room_id();
    if (room_id == 0)
        room_id = RoomMgr::GetInst().GetGidRoom(req.gid());

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(kRoomNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (!room->HasMember(req.gid()))
    {
        rsp.set_ret_code(kNotInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (!room->IsHost(req.gid()))
    {
        rsp.set_ret_code(kNotHost);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (room->state() != ROOM_STATE_WAITING)
    {
        rsp.set_ret_code(kRoomInBattle);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    const auto* target = req.slot_index() == 0 ? nullptr : room->GetMemberBySlot(req.slot_index());
    if (req.slot_index() != 0 &&
        (!target || !target->b_is_bot || (!req.bot_id().empty() && target->bot_id != req.bot_id())))
    {
        rsp.set_ret_code(kBotNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (req.slot_index() == 0 && req.bot_id().empty())
    {
        rsp.set_ret_code(kBotNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (!room->RemoveBot(req.slot_index(), req.bot_id()))
    {
        rsp.set_ret_code(kBotNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    rsp.set_ret_code(kOk);
    DoPushRoomDetail(*room);
    DoPushRoomList();
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::FailStartDsFlow(uint64_t room_id, uint64_t battle_generation, int32_t reason,
                                  const std::string& message)
{
    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room || room->state() != ROOM_STATE_DS_CREATING || room->battle_generation() != battle_generation)
        return;

    room->set_state(ROOM_STATE_WAITING);
    room->set_dsa_svr_id(0);
    DoPushBattleFailed(*room, reason, message);
    DoPushRoomDetail(*room);
    DoPushRoomList();
}

// StartBattle后立即执行：AllocDsa → CreateGame → PushBattleReady
void RoomService::StartDsFlow(uint64_t room_id, uint64_t host_gid, uint64_t battle_generation)
{
    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room || room->state() != ROOM_STATE_DS_CREATING || room->battle_generation() != battle_generation)
        return;

    // RPC会Yield，先复制开战成员快照，后续不跨Yield持有Room指针或成员引用。
    CreateGameReq create_req;
    create_req.set_room_id(room_id);
    create_req.set_ds_port(0);
    uint32_t map_id = room->map_id();
    if (!IsValidMapId(map_id))
    {
        APP_LOG_WARN(host_gid, "StartDsFlow invalid room map_id(%u), fallback to map_id(%u)", map_id, kDefaultMapId);
        map_id = kDefaultMapId;
    }
    create_req.set_map_id(map_id);
    create_req.set_battle_generation(battle_generation);
    for (const auto& m : room->members())
    {
        if (m.b_is_bot)
        {
            auto* bot = create_req.add_bots();
            bot->set_bot_id(m.bot_id);
            bot->set_battle_role_type(m.battle_role_type);
        }
        else
        {
            auto* player = create_req.add_players();
            player->set_gid(m.gid);
            player->set_battle_role_type(m.battle_role_type);
            player->set_display_name(m.display_name);
        }
    }

    AllocDsaReq alloc_req;
    alloc_req.set_room_id(room_id);
    AllocDsaResp alloc_rsp;
    uint32_t alloc_cmd = dscenter::GetDscMethodCmd("AllocDsa");
    int32_t alloc_ret = app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, host_gid, alloc_cmd, alloc_req,
                                                       &alloc_rsp, nullptr, app::kGroupAddrDsCenter, 2000);

    room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room || room->state() != ROOM_STATE_DS_CREATING || room->battle_generation() != battle_generation)
        return;
    if (alloc_ret != app::RPC_SUCCESS || alloc_rsp.ret_code() != 0 || alloc_rsp.dsa_svr_id() == 0)
    {
        APP_LOG_WARN(host_gid, "StartDsFlow AllocDsa fail, ret(%d), rsp_ret(%d)", alloc_ret, alloc_rsp.ret_code());
        FailStartDsFlow(room_id, battle_generation, kNoDsAvailable, "no ds available");
        return;
    }

    uint32_t dsa_svr_id = alloc_rsp.dsa_svr_id();
    room->set_dsa_svr_id(dsa_svr_id);

    CreateGameResp create_rsp;
    uint32_t create_cmd = dsagent::GetDsaMethodCmd("CreateGame");
    int32_t create_ret = app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, host_gid, create_cmd, create_req,
                                                        &create_rsp, nullptr, dsa_svr_id, 3000);

    room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
        return;
    // 异步NotifyDsStarted可能先于CreateGame响应完成并已推送2003。
    if (room->state() == ROOM_STATE_IN_BATTLE)
        return;
    if (room->state() != ROOM_STATE_DS_CREATING || room->battle_generation() != battle_generation)
        return;

    if (create_ret != app::RPC_SUCCESS || create_rsp.ret_code() != 0)
    {
        APP_LOG_WARN(host_gid, "StartDsFlow CreateGame fail, ret(%d), rsp_ret(%d)", create_ret, create_rsp.ret_code());
        DestroyDsReq destroy_req;
        destroy_req.set_room_id(room_id);
        destroy_req.set_reason(4);  // CreateGame失败/超时清理
        destroy_req.set_battle_generation(battle_generation);
        uint32_t destroy_cmd = dsagent::GetDsaMethodCmd("DestroyDs");
        app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, host_gid, destroy_cmd, destroy_req, nullptr, nullptr,
                                       dsa_svr_id, 1000);
        FailStartDsFlow(room_id, battle_generation, kNoDsAvailable, "create game failed");
        return;
    }

    for (const auto& auth : create_rsp.player_auth_list())
        room->SetPlayerToken(auth.gid(), auth.token());

    // dsagent同步返回连接信息时直接完成开战，否则等待NotifyDsStarted。
    if (create_rsp.has_ds_conn_info())
    {
        room->set_ds_conn_info(create_rsp.ds_conn_info());
        std::string server_address =
            create_rsp.ds_conn_info().ip() + ":" + std::to_string(create_rsp.ds_conn_info().port());
        room->SetInBattle(server_address, std::to_string(room_id));
        room->SetInBattleState();

        for (const auto& m : room->members())
        {
            if (m.b_is_bot)
                continue;
            PushBattleReadyNtf battle_ntf;
            battle_ntf.set_room_id(room_id);
            battle_ntf.set_server_address(server_address);
            battle_ntf.set_token(room->GetPlayerToken(m.gid));
            battle_ntf.set_battle_id(std::to_string(room_id));
            battle_ntf.add_target_gids(m.gid);

            uint32_t battle_push_cmd = GetRoomMethodCmd("PushBattleReady");
            app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, battle_push_cmd, battle_ntf, nullptr, nullptr,
                                           app::kGroupAddrConnSvr, 1000);
        }

        DoPushRoomDetail(*room);
        DoPushRoomList();
    }

    APP_LOG_INFO(host_gid, "StartDsFlow ok, room_id(%llu), ds_svr_id(0x%08X)", static_cast<unsigned long long>(room_id),
                 dsa_svr_id);
}

void RoomService::RoomSetRole(app::RpcContext& context)
{
    const auto& req = static_cast<const RoomSetRoleReq&>(context.GetReq());
    uint64_t gid = req.gid();
    uint64_t room_id = req.room_id();

    if (room_id == 0)
        room_id = RoomMgr::GetInst().GetGidRoom(gid);

    APP_LOG_INFO(gid, "RoomSetRole recv, gid(%llu), room_id(%llu), battle_role_type(%u)",
                 static_cast<unsigned long long>(gid), static_cast<unsigned long long>(room_id),
                 req.battle_role_type());

    auto& rsp = static_cast<RoomSetRoleResp&>(context.GetRsp());
    if (room_id == 0)
    {
        rsp.set_ret_code(kNotInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(kRoomNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (!room->HasMember(gid))
    {
        rsp.set_ret_code(kNotInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 仅WAITING允许选角，DS分配和战斗期间角色已锁定
    if (room->state() != ROOM_STATE_WAITING)
    {
        rsp.set_ret_code(kRoomInBattle);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    const RoomMemberData* member = room->GetMember(gid);
    if (!member || member->b_is_bot)
    {
        rsp.set_ret_code(kNotInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    // 房主默认已准备，仍允许在等待态调整角色；调整后保持准备状态。
    if (member->is_ready && gid != room->host_gid())
    {
        rsp.set_ret_code(kRoleLockedByReady);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 所有非零Catalog角色ID都合法，便于后续新增角色；0仅作为旧数据的开战兜底值
    if (req.battle_role_type() == 0)
    {
        rsp.set_ret_code(kInvalidRole);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    room->SetBattleRole(gid, req.battle_role_type());

    // 全房刷新可见选型
    DoPushRoomDetail(*room);

    rsp.set_ret_code(kOk);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::RoomSetMap(app::RpcContext& context)
{
    const auto& req = static_cast<const RoomSetMapReq&>(context.GetReq());
    uint64_t gid = req.gid();
    uint64_t room_id = req.room_id();
    if (room_id == 0)
        room_id = RoomMgr::GetInst().GetGidRoom(gid);

    auto& rsp = static_cast<RoomSetMapResp&>(context.GetRsp());
    if (room_id == 0)
    {
        rsp.set_ret_code(kNotInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(kRoomNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (!room->HasMember(gid))
    {
        rsp.set_ret_code(kNotInRoom);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (!room->IsHost(gid))
    {
        rsp.set_ret_code(kNotHost);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (room->state() != ROOM_STATE_WAITING)
    {
        rsp.set_ret_code(kRoomInBattle);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (!IsValidMapId(req.map_id()))
    {
        rsp.set_ret_code(kInvalidMapId);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (room->SetMap(req.map_id()))
    {
        // 改图清除其他真人的Ready，房主Ready保持不变以便继续选角。
        DoPushRoomDetail(*room);
        DoPushRoomList();
    }
    rsp.set_ret_code(kOk);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::RoomSendEmote(app::RpcContext& context)
{
    const auto& req = static_cast<const RoomSendEmoteReq&>(context.GetReq());
    uint64_t gid = req.gid();
    uint64_t room_id = req.room_id();
    if (room_id == 0)
        room_id = RoomMgr::GetInst().GetGidRoom(gid);

    auto& rsp = static_cast<RoomSendEmoteResp&>(context.GetRsp());
    context.ret_code = app::RPC_SUCCESS;

    // 校验1：有房间
    if (room_id == 0)
    {
        rsp.set_ret_code(kNotInRoom);
        return;
    }

    // 校验2：房间存在
    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(kRoomNotFound);
        return;
    }

    // 校验3：是该房真人成员。GetMember本身跳过Bot，且Bot的gid恒为0，
    // 所以误传bot gid在此返回nullptr被拦掉（R3）
    if (!room->GetMember(gid))
    {
        rsp.set_ret_code(kNotInRoom);
        return;
    }

    // 校验4：仅Waiting可发。DS_CREATING/DS_READY/IN_BATTLE 统一回1003，与RoomSetMap口径一致
    if (room->state() != ROOM_STATE_WAITING)
    {
        rsp.set_ret_code(kRoomInBattle);
        return;
    }

    // 校验5：emote_id白名单
    if (!IsValidEmoteId(req.emote_id()))
    {
        APP_LOG_WARN(gid, "RoomSendEmote reject, invalid emote_id(%u), room_id(%llu)", req.emote_id(),
                     static_cast<unsigned long long>(room_id));
        rsp.set_ret_code(kInvalidEmoteId);
        return;
    }

    // 不做服务端限频：客户端自己控制发送节奏（冷却/防连点是纯UI行为）
    APP_LOG_INFO(gid, "RoomSendEmote ok, gid(%llu), room_id(%llu), emote_id(%u)",
                 static_cast<unsigned long long>(gid), static_cast<unsigned long long>(room_id), req.emote_id());

    // 表情是瞬时事件：只广播，不进2001快照、不落库、不改房间任何持久态
    DoPushRoomEmote(*room, gid, req.emote_id());
    rsp.set_ret_code(kOk);
}

void RoomService::RenameRoom(app::RpcContext& context)
{
    const auto& req = static_cast<const RenameRoomReq&>(context.GetReq());
    uint64_t gid = req.gid();
    uint64_t room_id = req.room_id();

    if (room_id == 0)
        room_id = RoomMgr::GetInst().GetGidRoom(gid);

    APP_LOG_INFO(gid, "RenameRoom recv, gid(%llu), room_id(%llu), new_name(%s)", static_cast<unsigned long long>(gid),
                 static_cast<unsigned long long>(room_id), req.new_name().c_str());

    auto& rsp = static_cast<RenameRoomResp&>(context.GetRsp());

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(kRoomNotFound);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (!room->IsHost(gid))
    {
        rsp.set_ret_code(kNotHost);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 改名：trim后须1~11个Unicode码点；空串按失败处理，不恢复默认名
    std::string new_name;
    if (!app::text::ValidateLength(req.new_name(), app::text::kMaxRoomNameLen, &new_name))
    {
        APP_LOG_INFO(gid, "RenameRoom reject, invalid new_name(\"%s\")", req.new_name().c_str());
        rsp.set_ret_code(kInvalidName);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    room->set_room_name(new_name);

    // 推送Detail + List
    DoPushRoomDetail(*room);
    DoPushRoomList();

    rsp.set_ret_code(kOk);
    context.ret_code = app::RPC_SUCCESS;
}

// ============================================================
// 内部：dsagent→roomsvr
// ============================================================

void RoomService::NotifyDsStarted(app::RpcContext& context)
{
    const auto& req = static_cast<const NotifyDsStartedReq&>(context.GetReq());
    uint64_t room_id = req.room_id();

    APP_LOG_INFO(0, "NotifyDsStarted recv, room_id(%llu), auth_count(%d)", static_cast<unsigned long long>(room_id),
                 req.player_auth_list_size());

    auto& rsp = static_cast<NotifyDsStartedResp&>(context.GetRsp());

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(1);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (req.battle_generation() != room->battle_generation())
    {
        APP_LOG_WARN(0, "NotifyDsStarted generation mismatch, room_id(%llu), request(%llu), current(%llu)",
                     static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(req.battle_generation()),
                     static_cast<unsigned long long>(room->battle_generation()));
        rsp.set_ret_code(2);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    // 同步CreateGame响应可能已完成开战；同代次通知不得重复推送。
    if (room->state() == ROOM_STATE_IN_BATTLE)
    {
        APP_LOG_INFO(0, "NotifyDsStarted: room already IN_BATTLE, skip duplicate push, room_id(%llu)",
                     static_cast<unsigned long long>(room_id));
        rsp.set_ret_code(0);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (room->state() != ROOM_STATE_DS_CREATING)
    {
        APP_LOG_WARN(0, "NotifyDsStarted ignored, room_id(%llu), state(%u)", static_cast<unsigned long long>(room_id),
                     room->state());
        rsp.set_ret_code(2);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 更新房间状态和DS连接信息
    room->set_state(ROOM_STATE_IN_BATTLE);
    room->set_ds_conn_info(req.ds_conn_info());

    // 构造 server_address
    std::string server_address = req.ds_conn_info().ip() + ":" + std::to_string(req.ds_conn_info().port());
    room->SetInBattle(server_address, std::to_string(room_id));  // battle_id = room_id
    room->SetInBattleState();

    // 存储dsagent返回的player auth信息
    for (const auto& auth : req.player_auth_list())
        room->SetPlayerToken(auth.gid(), auth.token());

    // 为尚无token的玩家生成token
    std::vector<std::pair<uint64_t, uint64_t>> new_tokens;
    for (const auto& m : room->members())
    {
        if (m.b_is_bot)
            continue;
        if (!room->HasPlayerToken(m.gid))
        {
            uint64_t token = GenerateRandomToken();
            room->SetPlayerToken(m.gid, token);
            new_tokens.emplace_back(m.gid, token);
            APP_LOG_INFO(m.gid, "NotifyDsStarted: generated token for gid(%llu), token(%llu)",
                         static_cast<unsigned long long>(m.gid), static_cast<unsigned long long>(token));
        }
    }

    // 如果有新生成的token，发送SetDsAuth给dsagent
    if (!new_tokens.empty())
    {
        roomsvr::SetDsAuthReq auth_req;
        auth_req.set_room_id(room_id);
        for (const auto& m : room->members())
        {
            if (m.b_is_bot)
                continue;
            auto* auth = auth_req.add_player_auth_list();
            auth->set_gid(m.gid);
            auth->set_token(room->GetPlayerToken(m.gid));
        }
        uint32_t auth_cmd = dsagent::GetDsaMethodCmd("SetDsAuth");
        app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, auth_cmd, auth_req, nullptr, nullptr,
                                       room->dsa_svr_id(), 2000);
    }

    // 推送BattleReady到connsvr（每人独立token）
    for (const auto& m : room->members())
    {
        if (m.b_is_bot)
            continue;
        uint64_t token = room->GetPlayerToken(m.gid);
        PushBattleReadyNtf battle_ntf;
        battle_ntf.set_room_id(room_id);
        battle_ntf.set_server_address(server_address);
        battle_ntf.set_token(token);
        battle_ntf.set_battle_id(std::to_string(room_id));
        battle_ntf.add_target_gids(m.gid);

        uint32_t battle_push_cmd = GetRoomMethodCmd("PushBattleReady");
        app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, battle_push_cmd, battle_ntf, nullptr, nullptr,
                                       app::kGroupAddrConnSvr, 1000);
    }

    // 推送RoomList（in_battle=true）
    DoPushRoomList();

    // 推送RoomDetail（in_battle=true + battle_server_address）
    DoPushRoomDetail(*room);

    APP_LOG_INFO(0, "room DS ready, room_id(%llu), ds_ip(%s), ds_port(%u), member_count(%zu)",
                 static_cast<unsigned long long>(room_id), req.ds_conn_info().ip().c_str(), req.ds_conn_info().port(),
                 room->members().size());

    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::NotifyDsTimeout(app::RpcContext& context)
{
    const auto& req = static_cast<const NotifyDsTimeoutReq&>(context.GetReq());
    uint64_t room_id = req.room_id();

    APP_LOG_WARN(0, "NotifyDsTimeout recv, room_id(%llu), pid(%u)", static_cast<unsigned long long>(room_id),
                 req.pid());

    auto& rsp = static_cast<NotifyDsTimeoutResp&>(context.GetRsp());

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(1);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (req.battle_generation() != room->battle_generation())
    {
        APP_LOG_WARN(0, "NotifyDsTimeout generation mismatch, room_id(%llu), request(%llu), current(%llu)",
                     static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(req.battle_generation()),
                     static_cast<unsigned long long>(room->battle_generation()));
        rsp.set_ret_code(2);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (room->state() == ROOM_STATE_WAITING)
    {
        APP_LOG_INFO(0, "NotifyDsTimeout ignored for settled room, room_id(%llu), generation(%llu)",
                     static_cast<unsigned long long>(room_id),
                     static_cast<unsigned long long>(req.battle_generation()));
        rsp.set_ret_code(0);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (room->state() == ROOM_STATE_DS_CREATING)
    {
        FailStartDsFlow(room_id, req.battle_generation(), kNoDsAvailable, "ds exited during startup");
        rsp.set_ret_code(0);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 如果房间在战斗中，做保底结算
    if (room->in_battle() && (room->state() == ROOM_STATE_IN_BATTLE || room->state() == ROOM_STATE_DS_READY))
    {
        DoGuaranteedSettle(*room, 4);  // end_reason=4: DS_crash
        // 战斗中掉线的玩家在此统一离房（可能销毁房间，之后不可再用room指针）
        if (!ApplyPendingLeaves(room_id))
        {
            room = RoomMgr::GetInst().GetRoom(room_id);
            if (room)
            {
                DoPushRoomDetail(*room);
                DoPushRoomList();
            }
        }
    }
    else
    {
        // 房间不在战斗中 — 原有行为：销毁房间
        std::vector<uint64_t> kicked_gids;
        for (const auto& m : room->members())
        {
            if (!m.b_is_bot)
                kicked_gids.push_back(m.gid);
        }

        room->set_state(ROOM_STATE_DESTROYED);
        RoomMgr::GetInst().FreeRoom(room_id);

        DoPushKicked(kicked_gids, room_id, "ds_timeout");
        DoPushRoomList();
    }

    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

// ============================================================
// DS结算 handler（DS→dsagent→roomsvr）
// ============================================================

void RoomService::RoomDsPlayerSettle(app::RpcContext& context)
{
    const auto& req = static_cast<const RoomDsPlayerSettleReq&>(context.GetReq());
    uint64_t room_id = req.room_id();
    const FightPlayerInfo& info = req.player_info();
    uint64_t gid = info.gid();

    APP_LOG_INFO(gid,
                 "RoomDsPlayerSettle recv, room_id(%llu), generation(%llu), gid(%llu), is_bot(%d), bot_id(%s), "
                 "kills(%u), deaths(%u), rank(%u)",
                 static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(req.battle_generation()),
                 static_cast<unsigned long long>(gid), info.b_is_bot(), info.bot_id().c_str(), info.kills(),
                 info.deaths(), info.rank());

    auto& rsp = static_cast<RoomDsPlayerSettleResp&>(context.GetRsp());

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(1);
        rsp.set_gid(gid);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (room->dsa_svr_id() != 0 && context.head.src != room->dsa_svr_id())
    {
        APP_LOG_WARN(gid, "RoomDsPlayerSettle source mismatch, room_id(%llu), source(0x%08X), dsa(0x%08X)",
                     static_cast<unsigned long long>(room_id), context.head.src, room->dsa_svr_id());
        rsp.set_ret_code(4);
        rsp.set_gid(gid);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (room->state() != ROOM_STATE_IN_BATTLE && room->state() != ROOM_STATE_DS_READY)
    {
        APP_LOG_WARN(gid, "RoomDsPlayerSettle: room not in battle, state(%u)", room->state());
        rsp.set_ret_code(2);
        rsp.set_gid(gid);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (req.battle_generation() == 0 || req.battle_generation() != room->battle_generation())
    {
        APP_LOG_WARN(gid, "RoomDsPlayerSettle generation mismatch, room_id(%llu), request(%llu), current(%llu)",
                     static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(req.battle_generation()),
                     static_cast<unsigned long long>(room->battle_generation()));
        rsp.set_ret_code(3);
        rsp.set_gid(gid);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (info.b_is_bot())
    {
        if (gid != 0 || info.bot_id().empty() || !room->GetBattleBot(info.bot_id()))
        {
            APP_LOG_WARN(gid, "RoomDsPlayerSettle invalid bot identity, room_id(%llu), gid(%llu), bot_id(%s)",
                         static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(gid),
                         info.bot_id().c_str());
            rsp.set_ret_code(kNotInRoom);
            rsp.set_gid(gid);
            context.ret_code = app::RPC_SUCCESS;
            return;
        }
        room->StoreBotSettleData(info.bot_id(), info);
    }
    else
    {
        if (gid == 0 || !info.bot_id().empty() || !room->GetBattleMember(gid))
        {
            APP_LOG_WARN(gid, "RoomDsPlayerSettle invalid real-player identity, room_id(%llu), gid(%llu), bot_id(%s)",
                         static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(gid),
                         info.bot_id().c_str());
            rsp.set_ret_code(kNotInRoom);
            rsp.set_gid(gid);
            context.ret_code = app::RPC_SUCCESS;
            return;
        }
        room->StoreSettleData(gid, info);
    }

    rsp.set_ret_code(0);
    rsp.set_gid(gid);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::RoomDsGameFinish(app::RpcContext& context)
{
    const auto& req = static_cast<const RoomDsGameFinishReq&>(context.GetReq());
    uint64_t room_id = req.room_id();

    APP_LOG_INFO(0, "RoomDsGameFinish recv, room_id(%llu), generation(%llu), duration_sec(%u), end_reason(%u)",
                 static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(req.battle_generation()),
                 req.duration_sec(), req.end_reason());

    auto& rsp = static_cast<RoomDsGameFinishResp&>(context.GetRsp());

    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
    {
        rsp.set_ret_code(1);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (room->dsa_svr_id() != 0 && context.head.src != room->dsa_svr_id())
    {
        APP_LOG_WARN(0, "RoomDsGameFinish source mismatch, room_id(%llu), source(0x%08X), dsa(0x%08X)",
                     static_cast<unsigned long long>(room_id), context.head.src, room->dsa_svr_id());
        rsp.set_ret_code(4);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (req.battle_generation() == 0 || req.battle_generation() != room->battle_generation())
    {
        APP_LOG_WARN(0, "RoomDsGameFinish generation mismatch, room_id(%llu), request(%llu), current(%llu)",
                     static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(req.battle_generation()),
                     static_cast<unsigned long long>(room->battle_generation()));
        rsp.set_ret_code(3);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (room->state() != ROOM_STATE_IN_BATTLE && room->state() != ROOM_STATE_DS_READY)
    {
        APP_LOG_WARN(0, "RoomDsGameFinish: room not in battle, state(%u), room_id(%llu)", room->state(),
                     static_cast<unsigned long long>(room_id));
        rsp.set_ret_code(2);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    room->SetLastMatch(room->BuildMatchSummary(req.duration_sec(), req.end_reason()));

    // Save DS identity before ClearBattleState() resets runtime fields.
    uint32_t saved_dsa_svr_id = room->dsa_svr_id();
    uint64_t saved_battle_generation = room->battle_generation();

    room->ClearBattleState();

    // 战斗中掉线的玩家在此统一离房（可能导致房间销毁，之后不可再用room指针）
    bool destroyed = ApplyPendingLeaves(room_id);

    // Push to clients
    if (!destroyed)
    {
        room = RoomMgr::GetInst().GetRoom(room_id);
        if (room)
        {
            DoPushRoomDetail(*room);
            DoPushRoomList();
        }
    }

    // Destroy DS
    if (saved_dsa_svr_id != 0)
    {
        roomsvr::DestroyDsReq destroy_req;
        destroy_req.set_room_id(room_id);
        destroy_req.set_reason(2);  // reason=2: battle finished
        destroy_req.set_battle_generation(saved_battle_generation);
        uint32_t destroy_cmd = dsagent::GetDsaMethodCmd("DestroyDs");
        app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, destroy_cmd, destroy_req, nullptr, nullptr,
                                       saved_dsa_svr_id, 1000);
    }

    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
    APP_LOG_INFO(0, "RoomDsGameFinish ok, room_id(%llu) -> WAITING, last_match stored",
                 static_cast<unsigned long long>(room_id));
}

bool RoomService::ApplyPendingLeaves(uint64_t room_id)
{
    Room* room = RoomMgr::GetInst().GetRoom(room_id);
    if (!room)
        return true;

    std::vector<uint64_t> gids = room->TakePendingLeaves();
    if (gids.empty())
        return false;

    // 仅处理仍在房间内的成员（可能已被其它路径移除）
    std::vector<uint64_t> left_gids;
    // 待推Kicked的目标：已在别的房间的玩家必须排除，否则会把他们从新房间里踢出去
    std::vector<uint64_t> notify_gids;
    for (uint64_t gid : gids)
    {
        if (!room->HasMember(gid))
            continue;
        room->RemoveMember(gid);
        // 该玩家可能已重登并加入了别的房间，此时映射指向新房间，不能误删也不能推Kicked
        uint64_t cur_room_id = RoomMgr::GetInst().GetGidRoom(gid);
        if (cur_room_id == room_id)
            RoomMgr::GetInst().RemoveGidRoomMapping(gid);
        if (cur_room_id == 0 || cur_room_id == room_id)
            notify_gids.push_back(gid);
        else
            APP_LOG_INFO(gid,
                         "pending leave applied but gid already in another room, skip kicked push, "
                         "old_room_id(%llu), cur_room_id(%llu), gid(%llu)",
                         static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(cur_room_id),
                         static_cast<unsigned long long>(gid));
        left_gids.push_back(gid);
        APP_LOG_INFO(gid, "pending leave applied after battle, room_id(%llu), gid(%llu)",
                     static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(gid));
    }
    if (left_gids.empty())
        return false;

    // 全部真人都已离开：销毁房间（含DS与Bot清理），与LeaveRoom末态保持一致
    if (room->real_player_count() == 0)
    {
        if (room->dsa_svr_id() != 0)
        {
            DestroyDsReq destroy_req;
            destroy_req.set_room_id(room_id);
            destroy_req.set_reason(0);
            destroy_req.set_battle_generation(room->battle_generation());
            uint32_t destroy_cmd = dsagent::GetDsaMethodCmd("DestroyDs");
            app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, destroy_cmd, destroy_req, nullptr, nullptr,
                                           room->dsa_svr_id(), 1000);
        }
        if (!notify_gids.empty())
            DoPushKicked(notify_gids, room_id, "room_destroyed");
        RoomMgr::GetInst().FreeRoom(room_id);
        DoPushRoomList();
        APP_LOG_INFO(0, "room destroyed after applying pending leaves, room_id(%llu)",
                     static_cast<unsigned long long>(room_id));
        return true;
    }

    // 还有真人留下：必要时转移房主，然后同步房间状态
    if (!room->HasMember(room->host_gid()))
    {
        uint64_t new_host = room->MigrateHost();
        APP_LOG_INFO(0, "host left during battle, room_id(%llu), migrated to new_host(%llu)",
                     static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(new_host));
    }
    if (!notify_gids.empty())
        DoPushKicked(notify_gids, room_id, "leave");
    DoPushRoomDetail(*room);
    DoPushRoomList();
    return false;
}

void RoomService::DoGuaranteedSettle(Room& room, uint32_t end_reason)
{
    uint32_t duration_sec = 0;
    if (room.battle_start_time_ms() > 0)
    {
        uint64_t now_ms = app::Clock::GetInst().CurrentMilliSec();
        duration_sec = static_cast<uint32_t>((now_ms - room.battle_start_time_ms()) / 1000);
    }

    room.SetLastMatch(room.BuildMatchSummary(duration_sec, end_reason));
    room.ClearBattleState();
}

// ============================================================
// 推送空实现（roomsvr是发送方）
// ============================================================

void RoomService::OnPushRoomDetail(app::RpcContext& context)
{
    auto& rsp = static_cast<PushRoomDetailNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::OnPushRoomList(app::RpcContext& context)
{
    auto& rsp = static_cast<PushRoomListNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::OnPushBattleReady(app::RpcContext& context)
{
    auto& rsp = static_cast<PushBattleReadyNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::OnPushRoomKicked(app::RpcContext& context)
{
    auto& rsp = static_cast<PushRoomKickedNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::OnPushRoomSelecting(app::RpcContext& context)
{
    auto& rsp = static_cast<PushRoomSelectingNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::OnPushRoomBattleFailed(app::RpcContext& context)
{
    auto& rsp = static_cast<PushRoomBattleFailedNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void RoomService::OnPushRoomEmote(app::RpcContext& context)
{
    auto& rsp = static_cast<PushRoomEmoteNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

}  // namespace roomsvr
