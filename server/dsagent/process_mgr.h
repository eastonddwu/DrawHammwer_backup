/*
 * * file name: process_mgr.h
 * * description: DS进程管理器(Singleton)，管理UE DS子进程的创建/销毁/存活检测
 */

#ifndef _PROCESS_MGR_H_
#define _PROCESS_MGR_H_

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "patterns/singleton.h"

namespace dsagent
{

enum DSState : uint32_t
{
    DS_STATE_STARTING = 0,        // 启动中（fork后等待连接）
    DS_STATE_ALIVE = 1,           // 运行中（已收到心跳或启动超时已过）
    DS_STATE_WAIT_SELF_EXIT = 2,  // 正常结算后等待DS自行flush并退出
    DS_STATE_TERM_SENT = 3,       // 已发送SIGTERM，等待优雅退出
    DS_STATE_KILL_SENT = 4,       // SIGTERM超时，已发送SIGKILL
};

struct DSProcess
{
    uint64_t room_id = 0;
    uint64_t battle_generation = 0;
    pid_t pid = 0;
    pid_t pgid = 0;
    uint16_t port = 0;
    DSState state = DS_STATE_STARTING;
    uint64_t last_heartbeat_ms = 0;
    uint64_t gone_detect_ms = 0;
    bool destroy_requested = false;
    uint32_t destroy_reason = 0;
    uint64_t stop_deadline_ms = 0;
};

class ProcessMgr : public app::Singleton<ProcessMgr>
{
public:
    /// 设置DS端口范围
    void SetPortRange(uint16_t start, uint16_t end);

    /// 创建DS进程，返回0成功，非0失败
    int CreateDS(uint64_t room_id, uint64_t battle_generation, uint32_t requested_port, uint32_t map_id = 101);

    /// 发起异步销毁；正常结束先等自退，其余原因先SIGTERM
    int DestroyDS(uint64_t room_id, uint64_t battle_generation, uint32_t reason);

    /// 获取DS信息
    DSProcess* GetDS(uint64_t room_id);

    /// DS心跳
    void OnHeartBeat(uint64_t room_id);

    /// 获取运行中的DS数量
    size_t GetDSCount() const { return ds_map_.size(); }

    /// 更新认证信息（内存中，不写文件）
    void UpdateAuth(uint64_t room_id, const std::vector<std::pair<uint64_t, uint64_t>>& auth_list);

    /// 更新玩家局内角色（内存中，随DS_AUTH_ROOM_*环境变量下发给DS）
    void UpdateRoles(uint64_t room_id, const std::map<uint64_t, uint32_t>& roles);

    /// 更新玩家昵称（内存中，供DsGetPlayerInfo在dbproxy查不到时兜底；游客不落库）
    void UpdateNames(uint64_t room_id, const std::map<uint64_t, std::string>& names);

    /// 查询已缓存的玩家昵称，查不到返回空串
    std::string GetPlayerName(uint64_t gid) const;

    /// 更新人机身份和角色（不进入真人认证）
    void UpdateBots(uint64_t room_id, const std::vector<std::pair<std::string, uint32_t>>& bots);

    /// 通过环境变量设置认证信息（UE DS一期不回连，auth写入DS_AUTH_ROOM_<room_id> env）
    void PushAuthToDs(uint64_t room_id);

    /// 定时检查：用kill(pid,0)主动探测DS存活，检测到进程消失则清理并通知roomsvr
    void OnTick(uint64_t now_ms);

    /// UE DS进程存活检测阈值（毫秒），fork后等待DS启动的最大时间
    static constexpr uint64_t kDsStartupTimeoutMs = 60000;  // 60秒

    /// 探测到进程消失后的宽限期：UE DS在GameFinish后会自行退出，紧接着roomsvr才发DestroyDs。
    /// 若在此宽限期内DestroyDS到达并清理了记录，则视为正常结束，不误报NotifyDsTimeout；
    /// 超过宽限期仍未被DestroyDS清理，才判定为异常退出/崩溃并通知roomsvr做保底结算。
    static constexpr uint64_t kGoneGraceMs = 3000;      // 异常退出宽限3秒
    static constexpr uint64_t kSelfExitGraceMs = 5000;  // 正常结算等待自行退出5秒
    static constexpr uint64_t kTermGraceMs = 5000;      // SIGTERM后等待5秒

private:
    friend class app::Singleton<ProcessMgr>;
    ProcessMgr() = default;

    /// 分配可用端口，返回0表示无可用端口
    uint16_t AllocatePort(uint32_t requested_port);
    bool IsProcessGroupGone(pid_t pgid) const;
    bool SendSignal(const DSProcess& ds, int signal_value, const char* signal_name) const;
    std::unordered_map<uint64_t, DSProcess>::iterator CleanupDS(std::unordered_map<uint64_t, DSProcess>::iterator it,
                                                                const char* cause);

    uint16_t port_start_ = 20000;
    uint16_t port_end_ = 20099;
    uint16_t next_port_ = 20000;                      // 轮转分配起点，避免刚释放的端口被立即重用
    std::unordered_map<uint64_t, DSProcess> ds_map_;  // room_id -> DSProcess
    std::unordered_map<uint16_t, uint64_t> port_to_room_;  // port -> room_id (快速查端口占用)
    std::unordered_map<uint64_t, std::vector<std::pair<uint64_t, uint64_t>>> auth_map_;  // room_id -> [(gid, token)]
    std::unordered_map<uint64_t, std::map<uint64_t, uint32_t>> role_map_;  // room_id -> {gid -> battle_role_type}
    std::unordered_map<uint64_t, std::map<uint64_t, std::string>> name_map_;  // room_id -> {gid -> display_name}
    std::unordered_map<uint64_t, std::vector<std::pair<std::string, uint32_t>>> bot_map_;
};

}  // namespace dsagent

#endif
