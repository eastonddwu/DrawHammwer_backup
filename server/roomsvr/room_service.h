/*
 * * file name: room_service.h
 * * description: roomsvr具体RPC方法实现
 */

#ifndef _ROOM_SERVICE_H_
#define _ROOM_SERVICE_H_

#include "core/rpc_context.h"
#include "room.h"

namespace roomsvr
{
class RoomService
{
public:
    // ---- 房间操作（connsvr→roomsvr）----
    static void CreateRoom(app::RpcContext& context);
    static void JoinRoom(app::RpcContext& context);
    static void LeaveRoom(app::RpcContext& context);
    static void SetReady(app::RpcContext& context);
    static void StartBattle(app::RpcContext& context);
    static void AddBot(app::RpcContext& context);
    static void RemoveBot(app::RpcContext& context);
    static void RoomSetRole(app::RpcContext& context);
    static void RoomSetMap(app::RpcContext& context);
    static void RoomSendEmote(app::RpcContext& context);
    static void RenameRoom(app::RpcContext& context);
    static void QueryRoomList(app::RpcContext& context);
    static void QueryPlayerRoomState(app::RpcContext& context);
    static void UpdateMemberName(app::RpcContext& context);

    // ---- 内部：dsagent→roomsvr ----
    static void NotifyDsStarted(app::RpcContext& context);
    static void NotifyDsTimeout(app::RpcContext& context);

    // ---- DS结算（DS→dsagent→roomsvr）----
    static void RoomDsPlayerSettle(app::RpcContext& context);
    static void RoomDsGameFinish(app::RpcContext& context);

    // ---- 推送辅助函数（RoomApp::OnTick也需要调用）----
    static void DoPushRoomDetail(const Room& room);

    /// 请求推送房间列表。不立即发送，只置脏标记，由主循环的FlushPendingRoomList()
    /// 按kRoomListPushIntervalMs合并后发一次。
    ///
    /// 房间列表是全局广播：roomsvr要遍历所有房间构建全量快照，connsvr再推给每个在线玩家，
    /// 开销是O(房间数 × 在线数)。而建房/加入/离开/准备/改名等十余个高频动作都会触发它，
    /// 短时间内的多次触发产生的快照内容几乎相同，合并成一次可大幅削减扇出。
    static void DoPushRoomList();

    /// 主循环调用：若有挂起的房间列表推送且已达间隔，则真正发送一次。
    /// 返回true表示本次确实发送了。
    static bool FlushPendingRoomList(uint64_t now_ms);

    static void DoGuaranteedSettle(Room& room, uint32_t end_reason);

    /// 战斗结束后补做「战斗中掉线」玩家的离房。返回true表示房间已被销毁（room指针失效）。
    /// 必须在ClearBattleState()之后、使用room指针之前调用。
    static bool ApplyPendingLeaves(uint64_t room_id);

    /// StartBattle后由主循环协程执行：AllocDsa → CreateGame → PushBattleReady。
    static void StartDsFlow(uint64_t room_id, uint64_t host_gid, uint64_t battle_generation);
    /// DS创建任务无法启动或RPC失败时，回滚WAITING并推送PushRoomBattleFailed。
    static void FailStartDsFlow(uint64_t room_id, uint64_t battle_generation, int32_t reason,
                                const std::string& message);

    // ---- 推送（roomsvr→connsvr，发送方空实现）----
    static void OnPushRoomDetail(app::RpcContext& context);
    static void OnPushRoomList(app::RpcContext& context);
    static void OnPushBattleReady(app::RpcContext& context);
    static void OnPushRoomKicked(app::RpcContext& context);
    static void OnPushRoomSelecting(app::RpcContext& context);
    static void OnPushRoomBattleFailed(app::RpcContext& context);
    static void OnPushRoomEmote(app::RpcContext& context);

private:
    static void DoPushKicked(const std::vector<uint64_t>& gids, uint64_t room_id, const std::string& reason);
    static void DoPushBattleFailed(const Room& room, int32_t reason, const std::string& message);
    static void DoPushRoomEmote(const Room& room, uint64_t sender_gid, uint32_t emote_id);
};

}  // namespace roomsvr

#endif
