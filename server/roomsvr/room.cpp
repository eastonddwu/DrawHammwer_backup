/*
 * * file name: room.cpp
 * * description: Room类实现，见room.h说明
 * */

#include "room.h"
#include "common/clock.h"
#include "core/log.h"

namespace roomsvr
{

Room::Room(uint64_t room_id, uint64_t creator_gid, const std::string& room_name, uint32_t max_players,
           uint32_t map_id)
    : room_id_(room_id),
      creator_gid_(creator_gid),
      host_gid_(creator_gid),
      room_name_(room_name),
      max_players_(max_players > 0 ? max_players : 8),
      map_id_(IsValidMapId(map_id) ? map_id : kDefaultMapId),
      create_time_ms_(0)  // 由RoomMgr设置
{
}

// ---- 新版：成员管理 ----

uint32_t Room::real_player_count() const
{
    uint32_t count = 0;
    for (const auto& m : members_)
    {
        if (!m.b_is_bot)
            ++count;
    }
    return count;
}

uint32_t Room::bot_count() const
{
    uint32_t count = 0;
    for (const auto& m : members_)
    {
        if (m.b_is_bot)
            ++count;
    }
    return count;
}

RoomMemberData* Room::GetMember(uint64_t gid)
{
    for (auto& m : members_)
    {
        if (!m.b_is_bot && m.gid == gid)
            return &m;
    }
    return nullptr;
}

const RoomMemberData* Room::GetMember(uint64_t gid) const
{
    for (const auto& m : members_)
    {
        if (!m.b_is_bot && m.gid == gid)
            return &m;
    }
    return nullptr;
}

RoomMemberData* Room::GetBot(const std::string& bot_id)
{
    for (auto& m : members_)
    {
        if (m.b_is_bot && m.bot_id == bot_id)
            return &m;
    }
    return nullptr;
}

const RoomMemberData* Room::GetBot(const std::string& bot_id) const
{
    for (const auto& m : members_)
    {
        if (m.b_is_bot && m.bot_id == bot_id)
            return &m;
    }
    return nullptr;
}

const RoomMemberData* Room::GetBattleMember(uint64_t gid) const
{
    for (const auto& member : battle_members_)
    {
        if (!member.b_is_bot && member.gid == gid)
            return &member;
    }
    return nullptr;
}

const RoomMemberData* Room::GetBattleBot(const std::string& bot_id) const
{
    for (const auto& member : battle_members_)
    {
        if (member.b_is_bot && member.bot_id == bot_id)
            return &member;
    }
    return nullptr;
}

bool Room::HasMember(uint64_t gid) const
{
    return GetMember(gid) != nullptr;
}

bool Room::HasBot(const std::string& bot_id) const
{
    return GetBot(bot_id) != nullptr;
}

RoomMemberData* Room::GetMemberBySlot(uint32_t slot_index)
{
    if (slot_index == 0 || slot_index > max_players_)
        return nullptr;
    for (auto& m : members_)
    {
        if (m.slot_index == slot_index)
            return &m;
    }
    return nullptr;
}

const RoomMemberData* Room::GetMemberBySlot(uint32_t slot_index) const
{
    if (slot_index == 0 || slot_index > max_players_)
        return nullptr;
    for (const auto& m : members_)
    {
        if (m.slot_index == slot_index)
            return &m;
    }
    return nullptr;
}

bool Room::IsHost(uint64_t gid) const
{
    return gid == host_gid_;
}

std::string Room::host_display_name() const
{
    const RoomMemberData* host = GetMember(host_gid_);
    if (!host)
    {
        APP_LOG_WARN(host_gid_, "host not in member list, room_id(%llu), host_display_name empty",
                     static_cast<unsigned long long>(room_id_));
        return "";
    }
    return host->display_name;
}

void Room::MarkPendingLeave(uint64_t gid)
{
    pending_leave_gids_.insert(gid);
}

void Room::CancelPendingLeave(uint64_t gid)
{
    pending_leave_gids_.erase(gid);
}

std::vector<uint64_t> Room::TakePendingLeaves()
{
    std::vector<uint64_t> gids(pending_leave_gids_.begin(), pending_leave_gids_.end());
    pending_leave_gids_.clear();
    return gids;
}

int Room::AddMember(uint64_t gid, int64_t join_timestamp, bool is_ready, const std::string& display_name)
{
    if (HasMember(gid))
        return 1;  // 已在房间中

    uint32_t slot_index = 0;
    for (uint32_t slot = 1; slot <= max_players_; ++slot)
    {
        if (!GetMemberBySlot(slot))
        {
            slot_index = slot;
            break;
        }
    }
    if (slot_index == 0)
        return 2;  // 房间已满

    RoomMemberData member;
    member.gid = gid;
    member.join_timestamp = join_timestamp;
    member.is_ready = is_ready;
    member.battle_role_type = kDefaultBattleRoleType;
    member.slot_index = slot_index;
    member.display_name = display_name;
    members_.push_back(member);
    // 同步旧版 player_set_，只保存真人gid
    player_set_.insert(gid);

    APP_LOG_INFO(gid, "member join room, room_id(%llu), member_count(%u/%u)", static_cast<unsigned long long>(room_id_),
                 member_count(), max_players_);
    return 0;
}

int Room::AddBot(int64_t join_timestamp, std::string* out_bot_id, uint32_t* out_slot_index)
{
    uint32_t slot_index = 0;
    for (uint32_t slot = 1; slot <= max_players_; ++slot)
    {
        if (!GetMemberBySlot(slot))
        {
            slot_index = slot;
            break;
        }
    }
    if (slot_index == 0)
        return 1;

    std::string bot_id;
    do
    {
        bot_id = "bot_" + std::to_string(next_bot_serial_++);
    } while (HasBot(bot_id));

    RoomMemberData member;
    member.join_timestamp = join_timestamp;
    member.is_ready = true;
    member.battle_role_type = kDefaultBattleRoleType;
    member.b_is_bot = true;
    member.bot_id = bot_id;
    member.slot_index = slot_index;
    members_.push_back(member);
    if (out_bot_id)
        *out_bot_id = bot_id;
    if (out_slot_index)
        *out_slot_index = slot_index;

    APP_LOG_INFO(0, "bot join room, room_id(%llu), bot_id(%s), member_count(%u/%u)",
                 static_cast<unsigned long long>(room_id_), bot_id.c_str(), member_count(), max_players_);
    return 0;
}

bool Room::RemoveBot(uint32_t slot_index, const std::string& bot_id)
{
    for (auto it = members_.begin(); it != members_.end(); ++it)
    {
        if (!it->b_is_bot)
            continue;
        if (slot_index != 0 && it->slot_index != slot_index)
            continue;
        if (!bot_id.empty() && it->bot_id != bot_id)
            continue;
        members_.erase(it);
        return true;
    }
    return false;
}

void Room::RemoveMember(uint64_t gid)
{
    for (auto it = members_.begin(); it != members_.end(); ++it)
    {
        if (it->gid == gid)
        {
            members_.erase(it);
            player_set_.erase(gid);
            return;
        }
    }
}

bool Room::SetReady(uint64_t gid, bool ready)
{
    auto* m = GetMember(gid);
    if (!m)
        return false;
    m->is_ready = ready;
    return true;
}

bool Room::SetMemberDisplayName(uint64_t gid, const std::string& display_name)
{
    auto* m = GetMember(gid);
    if (!m || m->display_name == display_name)
        return false;
    m->display_name = display_name;
    return true;
}

bool Room::AllReady() const
{
    for (const auto& m : members_)
    {
        // Bot恒定为已准备，不参与真人准备校验
        if (m.b_is_bot)
            continue;
        if (!m.is_ready)
            return false;
    }
    return true;
}

bool Room::SetMap(uint32_t map_id)
{
    if (map_id_ == map_id)
        return false;

    map_id_ = map_id;
    for (auto& member : members_)
    {
        if (member.b_is_bot)
            member.is_ready = true;
        else if (member.gid != host_gid_)
            member.is_ready = false;
    }
    return true;
}

uint64_t Room::MigrateHost()
{
    if (members_.empty())
        return 0;

    // 按 join_timestamp 升序，取第一个非当前房主的成员
    const RoomMemberData* earliest = nullptr;
    for (const auto& m : members_)
    {
        if (m.b_is_bot || m.gid == host_gid_)
            continue;
        if (!earliest || m.join_timestamp < earliest->join_timestamp)
            earliest = &m;
    }

    if (!earliest)
        return 0;  // 只有房主一人，无人可转

    host_gid_ = earliest->gid;
    // 维持"房主恒定已准备"不变量：建房、切地图、结算都保证房主是已准备态，
    // 房主迁移同样要补上，否则新房主点开始会被AllReady()挡住且客户端不会补发SetReady。
    if (RoomMemberData* new_host = GetMember(host_gid_))
        new_host->is_ready = true;
    APP_LOG_INFO(host_gid_, "host migrated, room_id(%llu), new_host(%llu)", static_cast<unsigned long long>(room_id_),
                 static_cast<unsigned long long>(host_gid_));
    return host_gid_;
}

uint64_t Room::BeginBattleGeneration()
{
    battle_members_ = members_;
    return ++battle_generation_;
}

void Room::SetInBattle(const std::string& addr, const std::string& bid)
{
    in_battle_ = true;
    battle_server_address_ = addr;
    battle_id_ = bid;
}

// ---- 房间内选人 ----

bool Room::SetBattleRole(uint64_t gid, uint32_t battle_role_type)
{
    auto* m = GetMember(gid);
    if (!m)
        return false;
    m->battle_role_type = battle_role_type;
    return true;
}

void Room::FinalizeBattleRoles()
{
    for (auto& m : members_)
    {
        if (m.battle_role_type == 0)
            m.battle_role_type = kDefaultBattleRoleType;
    }
}

// ---- 旧版兼容 ----

int Room::AddPlayer(uint64_t gid)
{
    // 旧版不含 timestamp/is_ready，用0/false 默认值
    int64_t now_ts = 0;
    return AddMember(gid, now_ts, false);
}

void Room::RemovePlayer(uint64_t gid)
{
    RemoveMember(gid);
}

bool Room::HasPlayer(uint64_t gid) const
{
    return HasMember(gid);
}

// ---- 超时 ----

bool Room::IsTimeout(uint64_t now_ms, uint64_t timeout_ms) const
{
    if (create_time_ms_ == 0)
        return false;
    return (now_ms - create_time_ms_) > timeout_ms;
}

// ---- DS token ----

uint64_t Room::GetPlayerToken(uint64_t gid) const
{
    auto it = player_tokens_.find(gid);
    return (it != player_tokens_.end()) ? it->second : 0;
}

void Room::SetPlayerToken(uint64_t gid, uint64_t token)
{
    player_tokens_[gid] = token;
}

bool Room::HasPlayerToken(uint64_t gid) const
{
    return player_tokens_.count(gid) > 0;
}

// ---- 快照构建 ----

void Room::BuildDetailSnapshot(roomsvr::PushRoomDetailNtf& ntf) const
{
    ntf.set_room_id(room_id_);
    ntf.set_room_name(room_name_);
    ntf.set_host_gid(host_gid_);
    ntf.set_max_players(max_players_);
    ntf.set_map_id(map_id_);
    ntf.set_in_battle(in_battle_);
    ntf.set_battle_server_address(battle_server_address_);

    if (has_last_match_)
        *ntf.mutable_last_match() = last_match_;

    for (const auto& m : members_)
    {
        auto* info = ntf.add_members();
        info->set_gid(m.gid);
        info->set_join_timestamp(m.join_timestamp);
        info->set_is_ready(m.is_ready);
        info->set_battle_role_type(m.battle_role_type);
        info->set_b_is_bot(m.b_is_bot);
        info->set_bot_id(m.bot_id);
        info->set_slot_index(m.slot_index);
        info->set_display_name(m.display_name);
    }
}

void Room::BuildBriefInfo(roomsvr::RoomBriefInfo& brief) const
{
    brief.set_room_id(room_id_);
    brief.set_room_name(room_name_);
    brief.set_host_gid(host_gid_);
    brief.set_current_players(static_cast<int32_t>(members_.size()));
    brief.set_max_players(max_players_);
    brief.set_map_id(map_id_);
    brief.set_in_battle(in_battle_);
    brief.set_has_last_match(has_last_match_);
    brief.set_host_display_name(host_display_name());
}

// ---- 战斗结算 ----

void Room::SetInBattleState()
{
    state_ = ROOM_STATE_IN_BATTLE;
    battle_start_time_ms_ = app::Clock::GetInst().CurrentMilliSec();
}

void Room::SetLastMatch(const roomsvr::MatchSummary& match)
{
    last_match_ = match;
    has_last_match_ = true;
}

void Room::ClearLastMatch()
{
    last_match_.Clear();
    has_last_match_ = false;
}

void Room::StoreSettleData(uint64_t gid, const roomsvr::FightPlayerInfo& info)
{
    settle_data_[gid] = info;
}

void Room::StoreBotSettleData(const std::string& bot_id, const roomsvr::FightPlayerInfo& info)
{
    bot_settle_data_[bot_id] = info;
}

roomsvr::MatchSummary Room::BuildMatchSummary(uint32_t duration_sec, uint32_t end_reason) const
{
    roomsvr::MatchSummary match;
    match.set_duration_sec(duration_sec);
    match.set_end_reason(end_reason);

    const std::vector<RoomMemberData>& roster = battle_members_.empty() ? members_ : battle_members_;
    for (const auto& member : roster)
    {
        auto* player = match.add_players();
        player->set_battle_role_type(member.battle_role_type);

        const roomsvr::FightPlayerInfo* settle = nullptr;
        if (member.b_is_bot)
        {
            player->set_gid(0);
            player->set_b_is_bot(true);
            player->set_bot_id(member.bot_id);
            auto it = bot_settle_data_.find(member.bot_id);
            if (it != bot_settle_data_.end())
                settle = &it->second;
        }
        else
        {
            player->set_gid(member.gid);
            auto it = settle_data_.find(member.gid);
            if (it != settle_data_.end())
                settle = &it->second;
        }

        if (settle)
        {
            player->set_display_name(settle->display_name());
            player->set_kills(settle->kills());
            player->set_deaths(settle->deaths());
            player->set_rank(settle->rank());
        }

        // DS上报的昵称可能为空（游客不落库，DS查不到user_name），用房间内记录的登录昵称兜底
        if (player->display_name().empty() && !member.display_name.empty())
            player->set_display_name(member.display_name);
    }
    return match;
}

void Room::ClearBattleState()
{
    state_ = ROOM_STATE_WAITING;
    in_battle_ = false;
    battle_server_address_.clear();
    battle_id_.clear();
    battle_members_.clear();
    settle_data_.clear();
    bot_settle_data_.clear();
    player_tokens_.clear();
    battle_start_time_ms_ = 0;
    dsa_svr_id_ = 0;
    ds_conn_info_.Clear();
    // 结算后重置准备态：Bot恒定已准备；房主保持已准备(与建房、切地图一致，房主无需重复点准备)；
    // 其余真人清空，需重新点准备才能开下一局。
    // 房主若被一起清掉，客户端不会再补发SetReady，AllReady()将永远失败，同一房间无法开第二局。
    for (auto& m : members_)
    {
        if (m.b_is_bot)
            m.is_ready = true;
        else
            m.is_ready = (m.gid == host_gid_);
    }
}

}  // namespace roomsvr
