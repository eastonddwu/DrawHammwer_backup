/*
 * * file name: process_mgr.cpp
 * * description: ProcessMgr实现，见process_mgr.h说明
 */

#include "process_mgr.h"
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include "common/clock.h"
#include "core/log.h"
#include "core/rpc_service.h"
#include "core/svr_type.h"
#include "core/transport_type.h"
#include "dsa_app.h"
#include "dsa_rpc_meta.h"
#include "room.pb.h"
#include "room_rpc_meta.h"

namespace dsagent
{

void ProcessMgr::SetPortRange(uint16_t start, uint16_t end)
{
    port_start_ = start;
    port_end_ = end;
}

uint16_t ProcessMgr::AllocatePort(uint32_t requested_port)
{
    if (requested_port > 0)
    {
        // 指定端口
        if (requested_port >= port_start_ && requested_port <= port_end_ &&
            port_to_room_.find(static_cast<uint16_t>(requested_port)) == port_to_room_.end())
        {
            return static_cast<uint16_t>(requested_port);
        }
        APP_LOG_WARN(0, "requested port %u not available, auto-assigning", requested_port);
    }

    // 轮转分配：从next_port_开始找第一个未被占用的端口
    // 避免总是分配port_start_（刚释放的端口可能残留UDP socket，重用会导致新DS bind失败）
    for (uint16_t i = 0; i <= port_end_ - port_start_; ++i)
    {
        uint16_t p = port_start_ + ((next_port_ - port_start_ + i) % (port_end_ - port_start_ + 1));
        if (port_to_room_.find(p) == port_to_room_.end())
        {
            next_port_ = p + 1;
            if (next_port_ > port_end_)
                next_port_ = port_start_;
            return p;
        }
    }
    return 0;  // 无可用端口
}

void ProcessMgr::UpdateAuth(uint64_t room_id, const std::vector<std::pair<uint64_t, uint64_t>>& auth_list)
{
    auth_map_[room_id] = auth_list;
    APP_LOG_INFO(0, "UpdateAuth ok, room_id(%llu), player_count(%zu)", (unsigned long long)room_id, auth_list.size());
}

void ProcessMgr::UpdateRoles(uint64_t room_id, const std::map<uint64_t, uint32_t>& roles)
{
    role_map_[room_id] = roles;
    APP_LOG_INFO(0, "UpdateRoles ok, room_id(%llu), player_count(%zu)", (unsigned long long)room_id, roles.size());
}

void ProcessMgr::UpdateBots(uint64_t room_id, const std::vector<std::pair<std::string, uint32_t>>& bots)
{
    bot_map_[room_id] = bots;
    APP_LOG_INFO(0, "UpdateBots ok, room_id(%llu), bot_count(%zu)", (unsigned long long)room_id, bots.size());
}

void ProcessMgr::UpdateNames(uint64_t room_id, const std::map<uint64_t, std::string>& names)
{
    name_map_[room_id] = names;
    APP_LOG_INFO(0, "UpdateNames ok, room_id(%llu), player_count(%zu)", (unsigned long long)room_id, names.size());
}

std::string ProcessMgr::GetPlayerName(uint64_t gid) const
{
    // 同一gid同时只在一个房间内，遍历房间取首个命中即可（房间数=本机DS并发数，规模很小）
    for (const auto& [room_id, names] : name_map_)
    {
        auto it = names.find(gid);
        if (it != names.end() && !it->second.empty())
            return it->second;
    }
    return std::string();
}

void ProcessMgr::PushAuthToDs(uint64_t room_id)
{
    // UE DS一期不回连dsagent，auth信息通过环境变量传递
    // PushAuthToDs仅更新环境变量（用于后续fork时setenv），已启动的DS不再推送
    auto it = auth_map_.find(room_id);
    if (it == auth_map_.end())
        return;

    APP_LOG_INFO(0, "PushAuthToDs: auth stored for room_id(%llu), player_count(%zu) (UE DS reads from env)",
                 (unsigned long long)room_id, it->second.size());
}

int ProcessMgr::CreateDS(uint64_t room_id, uint64_t battle_generation, uint32_t requested_port, uint32_t map_id)
{
    uint32_t resolved_map_id = map_id;
    std::string resolved_map = "/Game/DrawHammer/Map/LivingRoom/Lvl_LivingRoom_BattleMap";
    if (map_id == 102)
        resolved_map = "/Game/DrawHammer/Map/Pool/Lvl_Pool_BattleMap";
    else if (map_id != 101)
    {
        resolved_map_id = 101;
        APP_LOG_WARN(0, "invalid map_id(%u), fallback to map_id(101)", map_id);
    }
    APP_LOG_INFO(0, "CreateDS map_id(%u), map(%s)", resolved_map_id, resolved_map.c_str());
    // 检查是否已存在
    if (ds_map_.find(room_id) != ds_map_.end())
    {
        APP_LOG_WARN(0, "DS already exists for room_id(%llu)", static_cast<unsigned long long>(room_id));
        return 1;
    }

    // 分配端口
    uint16_t port = AllocatePort(requested_port);
    if (port == 0)
    {
        APP_LOG_ERROR(0, "no available port for room_id(%llu)", static_cast<unsigned long long>(room_id));
        return 2;
    }

    const std::string& ds_exec = DsaApp::GetInst().ds_exec_path();

    // fork DS进程
    pid_t pid = fork();
    if (pid < 0)
    {
        APP_LOG_ERROR(0, "fork failed for room_id(%llu)", static_cast<unsigned long long>(room_id));
        return 3;
    }

    if (pid == 0)
    {
        // 子进程：设置独立进程组，确保kill进程组时shell wrapper + UE DS binary都能收到信号
        setpgid(0, 0);

        // ---- 重定向 stdout/stderr 到独立日志文件 ----
        // 日志路径：../log/ds_<room_id>.log（与dsagent日志同目录）
        char ds_log_path[256];
        snprintf(ds_log_path, sizeof(ds_log_path), "../log/ds_%llu.log", static_cast<unsigned long long>(room_id));
        int log_fd = open(ds_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (log_fd >= 0)
        {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        // 关闭dsagent继承的文件描述符（避免子进程占用父进程资源）
        // 保留 stdin/stdout/stderr (fd 0/1/2)，关闭其他
        for (int fd = 3; fd < 1024; ++fd)
            close(fd);

        // 获取DS可执行文件路径
        std::string ds_path;
        if (ds_exec.find('/') != std::string::npos)
        {
            // 绝对路径或相对路径，直接使用
            ds_path = ds_exec;
        }
        else
        {
            // 仅文件名，从dsagent同目录查找
            char self_path[512] = {};
            ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
            if (len > 0)
            {
                self_path[len] = '\0';
                std::string exe_path(self_path);
                auto last_slash = exe_path.rfind('/');
                if (last_slash != std::string::npos)
                    ds_path = exe_path.substr(0, last_slash + 1) + ds_exec;
                else
                    ds_path = "./" + ds_exec;
            }
            else
            {
                ds_path = "./" + ds_exec;
            }
        }

        // UE DS启动命令（对齐客户端文档 + DrawHammerServer.sh）：
        //   DrawHammerServer.sh -> DrawHammerServer DrawHammer "$@"
        //   实际参数：DrawHammer /Game/DrawHammer/Map/LivingRoom/Lvl_LivingRoom_BattleMap
        //     -log -Port=<port> -RoomID=<room_id> -BattleID=<battle_id>
        //     -DsaHost=<dsa_host> -DsaPort=<dsa_port> -DsBusid=<ds_busid>
        //   注意：UE命令行解析要求 -Key=Value 格式（等号连接），不能用空格分离
        char port_arg[32], room_id_arg[64], battle_id_arg[64], bot_count_arg[32], battle_generation_arg[64];
        char dsa_host_arg[64], dsa_port_arg[32], ds_busid_arg[64];
        snprintf(port_arg, sizeof(port_arg), "-Port=%u", port);
        snprintf(room_id_arg, sizeof(room_id_arg), "-RoomID=%llu", static_cast<unsigned long long>(room_id));
        snprintf(battle_id_arg, sizeof(battle_id_arg), "-BattleID=B_%llu", static_cast<unsigned long long>(room_id));
        snprintf(battle_generation_arg, sizeof(battle_generation_arg), "-BattleGeneration=%llu",
                 static_cast<unsigned long long>(battle_generation));
        snprintf(dsa_host_arg, sizeof(dsa_host_arg), "-DsaHost=%s", DsaApp::GetInst().dsa_host().c_str());
        snprintf(dsa_port_arg, sizeof(dsa_port_arg), "-DsaPort=%u", DsaApp::GetInst().ds_listen_port());
        uint32_t ds_busid = DsaApp::kDsaGroupBase | static_cast<uint32_t>(room_id & 0xFFFF);
        snprintf(ds_busid_arg, sizeof(ds_busid_arg), "-DsBusid=0x%08X", ds_busid);
        auto bot_it = bot_map_.find(room_id);
        size_t bot_count = bot_it == bot_map_.end() ? 0 : bot_it->second.size();
        snprintf(bot_count_arg, sizeof(bot_count_arg), "-BotCount=%zu", bot_count);

        // UE -AbsLog参数：强制指定日志文件绝对路径，修复 DLogBase LogFile=(nil) 导致UE不写文件日志的问题
        // AbsLog=<path> 会在标准 UE FOutputDeviceFile 初始化时被解析，绕过 ini/XDG 路径解析
        char abs_log_arg[768];
        {
            char resolved_dir[512] = {};
            if (realpath("../log", resolved_dir) != nullptr)
            {
                snprintf(abs_log_arg, sizeof(abs_log_arg), "-AbsLog=%s/DrawHammer_%llu.log", resolved_dir,
                         static_cast<unsigned long long>(room_id));
            }
            else
            {
                // realpath失败时用/tmp兜底（极少见）
                snprintf(abs_log_arg, sizeof(abs_log_arg), "-AbsLog=/tmp/DrawHammer_%llu.log",
                         static_cast<unsigned long long>(room_id));
            }
        }

        const std::string& ds_map = resolved_map;

        // 环境变量（UE DS一期不读，仅命令行参数生效）
        char port_val[16], room_id_val[32], battle_generation_val[32];
        snprintf(port_val, sizeof(port_val), "%u", port);
        snprintf(room_id_val, sizeof(room_id_val), "%llu", static_cast<unsigned long long>(room_id));
        snprintf(battle_generation_val, sizeof(battle_generation_val), "%llu",
                 static_cast<unsigned long long>(battle_generation));
        setenv("DS_PORT", port_val, 1);
        setenv("DS_ROOM_ID", room_id_val, 1);
        setenv("DS_BATTLE_GENERATION", battle_generation_val, 1);
        // auth环境变量（一期不鉴权，PreLogin放行所有连接）
        // 格式：gid:token:battle_role_type，逗号分隔（role缺省为0）
        std::string auth_env_val;
        auto auth_it = auth_map_.find(room_id);
        if (auth_it != auth_map_.end())
        {
            auto role_it = role_map_.find(room_id);
            for (const auto& [gid, token] : auth_it->second)
            {
                if (!auth_env_val.empty())
                    auth_env_val += ",";
                uint32_t role = 0;
                if (role_it != role_map_.end())
                {
                    auto rit = role_it->second.find(gid);
                    if (rit != role_it->second.end())
                        role = rit->second;
                }
                auth_env_val += std::to_string(gid) + ":" + std::to_string(token) + ":" + std::to_string(role);
            }
        }
        char auth_env_name[64];
        snprintf(auth_env_name, sizeof(auth_env_name), "DS_AUTH_ROOM_%llu", (unsigned long long)room_id);
        setenv(auth_env_name, auth_env_val.c_str(), 1);

        std::string bot_env_val;
        if (bot_it != bot_map_.end())
        {
            for (const auto& [bot_id, role] : bot_it->second)
            {
                if (!bot_env_val.empty())
                    bot_env_val += ",";
                bot_env_val += bot_id + ":" + std::to_string(role);
            }
        }
        char bot_env_name[64];
        snprintf(bot_env_name, sizeof(bot_env_name), "DS_BOT_INFO_ROOM_%llu", (unsigned long long)room_id);
        setenv(bot_env_name, bot_env_val.c_str(), 1);

        APP_LOG_INFO(0, "exec UE DS: %s DrawHammer %s -log -forcelogflush %s -NULLRHI -nosound %s %s %s %s %s %s %s %s",
                     ds_path.c_str(), ds_map.c_str(), abs_log_arg, port_arg, room_id_arg, battle_id_arg,
                     battle_generation_arg, dsa_host_arg, dsa_port_arg, ds_busid_arg, bot_count_arg);

        // 写入客户端调试信息到DS日志（stderr已重定向到ds_<room_id>.log）
        fprintf(stderr, "\n========== DS START room_id=%llu port=%u ==========\n",
                static_cast<unsigned long long>(room_id), port);
        fprintf(stderr, "启动命令: %s DrawHammer %s -log -forcelogflush %s -NULLRHI -nosound %s %s %s %s %s %s %s %s\n",
                ds_path.c_str(), ds_map.c_str(), abs_log_arg, port_arg, room_id_arg, battle_id_arg,
                battle_generation_arg, dsa_host_arg, dsa_port_arg, ds_busid_arg, bot_count_arg);
        fprintf(stderr, "客户端连接地址: %s:%u (UDP)\n", DsaApp::GetInst().ds_client_ip().c_str(), port);
        // 写入auth信息（一期不鉴权，但记录在日志便于排查）
        auto auth_it2 = auth_map_.find(room_id);
        if (auth_it2 != auth_map_.end())
        {
            fprintf(stderr, "玩家列表:\n");
            for (const auto& [gid, token] : auth_it2->second)
                fprintf(stderr, "  gid=%llu token=%llu\n", (unsigned long long)gid, (unsigned long long)token);
        }
        fprintf(stderr, "========================================\n\n");
        fflush(stderr);

        // UE命令行：DrawHammerServer DrawHammer <map> -log -AbsLog=... -NULLRHI -nosound
        //   -Port=<port> -RoomID=<room_id> -BattleID=<battle_id>
        //   -DsaHost=<dsa_host> -DsaPort=<dsa_port> -DsBusid=<ds_busid>
        // 注意：
        //   argv[1] = "DrawHammer" (UE project identifier, 必须有)
        //   argv[2] = map path (ds_map_)
        //   直接execl UE二进制，不经过shell wrapper，避免双进程问题
        //   （shell wrapper不用exec，导致SIGTERM后shell先死、UE binary成孤儿进程）
        execl(ds_path.c_str(), ds_path.c_str(), "DrawHammer", ds_map.c_str(), "-log", "-forcelogflush", abs_log_arg,
              "-NULLRHI", "-nosound", port_arg, room_id_arg, battle_id_arg, battle_generation_arg, dsa_host_arg,
              dsa_port_arg, ds_busid_arg, bot_count_arg, static_cast<char*>(nullptr));

        // exec失败
        APP_LOG_ERROR(0, "UE DS exec failed: %s", ds_path.c_str());
        _exit(1);
    }

    // 父进程也建立进程组，关闭DestroyDs早于子进程setpgid的竞态。
    int setpgid_ret = 0;
    do
    {
        setpgid_ret = setpgid(pid, pid);
    } while (setpgid_ret != 0 && errno == EINTR);
    if (setpgid_ret != 0 && getpgid(pid) != pid)
    {
        int saved_errno = errno;
        APP_LOG_ERROR(0, "setpgid failed, room_id(%llu), pid(%d), errno(%d)", static_cast<unsigned long long>(room_id),
                      pid, saved_errno);
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        return 4;
    }

    // 父进程：记录DS信息
    DSProcess ds;
    ds.room_id = room_id;
    ds.battle_generation = battle_generation;
    ds.pid = pid;
    ds.pgid = pid;
    ds.port = port;
    ds.state = DS_STATE_STARTING;
    ds.last_heartbeat_ms = app::Clock::GetInst().CurrentMilliSec();  // 记录fork时间，用于启动超时检测

    ds_map_[room_id] = ds;
    port_to_room_[port] = room_id;

    APP_LOG_INFO(0, "DS created, room_id(%llu), pid(%d), port(%u)", static_cast<unsigned long long>(room_id), pid,
                 port);
    return 0;
}

bool ProcessMgr::IsProcessGroupGone(pid_t pgid) const
{
    if (pgid <= 0)
        return true;
    while (kill(-pgid, 0) != 0)
    {
        if (errno == EINTR)
            continue;
        if (errno == ESRCH)
            return true;
        if (errno != EPERM)
            APP_LOG_WARN(0, "probe DS process group fail, pgid(%d), errno(%d)", pgid, errno);
        return false;
    }
    return false;
}

bool ProcessMgr::SendSignal(const DSProcess& ds, int signal_value, const char* signal_name) const
{
    if (ds.pgid <= 0)
        return false;
    while (kill(-ds.pgid, signal_value) != 0)
    {
        if (errno == EINTR)
            continue;
        if (errno == ESRCH)
            return true;
        APP_LOG_WARN(0, "send %s fail, room_id(%llu), generation(%llu), pgid(%d), errno(%d)", signal_name,
                     static_cast<unsigned long long>(ds.room_id), static_cast<unsigned long long>(ds.battle_generation),
                     ds.pgid, errno);
        return false;
    }
    APP_LOG_INFO(0, "sent %s to DS process group, room_id(%llu), generation(%llu), pid(%d), pgid(%d), reason(%u)",
                 signal_name, static_cast<unsigned long long>(ds.room_id),
                 static_cast<unsigned long long>(ds.battle_generation), ds.pid, ds.pgid, ds.destroy_reason);
    return true;
}

std::unordered_map<uint64_t, DSProcess>::iterator ProcessMgr::CleanupDS(
    std::unordered_map<uint64_t, DSProcess>::iterator it, const char* cause)
{
    uint64_t room_id = it->second.room_id;
    pid_t pid = it->second.pid;
    uint16_t port = it->second.port;
    int status = 0;
    waitpid(pid, &status, WNOHANG);
    auth_map_.erase(room_id);
    role_map_.erase(room_id);
    bot_map_.erase(room_id);
    name_map_.erase(room_id);
    port_to_room_.erase(port);
    APP_LOG_INFO(0, "DS mappings cleaned, room_id(%llu), pid(%d), port(%u), cause(%s)",
                 static_cast<unsigned long long>(room_id), pid, port, cause);
    return ds_map_.erase(it);
}

int ProcessMgr::DestroyDS(uint64_t room_id, uint64_t battle_generation, uint32_t reason)
{
    auto it = ds_map_.find(room_id);
    if (it == ds_map_.end())
    {
        APP_LOG_INFO(0, "DestroyDS already cleaned, room_id(%llu)", static_cast<unsigned long long>(room_id));
        return 0;
    }

    DSProcess& ds = it->second;
    if (battle_generation == 0 || ds.battle_generation != battle_generation)
    {
        APP_LOG_WARN(0, "DestroyDS generation mismatch, room_id(%llu), request(%llu), current(%llu)",
                     static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(battle_generation),
                     static_cast<unsigned long long>(ds.battle_generation));
        return 2;
    }

    if (ds.destroy_requested)
    {
        // 更紧急的销毁原因可将“等待自退”升级为SIGTERM，但不延长已有deadline。
        if (ds.state == DS_STATE_WAIT_SELF_EXIT && reason != 0 && reason != 2)
        {
            ds.destroy_reason = reason;
            SendSignal(ds, SIGTERM, "SIGTERM");
            ds.state = DS_STATE_TERM_SENT;
            uint64_t term_deadline = app::Clock::GetInst().CurrentMilliSec() + kTermGraceMs;
            if (term_deadline < ds.stop_deadline_ms)
                ds.stop_deadline_ms = term_deadline;
        }
        return 0;
    }

    ds.destroy_requested = true;
    ds.destroy_reason = reason;
    ds.gone_detect_ms = 0;
    uint64_t now_ms = app::Clock::GetInst().CurrentMilliSec();
    if (reason == 0 || reason == 2)
    {
        ds.state = DS_STATE_WAIT_SELF_EXIT;
        ds.stop_deadline_ms = now_ms + kSelfExitGraceMs;
        APP_LOG_INFO(0, "DestroyDS accepted, wait self exit, room_id(%llu), generation(%llu), pid(%d), deadline(%llu)",
                     static_cast<unsigned long long>(room_id), static_cast<unsigned long long>(battle_generation),
                     ds.pid, static_cast<unsigned long long>(ds.stop_deadline_ms));
    }
    else
    {
        SendSignal(ds, SIGTERM, "SIGTERM");
        ds.state = DS_STATE_TERM_SENT;
        ds.stop_deadline_ms = now_ms + kTermGraceMs;
    }
    return 0;
}

DSProcess* ProcessMgr::GetDS(uint64_t room_id)
{
    auto it = ds_map_.find(room_id);
    if (it == ds_map_.end())
        return nullptr;
    return &it->second;
}

void ProcessMgr::OnHeartBeat(uint64_t room_id)
{
    auto it = ds_map_.find(room_id);
    if (it == ds_map_.end())
        return;

    if (!it->second.destroy_requested)
        it->second.state = DS_STATE_ALIVE;
    it->second.last_heartbeat_ms = app::Clock::GetInst().CurrentMilliSec();
}

void ProcessMgr::OnTick(uint64_t now_ms)
{
    // UE DS一期不回连dsagent TCP、不发心跳。
    // 注意：dsagent的waitpid收不到子进程退出（tapp daemon框架忽略SIGCHLD，子进程被内核自动reap），
    // 因此存活检测不能依赖waitpid，改用kill(pid,0)主动探测：
    //   - kill(pid,0)==0            -> 进程仍在
    //   - kill(pid,0)==-1&&ESRCH    -> 进程已消失（自行RequestExit退出或崩溃）
    // 检测到消失则立即清理映射并通知roomsvr（正常GameFinish走DestroyDS已同步清理，这里主要兜住
    // “UE DS自杀退出/崩溃但未走DestroyDS”的情况）。

    // 先尽力reap僵尸（若SIGCHLD未被忽略则回收；被忽略时返回<=0，无副作用）。
    // 不在此处做清理判定，清理统一由下面的kill(pid,0)探测负责，避免两条路径重复通知。
    {
        int status = 0;
        while (waitpid(-1, &status, WNOHANG) > 0)
        {
        }
    }

    for (auto it = ds_map_.begin(); it != ds_map_.end();)
    {
        auto& ds = it->second;
        bool gone = IsProcessGroupGone(ds.pgid);

        if (ds.destroy_requested)
        {
            if (gone)
            {
                it = CleanupDS(it, "intentional_stop");
                continue;
            }

            if (now_ms >= ds.stop_deadline_ms)
            {
                if (ds.state == DS_STATE_WAIT_SELF_EXIT)
                {
                    SendSignal(ds, SIGTERM, "SIGTERM");
                    ds.state = DS_STATE_TERM_SENT;
                    ds.stop_deadline_ms = now_ms + kTermGraceMs;
                }
                else if (ds.state == DS_STATE_TERM_SENT)
                {
                    SendSignal(ds, SIGKILL, "SIGKILL");
                    ds.state = DS_STATE_KILL_SENT;
                    ds.stop_deadline_ms = now_ms + kTermGraceMs;
                }
                else if (ds.state == DS_STATE_KILL_SENT)
                {
                    APP_LOG_WARN(0, "DS process group still present after SIGKILL, room_id(%llu), pgid(%d)",
                                 static_cast<unsigned long long>(ds.room_id), ds.pgid);
                    SendSignal(ds, SIGKILL, "SIGKILL");
                    ds.stop_deadline_ms = now_ms + kTermGraceMs;
                }
            }
            ++it;
            continue;
        }

        if (!gone)
        {
            ds.gone_detect_ms = 0;
            if (ds.state == DS_STATE_STARTING && (now_ms - ds.last_heartbeat_ms) > kDsStartupTimeoutMs)
            {
                ds.state = DS_STATE_ALIVE;
                APP_LOG_INFO(0, "UE DS startup timeout expired -> mark ALIVE, room_id(%llu), pid(%d)",
                             static_cast<unsigned long long>(ds.room_id), ds.pid);
            }
            ++it;
            continue;
        }

        if (ds.gone_detect_ms == 0)
        {
            ds.gone_detect_ms = now_ms;
            ++it;
            continue;
        }
        if ((now_ms - ds.gone_detect_ms) <= kGoneGraceMs)
        {
            ++it;
            continue;
        }

        APP_LOG_WARN(0, "UE DS process gone abnormally, room_id(%llu), generation(%llu), pid(%d), port(%u)",
                     static_cast<unsigned long long>(ds.room_id), static_cast<unsigned long long>(ds.battle_generation),
                     ds.pid, ds.port);

        char ds_log_path[256];
        snprintf(ds_log_path, sizeof(ds_log_path), "../log/ds_%llu.log", static_cast<unsigned long long>(ds.room_id));
        FILE* log_file = fopen(ds_log_path, "a");
        if (log_file)
        {
            fprintf(log_file, "\n========== DS GONE room_id=%llu pid=%d state=%u ==========\n",
                    static_cast<unsigned long long>(ds.room_id), ds.pid, ds.state);
            fclose(log_file);
        }

        roomsvr::NotifyDsTimeoutReq timeout_req;
        timeout_req.set_room_id(ds.room_id);
        timeout_req.set_pid(ds.pid);
        timeout_req.set_battle_generation(ds.battle_generation);
        uint32_t timeout_cmd = roomsvr::GetRoomMethodCmd("NotifyDsTimeout");
        app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, timeout_cmd, timeout_req, nullptr, nullptr,
                                       app::kGroupAddrRoomSvr, 1000);
        it = CleanupDS(it, "abnormal_exit");
    }
}

}  // namespace dsagent
