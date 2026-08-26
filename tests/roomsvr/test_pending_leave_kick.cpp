/*
 * * file name: test_pending_leave_kick.cpp
 * * description: 回归测试 —— 战斗中标记待离房的玩家，若在结算前已加入新房间，
 * *              ApplyPendingLeaves() 不得给他推 PushRoomKicked（否则会把他从新房间踢回大厅）。
 * *
 * * 复现的线上问题（2026-08-20 room 56/57）：
 * *   20:09:38 gid A/B 在 room 56 战斗中点退出 -> LeaveRoom deferred（只打标记）
 * *   20:10:37 gid A 建了 room 57，A/B 都进了新房间并开打
 * *   20:12:57 room 56 的 DS 才结算 -> ApplyPendingLeaves 把 A/B 一起 PushKicked
 * *            A/B 当时正在 room 57 局内，却收到 room 56 的踢出通知 -> 全员被弹回大厅
 */

#include "room_service.h"

#include <cstdio>
#include <string>
#include <vector>

#include "core/interface/channel_interface.h"
#include "core/rpc_service.h"
#include "core/transport_type.h"
#include "net/pb_codec.h"
#include "room.pb.h"
#include "room_mgr.h"
#include "room_rpc_meta.h"

namespace
{

#define CHECK(condition, message)                                            \
    do                                                                       \
    {                                                                        \
        if (!(condition))                                                    \
        {                                                                    \
            std::fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
            return 1;                                                        \
        }                                                                    \
    } while (false)

/// 捕获 roomsvr 发出的推送帧，供断言检查「谁收到了 Kicked」。
class CapturingChannel : public app::IChannel
{
public:
    struct KickedPush
    {
        uint64_t room_id;
        std::string reason;
        std::vector<uint64_t> target_gids;
    };

    uint32_t MyID() const override { return 0x06010001; }
    size_t Loop(uint32_t) override { return 0; }

    int32_t Send(uint32_t, const char* buff, size_t buff_len) override
    {
        app::PbRecvCodec codec;
        if (codec.Decode(buff, static_cast<uint32_t>(buff_len)) <= 0)
            return 0;

        if (codec.GetCmd() != roomsvr::GetRoomMethodCmd("PushRoomKicked"))
            return 0;

        roomsvr::PushRoomKickedNtf ntf;
        if (!ntf.ParseFromArray(codec.GetBody(), static_cast<int>(codec.GetBodyLen())))
            return 0;

        KickedPush push;
        push.room_id = ntf.room_id();
        push.reason = ntf.reason();
        for (uint64_t gid : ntf.target_gids())
            push.target_gids.push_back(gid);
        kicked_pushes_.push_back(std::move(push));
        return 0;
    }

    const std::vector<KickedPush>& kicked_pushes() const { return kicked_pushes_; }
    void Clear() { kicked_pushes_.clear(); }

    /// 该 gid 是否在任何一条 Kicked 推送的目标里
    bool WasKicked(uint64_t gid) const
    {
        for (const auto& push : kicked_pushes_)
        {
            for (uint64_t target : push.target_gids)
            {
                if (target == gid)
                    return true;
            }
        }
        return false;
    }

private:
    std::vector<KickedPush> kicked_pushes_;
};

}  // namespace

int main()
{
    // ---- 装上可捕获推送的 transport（roomsvr 的推送都走 TRANSPORT_PB_TBUSPP）----
    CapturingChannel channel;
    app::PbRecvCodec recv_codec;
    app::PbSendCodec send_codec;
    app::TransportInfo info;
    info.channel = &channel;
    info.recv_codec = &recv_codec;
    info.send_codec = &send_codec;
    CHECK(app::RpcService::GetInst().AddTransport(app::TRANSPORT_PB_TBUSPP, info), "install capturing transport");

    constexpr uint64_t kMover1 = 4294967351;   // 退出旧房 -> 建了新房
    constexpr uint64_t kMover2 = 4294967355;   // 退出旧房 -> 进了新房
    constexpr uint64_t kToLobby = 4294967352;  // 退出旧房 -> 留在大厅
    constexpr uint64_t kStayer = 4294967354;   // 全程留在旧房，打完整场

    // ---- 旧房间：4 个真人，开打 ----
    // AddRoom 自动分配 room_id、把创建者加为成员并建好 gid->room 映射
    roomsvr::Room* old_room = roomsvr::RoomMgr::GetInst().AddRoom(kMover1, "old_battle", 8);
    CHECK(old_room != nullptr, "create old room");
    const uint64_t kOldRoom = old_room->room_id();
    CHECK(old_room->host_gid() == kMover1, "old room host is creator");
    CHECK(old_room->AddMember(kMover2, 2, false) == 0, "old room add mover2");
    CHECK(old_room->AddMember(kToLobby, 3, false) == 0, "old room add to-lobby player");
    CHECK(old_room->AddMember(kStayer, 4, false) == 0, "old room add stayer");
    roomsvr::RoomMgr::GetInst().AddGidRoomMapping(kMover2, kOldRoom);
    roomsvr::RoomMgr::GetInst().AddGidRoomMapping(kToLobby, kOldRoom);
    roomsvr::RoomMgr::GetInst().AddGidRoomMapping(kStayer, kOldRoom);
    CHECK(old_room->real_player_count() == 4, "old room has four real players");

    old_room->BeginBattleGeneration();
    old_room->SetInBattle("127.0.0.1:20012", "B_56");
    old_room->SetInBattleState();
    CHECK(old_room->state() == roomsvr::ROOM_STATE_IN_BATTLE, "old room is in battle");

    // ---- 战斗中三人点退出：只打标记 + 解除 gid->room 映射（复刻 LeaveRoom deferred 分支）----
    for (uint64_t gid : {kMover1, kMover2, kToLobby})
    {
        old_room->MarkPendingLeave(gid);
        roomsvr::RoomMgr::GetInst().RemoveGidRoomMapping(gid);
    }
    CHECK(old_room->HasMember(kMover1), "deferred leave keeps member in old room roster");
    CHECK(roomsvr::RoomMgr::GetInst().GetGidRoom(kMover1) == 0, "deferred leave releases gid->room mapping");

    // ---- 旧房还在战斗中，两人已经进了新房间并开打 ----
    // kMover1 的映射已在上面被解除，AddRoom 的「已在房间」校验才能过
    roomsvr::Room* new_room = roomsvr::RoomMgr::GetInst().AddRoom(kMover1, "new_battle", 8);
    CHECK(new_room != nullptr, "create new room");
    const uint64_t kNewRoom = new_room->room_id();
    CHECK(kNewRoom != kOldRoom, "new room has a distinct id");
    CHECK(new_room->AddMember(kMover2, 2, false) == 0, "new room add mover2");
    roomsvr::RoomMgr::GetInst().AddGidRoomMapping(kMover2, kNewRoom);
    CHECK(roomsvr::RoomMgr::GetInst().GetGidRoom(kMover1) == kNewRoom, "mover1 now maps to new room");
    new_room->BeginBattleGeneration();
    new_room->SetInBattle("127.0.0.1:20013", "B_57");
    new_room->SetInBattleState();

    // kToLobby 没有进新房间，仍在大厅（映射为 0）
    CHECK(roomsvr::RoomMgr::GetInst().GetGidRoom(kToLobby) == 0, "to-lobby player has no room");

    // ---- 旧房间结算：这里是 bug 触发点 ----
    channel.Clear();
    roomsvr::RoomService::DoGuaranteedSettle(*old_room, 0);
    bool destroyed = roomsvr::RoomService::ApplyPendingLeaves(kOldRoom);
    CHECK(!destroyed, "old room survives because stayer is still in it");

    // 核心断言：已在新房间的两人绝不能收到旧房的 Kicked
    CHECK(!channel.WasKicked(kMover1), "player who already joined a new room must NOT be kicked");
    CHECK(!channel.WasKicked(kMover2), "second player in new room must NOT be kicked");

    // 真的退到大厅的人仍要收到通知，否则客户端会卡在残留的房间界面
    CHECK(channel.WasKicked(kToLobby), "player who left to lobby must still receive kicked push");

    // 打完整场的人不在待离房名单里，不该被推
    CHECK(!channel.WasKicked(kStayer), "player who stayed must not be kicked");

    // 推送里带的 room_id 应当是旧房间
    for (const auto& push : channel.kicked_pushes())
        CHECK(push.room_id == kOldRoom, "kicked push carries the old room id");

    // ---- 房间状态清理仍要照做：三人都要从旧房名单里移除 ----
    CHECK(!old_room->HasMember(kMover1), "mover1 removed from old room roster");
    CHECK(!old_room->HasMember(kMover2), "mover2 removed from old room roster");
    CHECK(!old_room->HasMember(kToLobby), "to-lobby player removed from old room roster");
    CHECK(old_room->HasMember(kStayer), "stayer remains in old room");
    CHECK(old_room->real_player_count() == 1, "old room keeps exactly one real player");

    // ---- 映射保护：不能把已指向新房间的映射误删 ----
    CHECK(roomsvr::RoomMgr::GetInst().GetGidRoom(kMover1) == kNewRoom, "mover1 mapping still points to new room");
    CHECK(roomsvr::RoomMgr::GetInst().GetGidRoom(kMover2) == kNewRoom, "mover2 mapping still points to new room");

    // ---- 房主迁移：旧房主已离开，应迁到留下的人 ----
    CHECK(old_room->host_gid() == kStayer, "host migrated to the remaining player");

    // ============================================================
    // 场景二：最后一个真人也走了 -> 房间销毁，且仍要过滤掉已在新房间的人
    // ============================================================
    channel.Clear();
    old_room->MarkPendingLeave(kStayer);
    roomsvr::RoomMgr::GetInst().RemoveGidRoomMapping(kStayer);
    // 再塞一个「已经在新房间」的待离房标记，确认销毁路径也做了过滤
    CHECK(old_room->AddMember(kMover1, 2, false) == 0, "re-add mover1 to old room roster for destroy-path check");
    old_room->MarkPendingLeave(kMover1);

    destroyed = roomsvr::RoomService::ApplyPendingLeaves(kOldRoom);
    CHECK(destroyed, "old room destroyed once all real players left");
    CHECK(roomsvr::RoomMgr::GetInst().GetRoom(kOldRoom) == nullptr, "old room freed");
    CHECK(channel.WasKicked(kStayer), "last leaver receives room_destroyed push");
    CHECK(!channel.WasKicked(kMover1), "destroy path must also skip player who is in another room");
    for (const auto& push : channel.kicked_pushes())
        CHECK(push.reason == "room_destroyed", "destroy path uses room_destroyed reason");

    // 新房间不受旧房销毁影响
    CHECK(roomsvr::RoomMgr::GetInst().GetRoom(kNewRoom) != nullptr, "new room untouched");
    CHECK(roomsvr::RoomMgr::GetInst().GetGidRoom(kMover1) == kNewRoom, "mover1 still mapped to new room");

    roomsvr::RoomMgr::GetInst().FreeRoom(kNewRoom);
    std::puts("ALL PENDING LEAVE KICK TESTS PASSED");
    return 0;
}
