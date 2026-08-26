/*
 * * file name: room_mgr.h
 * * description: 房间管理器(Singleton)，管理所有房间的生命周期：
 * *              room_id生成、房间增删查、超时清理、大厅订阅者、gid→room映射
 */

#ifndef _ROOM_MGR_H_
#define _ROOM_MGR_H_

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "patterns/singleton.h"
#include "room.h"

namespace roomsvr
{

/// OnTick返回的超时房间信息，用于上层发送DestroyDs等RPC清理
struct TimeoutRoomInfo
{
    uint64_t room_id = 0;
    uint32_t dsa_svr_id = 0;  // 0 if no DS allocated
    uint64_t battle_generation = 0;
    bool is_battle_timeout = false;  // true: battle timeout; false: empty room timeout
};

/// StartBattle校验成功后，由主循环立即启动的DS创建任务
struct PendingDsFlow
{
    uint64_t room_id = 0;
    uint64_t host_gid = 0;
    uint64_t battle_generation = 0;
};

class RoomMgr : public app::Singleton<RoomMgr>
{
public:
    /// 创建房间，返回room指针（所有权归RoomMgr），失败返回nullptr
    Room* AddRoom(uint64_t creator_gid, const std::string& room_name, uint32_t max_players,
                  uint32_t map_id = kDefaultMapId, const std::string& display_name = "");

    /// 获取房间（按room_id）
    Room* GetRoom(uint64_t room_id);

    /// 获取所有房间（用于列表查询）
    const std::unordered_map<uint64_t, std::unique_ptr<Room>>& GetRooms() const { return rooms_; }

    /// 释放房间
    void FreeRoom(uint64_t room_id);

    /// 查找玩家所在的房间（使用gid_to_room_快速查找）
    Room* GetRoomByGid(uint64_t gid);

    /// 定时清理超时房间，返回超时房间列表（含DS信息）供上层做DS清理RPC
    std::vector<TimeoutRoomInfo> OnTick(uint64_t now_ms);

    /// 将StartBattle通过后的DS创建任务交给主循环启动
    void EnqueueDsFlow(uint64_t room_id, uint64_t host_gid, uint64_t battle_generation);
    std::vector<PendingDsFlow> TakePendingDsFlows();

    // ---- 大厅订阅者管理 ----
    void AddLobbySubscriber(uint64_t gid);
    void RemoveLobbySubscriber(uint64_t gid);
    const std::set<uint64_t>& GetLobbySubscribers() const;

    // ---- gid→room映射 ----
    void AddGidRoomMapping(uint64_t gid, uint64_t room_id);
    void RemoveGidRoomMapping(uint64_t gid);
    uint64_t GetGidRoom(uint64_t gid) const;

    // ---- 房间列表快照 ----
    void BuildRoomListSnapshot(roomsvr::PushRoomListNtf& ntf) const;

    /// 房间超时阈值（毫秒）：没人且不在战斗的房间超过此时间自动销毁
    /// 有人的房间依赖断连回调自动LeaveRoom；没人在战斗的房间依赖NotifyDsTimeout
    static constexpr uint64_t kRoomTimeoutMs = 60000;  // 60秒

    /// 战斗超时阈值（毫秒）：战斗开始超过此时间没有GameFinish，强制保底结算
    /// 需覆盖SoloRounds最长耗时（Cut+Prep+Combat x N轮+Settle），避免中途被兜底提前打断
    static constexpr uint64_t kBattleTimeoutMs = 1800000;  // 30分钟

private:
    friend class app::Singleton<RoomMgr>;
    RoomMgr() = default;

    uint64_t next_room_id_ = 1;
    std::unordered_map<uint64_t, std::unique_ptr<Room>> rooms_;

    /// 大厅订阅者（gid集合）
    std::set<uint64_t> lobby_subscribers_;

    /// gid → room_id 快速查找（替代遍历所有房间）
    std::unordered_map<uint64_t, uint64_t> gid_to_room_;

    std::vector<PendingDsFlow> pending_ds_flows_;
};

}  // namespace roomsvr

#endif
