/*
 * * file name: room.h
 * * description: 单个房间的数据模型，管理房间状态、成员列表、DS连接信息
 * *              v2: 新增 RoomMemberData、host_gid(支持房主转移)、is_ready、in_battle
 */

#ifndef _ROOM_H_
#define _ROOM_H_

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "room.pb.h"

namespace roomsvr
{

// 房间状态枚举
enum RoomState : uint32_t
{
    ROOM_STATE_WAITING = 0,      // 等待玩家加入
    ROOM_STATE_DS_CREATING = 1,  // 正在分配DS
    ROOM_STATE_DS_READY = 2,     // DS已就绪
    ROOM_STATE_DESTROYED = 3,    // 房间已销毁
    ROOM_STATE_IN_BATTLE = 4,    // 战斗进行中
};

// 局内角色默认值（后台配置，非账户级回退）
static constexpr uint32_t kDefaultBattleRoleType = 1;
static constexpr uint32_t kDefaultMapId = 101;
static constexpr uint32_t kPoolMapId = 102;

inline bool IsValidMapId(uint32_t map_id)
{
    return map_id == kDefaultMapId || map_id == kPoolMapId;
}

// ---- 房间快捷表情 ----
// 合法emote_id白名单：本期1~6。以后加表情必须改这里并发版，客户端不能单方面扩展。
static constexpr uint32_t kMinEmoteId = 1;
static constexpr uint32_t kMaxEmoteId = 6;

inline bool IsValidEmoteId(uint32_t emote_id)
{
    return emote_id >= kMinEmoteId && emote_id <= kMaxEmoteId;
}

// 成员数据（替代原来的 std::set<uint64_t>）
struct RoomMemberData
{
    uint64_t gid = 0;
    int64_t join_timestamp = 0;  // 加入时间（秒级时间戳，用于房主转移排序）
    bool is_ready = false;
    uint32_t battle_role_type = 0;  // 本局局内角色（选人结果），仅内存态，不落库
    bool b_is_bot = false;
    std::string bot_id;
    uint32_t slot_index = 0;
    std::string display_name;
};

class Room
{
public:
    Room(uint64_t room_id, uint64_t creator_gid, const std::string& room_name, uint32_t max_players,
         uint32_t map_id = kDefaultMapId);

    // ---- 基础访问器 ----
    uint64_t room_id() const { return room_id_; }
    uint64_t creator_gid() const { return creator_gid_; }
    const std::string& room_name() const { return room_name_; }
    uint32_t max_players() const { return max_players_; }
    uint32_t map_id() const { return map_id_; }
    RoomState state() const { return state_; }
    uint32_t dsa_svr_id() const { return dsa_svr_id_; }
    uint64_t create_time_ms() const { return create_time_ms_; }
    uint64_t battle_generation() const { return battle_generation_; }
    const roomsvr::DSConnectInfo& ds_conn_info() const { return ds_conn_info_; }

    // ---- 旧版兼容：player_set ----
    const std::set<uint64_t>& player_set() const { return player_set_; }

    // ---- 新版：成员管理 ----
    const std::vector<RoomMemberData>& members() const { return members_; }
    uint32_t member_count() const { return static_cast<uint32_t>(members_.size()); }
    uint32_t real_player_count() const;
    uint32_t bot_count() const;
    RoomMemberData* GetMember(uint64_t gid);
    const RoomMemberData* GetMember(uint64_t gid) const;
    RoomMemberData* GetBot(const std::string& bot_id);
    const RoomMemberData* GetBot(const std::string& bot_id) const;
    const RoomMemberData* GetBattleMember(uint64_t gid) const;
    const RoomMemberData* GetBattleBot(const std::string& bot_id) const;
    RoomMemberData* GetMemberBySlot(uint32_t slot_index);
    const RoomMemberData* GetMemberBySlot(uint32_t slot_index) const;
    bool HasMember(uint64_t gid) const;
    bool HasBot(const std::string& bot_id) const;
    bool IsHost(uint64_t gid) const;
    uint64_t host_gid() const { return host_gid_; }
    /// 当前房主登录昵称：从成员表按host_gid取，房主换人后自动跟着变。
    /// 查不到（成员已移除/昵称本身为空）返回空串——绝不用gid冒充昵称。
    std::string host_display_name() const;

    // ---- 战斗中掉线延迟离房 ----
    /// 标记该真人战斗结束后需要离房（战斗中掉线不立即移除，避免破坏冻结名单与结算）
    void MarkPendingLeave(uint64_t gid);
    /// 取消待离房标记（战斗结束前重连回来）
    void CancelPendingLeave(uint64_t gid);
    /// 取出并清空所有待离房gid
    std::vector<uint64_t> TakePendingLeaves();

    // ---- 新版：房间状态 ----
    bool in_battle() const { return in_battle_; }
    const std::string& battle_server_address() const { return battle_server_address_; }
    const std::string& battle_id() const { return battle_id_; }

    // ---- 房间内选人 ----
    /// 设置真人成员局内角色，返回true成功（成员存在）
    bool SetBattleRole(uint64_t gid, uint32_t battle_role_type);
    /// 开战时固化角色：异常的未选值（0）填默认角色
    void FinalizeBattleRoles();

    // ---- setter ----
    void set_state(RoomState state) { state_ = state; }
    uint64_t BeginBattleGeneration();
    void set_dsa_svr_id(uint32_t dsa_svr_id) { dsa_svr_id_ = dsa_svr_id; }
    void set_ds_conn_info(const roomsvr::DSConnectInfo& info) { ds_conn_info_ = info; }
    void set_create_time_ms(uint64_t ms) { create_time_ms_ = ms; }
    void set_host_gid(uint64_t gid) { host_gid_ = gid; }
    void set_room_name(const std::string& name) { room_name_ = name; }
    void SetInBattle(const std::string& addr, const std::string& bid);
    void set_in_battle(bool v) { in_battle_ = v; }

    // ---- 成员操作 ----
    /// 添加成员，返回0成功，1已在房间中，2房间已满
    int AddMember(uint64_t gid, int64_t join_timestamp, bool is_ready = false,
                  const std::string& display_name = "");
    /// 添加人机占位，返回0成功，1已无空位
    int AddBot(int64_t join_timestamp, std::string* out_bot_id, uint32_t* out_slot_index);
    /// 按座位移除人机，bot_id可用于旧协议回退
    bool RemoveBot(uint32_t slot_index, const std::string& bot_id);
    /// 移除成员
    void RemoveMember(uint64_t gid);
    /// 设置成员准备状态
    bool SetReady(uint64_t gid, bool ready);
    /// 更新成员登录昵称，返回true=成员存在且昵称有变化
    bool SetMemberDisplayName(uint64_t gid, const std::string& display_name);
    /// 所有真人成员是否全部准备（Bot恒定为已准备）
    bool AllReady() const;
    bool SetMap(uint32_t map_id);
    /// 房主转移：按 join_timestamp 升序取第一个非当前房主的成员，返回新房主gid（0=无人可转）
    uint64_t MigrateHost();

    // ---- 旧版兼容 ----
    int AddPlayer(uint64_t gid);
    void RemovePlayer(uint64_t gid);
    bool HasPlayer(uint64_t gid) const;
    uint32_t player_count() const { return static_cast<uint32_t>(player_set_.size()); }

    // ---- 超时 ----
    bool IsTimeout(uint64_t now_ms, uint64_t timeout_ms) const;

    // ---- DS token ----
    uint64_t GetPlayerToken(uint64_t gid) const;
    void SetPlayerToken(uint64_t gid, uint64_t token);
    bool HasPlayerToken(uint64_t gid) const;

    // ---- 战斗结算 ----
    const roomsvr::MatchSummary& last_match() const { return last_match_; }
    bool has_last_match() const { return has_last_match_; }
    uint64_t battle_start_time_ms() const { return battle_start_time_ms_; }
    const std::unordered_map<uint64_t, roomsvr::FightPlayerInfo>& settle_data() const { return settle_data_; }
    const std::unordered_map<std::string, roomsvr::FightPlayerInfo>& bot_settle_data() const
    {
        return bot_settle_data_;
    }

    void SetInBattleState();  // state→IN_BATTLE, record battle_start_time_ms_
    void SetLastMatch(const roomsvr::MatchSummary& match);
    void ClearLastMatch();
    void StoreSettleData(uint64_t gid, const roomsvr::FightPlayerInfo& info);
    void StoreBotSettleData(const std::string& bot_id, const roomsvr::FightPlayerInfo& info);
    roomsvr::MatchSummary BuildMatchSummary(uint32_t duration_sec, uint32_t end_reason) const;
    void ClearBattleState();  // state→WAITING, clear in_battle + ready + settle_data + dsa

    // ---- 快照构建 ----
    void BuildDetailSnapshot(roomsvr::PushRoomDetailNtf& ntf) const;
    void BuildBriefInfo(roomsvr::RoomBriefInfo& brief) const;

private:
    uint64_t room_id_ = 0;
    uint64_t creator_gid_ = 0;
    uint64_t host_gid_ = 0;  // 当前房主（创建时=creator_gid，可转移）
    std::string room_name_;
    uint32_t max_players_ = 8;
    uint32_t map_id_ = kDefaultMapId;
    RoomState state_ = ROOM_STATE_WAITING;
    uint32_t dsa_svr_id_ = 0;
    uint64_t battle_generation_ = 0;
    roomsvr::DSConnectInfo ds_conn_info_;
    uint64_t create_time_ms_ = 0;

    // 旧版兼容
    std::set<uint64_t> player_set_;
    std::unordered_map<uint64_t, uint64_t> player_tokens_;  // gid -> token

    // 新版成员列表
    std::vector<RoomMemberData> members_;
    uint32_t next_bot_serial_ = 1;

    // 战斗中掉线的真人：不立即移除（保留冻结名单与结算），战斗结束后统一离房
    std::set<uint64_t> pending_leave_gids_;

    // 新版战斗状态
    bool in_battle_ = false;
    std::string battle_server_address_;
    std::string battle_id_;
    std::vector<RoomMemberData> battle_members_;  // Start时固化的参战名单，供离房后的结算使用

    // 战斗结算
    roomsvr::MatchSummary last_match_;
    bool has_last_match_ = false;
    uint64_t battle_start_time_ms_ = 0;
    std::unordered_map<uint64_t, roomsvr::FightPlayerInfo> settle_data_;
    std::unordered_map<std::string, roomsvr::FightPlayerInfo> bot_settle_data_;
};

}  // namespace roomsvr

#endif
