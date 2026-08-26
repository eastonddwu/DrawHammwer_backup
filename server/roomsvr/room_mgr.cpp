/*
 * * file name: room_mgr.cpp
 * * description: RoomMgr实现，见room_mgr.h说明
 * */

#include "room_mgr.h"
#include <chrono>
#include "core/log.h"

namespace roomsvr
{

Room* RoomMgr::AddRoom(uint64_t creator_gid, const std::string& room_name, uint32_t max_players, uint32_t map_id,
                        const std::string& display_name)
{
    if (max_players == 0)
        max_players = 8;
    if (max_players > 8)
        return nullptr;
    if (map_id == 0)
        map_id = kDefaultMapId;
    if (!IsValidMapId(map_id))
        return nullptr;

    // 检查该玩家是否已在某个房间中
    if (GetGidRoom(creator_gid) != 0)
    {
        Room* existing = GetRoom(GetGidRoom(creator_gid));
        if (existing)
        {
            APP_LOG_WARN(creator_gid, "player already in room(%llu), cannot create new room",
                         static_cast<unsigned long long>(existing->room_id()));
        }
        return nullptr;
    }

    uint64_t room_id = next_room_id_++;
    auto room = std::make_unique<Room>(room_id, creator_gid, room_name, max_players, map_id);

    room->set_state(ROOM_STATE_WAITING);
    // 用系统时钟的毫秒时间戳作为创建时间（用于超时检测）
    auto now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
    room->set_create_time_ms(now_ms);

    auto* ptr = room.get();
    rooms_[room_id] = std::move(room);

    // 创建者默认已准备；其他真人加入后仍需显式准备。
    auto now_ts = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    ptr->AddMember(creator_gid, now_ts, true, display_name);

    // 维护 gid→room 映射
    AddGidRoomMapping(creator_gid, room_id);

    APP_LOG_INFO(creator_gid, "room created, room_id(%llu), max_players(%u)", static_cast<unsigned long long>(room_id),
                 max_players > 0 ? max_players : 16);
    return ptr;
}

Room* RoomMgr::GetRoom(uint64_t room_id)
{
    auto it = rooms_.find(room_id);
    if (it == rooms_.end())
        return nullptr;
    return it->second.get();
}

void RoomMgr::FreeRoom(uint64_t room_id)
{
    auto it = rooms_.find(room_id);
    if (it != rooms_.end())
    {
        // 清理 gid→room 映射（仅清仍指向本房间的，避免误删已重登换房玩家的新归属）
        Room* room = it->second.get();
        for (const auto& m : room->members())
        {
            if (!m.b_is_bot && GetGidRoom(m.gid) == room_id)
                RemoveGidRoomMapping(m.gid);
        }

        APP_LOG_INFO(0, "room freed, room_id(%llu)", static_cast<unsigned long long>(room_id));
        rooms_.erase(it);
    }
}

Room* RoomMgr::GetRoomByGid(uint64_t gid)
{
    uint64_t room_id = GetGidRoom(gid);
    if (room_id == 0)
        return nullptr;
    return GetRoom(room_id);
}

std::vector<TimeoutRoomInfo> RoomMgr::OnTick(uint64_t now_ms)
{
    // 用system_clock获取当前时间（与create_time_ms_同源）
    auto tick_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());

    std::vector<TimeoutRoomInfo> timeout_infos;

    // 1. 空房间超时（原有逻辑）
    std::vector<uint64_t> empty_timeout_ids;
    for (auto& [id, room] : rooms_)
    {
        if (room->state() == ROOM_STATE_DESTROYED)
            continue;
        // 有人的房间不超时；没人在战斗的房间靠NotifyDsTimeout
        if (room->real_player_count() > 0 || room->in_battle())
            continue;
        if (room->IsTimeout(tick_ms, kRoomTimeoutMs))
        {
            APP_LOG_WARN(0, "room timeout (empty), room_id(%llu), state(%u)", static_cast<unsigned long long>(id),
                         static_cast<uint32_t>(room->state()));
            timeout_infos.push_back({id, room->dsa_svr_id(), room->battle_generation(), false});
            empty_timeout_ids.push_back(id);
        }
    }

    // FreeRoom empty timeout rooms（原有行为）
    for (uint64_t id : empty_timeout_ids)
        FreeRoom(id);

    // 2. 战斗超时（5分钟无GameFinish）
    for (auto& [id, room] : rooms_)
    {
        if (room->state() == ROOM_STATE_IN_BATTLE && room->battle_start_time_ms() > 0)
        {
            if ((tick_ms - room->battle_start_time_ms()) > kBattleTimeoutMs)
            {
                APP_LOG_WARN(0, "battle timeout (30min), room_id(%llu)", static_cast<unsigned long long>(id));
                timeout_infos.push_back({id, room->dsa_svr_id(), room->battle_generation(), true});
            }
        }
    }

    return timeout_infos;
}

void RoomMgr::EnqueueDsFlow(uint64_t room_id, uint64_t host_gid, uint64_t battle_generation)
{
    pending_ds_flows_.push_back({room_id, host_gid, battle_generation});
}

std::vector<PendingDsFlow> RoomMgr::TakePendingDsFlows()
{
    std::vector<PendingDsFlow> flows;
    flows.swap(pending_ds_flows_);
    return flows;
}

// ---- 大厅订阅者管理 ----

void RoomMgr::AddLobbySubscriber(uint64_t gid)
{
    lobby_subscribers_.insert(gid);
}

void RoomMgr::RemoveLobbySubscriber(uint64_t gid)
{
    lobby_subscribers_.erase(gid);
}

const std::set<uint64_t>& RoomMgr::GetLobbySubscribers() const
{
    return lobby_subscribers_;
}

// ---- gid→room映射 ----

void RoomMgr::AddGidRoomMapping(uint64_t gid, uint64_t room_id)
{
    gid_to_room_[gid] = room_id;
}

void RoomMgr::RemoveGidRoomMapping(uint64_t gid)
{
    gid_to_room_.erase(gid);
}

uint64_t RoomMgr::GetGidRoom(uint64_t gid) const
{
    auto it = gid_to_room_.find(gid);
    return (it != gid_to_room_.end()) ? it->second : 0;
}

// ---- 房间列表快照 ----

void RoomMgr::BuildRoomListSnapshot(roomsvr::PushRoomListNtf& ntf) const
{
    for (const auto& [id, room] : rooms_)
    {
        if (room->state() == ROOM_STATE_DESTROYED)
            continue;
        auto* brief = ntf.add_rooms();
        room->BuildBriefInfo(*brief);
    }
}

}  // namespace roomsvr
