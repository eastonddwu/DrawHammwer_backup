/*
 * * file name: dsa_service.cpp
 * * description: DsaService各RPC handler实现
 */

#include "dsa_service.h"
#include "core/log.h"
#include "core/rpc_error.h"
#include "core/rpc_service.h"
#include "core/svr_type.h"
#include "core/transport_type.h"
#include "db_rpc_meta.h"
#include "dbproxy.pb.h"
#include "dsa_app.h"
#include "process_mgr.h"
#include "room.pb.h"
#include "room_rpc_meta.h"

#include <cstdlib>
#include <cstring>
#include <map>

namespace dsagent
{

static uint64_t GenerateRandomToken()
{
    uint64_t hi = static_cast<uint64_t>(rand()) << 32;
    uint64_t lo = static_cast<uint64_t>(rand());
    return hi | lo | 1;  // 确保 non-zero
}

void DsaService::CreateGame(app::RpcContext& context)
{
    const auto& req = static_cast<const roomsvr::CreateGameReq&>(context.GetReq());
    uint64_t room_id = req.room_id();

    APP_LOG_INFO(0, "CreateGame recv, room_id(%llu), ds_port(%u), map_id(%u), player_count(%d)",
                 static_cast<unsigned long long>(room_id), req.ds_port(), req.map_id(), req.players_size());

    auto& rsp = static_cast<roomsvr::CreateGameResp&>(context.GetRsp());

    // 为每个玩家生成独立token，存入内存（不写文件）
    std::vector<std::pair<uint64_t, uint64_t>> auth_list;
    std::map<uint64_t, uint32_t> role_map;
    std::map<uint64_t, std::string> name_map;
    for (int i = 0; i < req.players_size(); ++i)
    {
        uint64_t gid = req.players(i).gid();
        uint64_t token = GenerateRandomToken();
        auth_list.emplace_back(gid, token);
        role_map[gid] = req.players(i).battle_role_type();
        if (!req.players(i).display_name().empty())
            name_map[gid] = req.players(i).display_name();
        APP_LOG_INFO(0, "CreateGame: gid(%llu), token(%llu), battle_role_type(%u), display_name(%s)",
                     static_cast<unsigned long long>(gid), static_cast<unsigned long long>(token),
                     req.players(i).battle_role_type(), req.players(i).display_name().c_str());
    }

    std::vector<std::pair<std::string, uint32_t>> bot_list;
    for (const auto& bot : req.bots())
        bot_list.emplace_back(bot.bot_id(), bot.battle_role_type());
    ProcessMgr::GetInst().UpdateBots(room_id, bot_list);

    // 存储auth到内存（UE DS一期不回连，auth通过环境变量传递）
    if (!auth_list.empty())
        ProcessMgr::GetInst().UpdateAuth(room_id, auth_list);
    if (!role_map.empty())
        ProcessMgr::GetInst().UpdateRoles(room_id, role_map);
    // 缓存房间内昵称：游客不落库，DsGetPlayerInfo查dbproxy会拿到空值，用这份内存数据兜底
    if (!name_map.empty())
        ProcessMgr::GetInst().UpdateNames(room_id, name_map);

    int ret = ProcessMgr::GetInst().CreateDS(room_id, req.battle_generation(), req.ds_port(), req.map_id());
    if (ret != 0)
    {
        APP_LOG_WARN(0, "CreateGame CreateDS fail, room_id(%llu), ret(%d)", static_cast<unsigned long long>(room_id),
                     ret);
        ProcessMgr::GetInst().UpdateBots(room_id, {});
        rsp.set_ret_code(ret);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 获取刚创建的DS信息
    DSProcess* ds = ProcessMgr::GetInst().GetDS(room_id);
    if (!ds)
    {
        rsp.set_ret_code(-1);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    rsp.set_ret_code(0);
    rsp.set_dsa_svr_id(context.head.dst);  // 本dsagent的busid
    rsp.mutable_ds_conn_info()->set_ip(DsaApp::GetInst().ds_client_ip());
    rsp.mutable_ds_conn_info()->set_port(ds->port);

    // 附带 player_auth_list
    for (const auto& [gid, token] : auth_list)
    {
        auto* auth = rsp.add_player_auth_list();
        auth->set_gid(gid);
        auth->set_token(token);
    }

    context.ret_code = app::RPC_SUCCESS;

    // 客户端关键调试信息：推给客户端的 DS 地址和每个玩家的 token
    std::string ds_addr = DsaApp::GetInst().ds_client_ip() + ":" + std::to_string(ds->port);
    APP_LOG_INFO(0, "CreateGame ok, room_id(%llu), ds_addr(%s), pid(%d)", static_cast<unsigned long long>(room_id),
                 ds_addr.c_str(), ds->pid);
    for (const auto& [gid, token] : auth_list)
    {
        APP_LOG_INFO(0, "CreateGame: client_info gid(%llu) -> DS(%s) token(%llu)", static_cast<unsigned long long>(gid),
                     ds_addr.c_str(), static_cast<unsigned long long>(token));
    }

    // 通知roomsvr DS已就绪（fork后立即通知，UE DS不回连dsagent）
    roomsvr::NotifyDsStartedReq ntf_req;
    ntf_req.set_room_id(room_id);
    ntf_req.set_battle_generation(req.battle_generation());
    ntf_req.mutable_ds_conn_info()->set_ip(DsaApp::GetInst().ds_client_ip());
    ntf_req.mutable_ds_conn_info()->set_port(ds->port);

    // 附带 player_auth_list
    for (const auto& [gid, token] : auth_list)
    {
        auto* auth = ntf_req.add_player_auth_list();
        auth->set_gid(gid);
        auth->set_token(token);
    }

    uint32_t ntf_cmd = roomsvr::GetRoomMethodCmd("NotifyDsStarted");
    app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, ntf_cmd, ntf_req, nullptr, nullptr,
                                   app::kGroupAddrRoomSvr, 1000);

    APP_LOG_INFO(0, "NotifyDsStarted sent, room_id(%llu), ds_addr(%s)", static_cast<unsigned long long>(room_id),
                 ds_addr.c_str());
}

void DsaService::DestroyDs(app::RpcContext& context)
{
    const auto& req = static_cast<const roomsvr::DestroyDsReq&>(context.GetReq());
    uint64_t room_id = req.room_id();

    APP_LOG_INFO(0, "DestroyDs recv, room_id(%llu), reason(%u)", static_cast<unsigned long long>(room_id),
                 req.reason());

    auto& rsp = static_cast<roomsvr::DestroyDsResp&>(context.GetRsp());

    int ret = ProcessMgr::GetInst().DestroyDS(room_id, req.battle_generation(), req.reason());
    if (ret != 0)
    {
        APP_LOG_WARN(0, "DestroyDs fail, room_id(%llu), ret(%d)", static_cast<unsigned long long>(room_id), ret);
        rsp.set_ret_code(ret);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void DsaService::DsHeartBeat(app::RpcContext& context)
{
    const auto& req = static_cast<const roomsvr::DsHeartBeatReq&>(context.GetReq());
    uint64_t room_id = req.room_id();

    ProcessMgr::GetInst().OnHeartBeat(room_id);

    // 每次心跳推送auth信息（保证首次连接、重连、SetDsAuth后都能及时送达）
    ProcessMgr::GetInst().PushAuthToDs(room_id);

    auto& rsp = static_cast<roomsvr::DsHeartBeatResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void DsaService::SetDsAuth(app::RpcContext& context)
{
    const auto& req = static_cast<const roomsvr::SetDsAuthReq&>(context.GetReq());
    uint64_t room_id = req.room_id();

    APP_LOG_INFO(0, "SetDsAuth recv, room_id(%llu), player_count(%d)", static_cast<unsigned long long>(room_id),
                 req.player_auth_list_size());

    std::vector<std::pair<uint64_t, uint64_t>> auth_list;
    for (const auto& auth : req.player_auth_list())
        auth_list.emplace_back(auth.gid(), auth.token());

    // 更新内存中的auth（UE DS一期不回连，auth通过env传递）
    ProcessMgr::GetInst().UpdateAuth(room_id, auth_list);
    ProcessMgr::GetInst().PushAuthToDs(room_id);

    auto& rsp = static_cast<roomsvr::SetDsAuthResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

// ============================================================
// DS结算代理（转发DS→roomsvr）
// ============================================================

void DsaService::RoomDsPlayerSettleProxy(app::RpcContext& context)
{
    const auto& req = static_cast<const roomsvr::RoomDsPlayerSettleReq&>(context.GetReq());
    uint64_t gid = req.player_info().gid();
    auto& rsp = static_cast<roomsvr::RoomDsPlayerSettleResp&>(context.GetRsp());

    if (context.index != app::TRANSPORT_DS_TCP)
    {
        APP_LOG_WARN(gid, "RoomDsPlayerSettle rejected non-DS transport(%u)", context.index);
        rsp.set_ret_code(-3);
        rsp.set_gid(gid);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    DSProcess* ds = ProcessMgr::GetInst().GetDS(req.room_id());
    if (!ds)
    {
        APP_LOG_WARN(gid, "RoomDsPlayerSettle: DS not found, room_id(%llu)",
                     static_cast<unsigned long long>(req.room_id()));
        rsp.set_ret_code(-1);
        rsp.set_gid(gid);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    roomsvr::RoomDsPlayerSettleReq forward_req = req;
    if (forward_req.battle_generation() == 0)
        forward_req.set_battle_generation(ds->battle_generation);  // 兼容旧DS，仅限当前DS TCP连接
    else if (forward_req.battle_generation() != ds->battle_generation)
    {
        APP_LOG_WARN(gid, "RoomDsPlayerSettle generation mismatch, room_id(%llu), request(%llu), current(%llu)",
                     static_cast<unsigned long long>(req.room_id()),
                     static_cast<unsigned long long>(req.battle_generation()),
                     static_cast<unsigned long long>(ds->battle_generation));
        rsp.set_ret_code(-2);
        rsp.set_gid(gid);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    APP_LOG_INFO(gid, "RoomDsPlayerSettle proxy: room_id(%llu), generation(%llu), gid(%llu), is_bot(%d), bot_id(%s)",
                 static_cast<unsigned long long>(req.room_id()),
                 static_cast<unsigned long long>(forward_req.battle_generation()), static_cast<unsigned long long>(gid),
                 req.player_info().b_is_bot(), req.player_info().bot_id().c_str());

    roomsvr::RoomDsPlayerSettleResp room_rsp;
    uint32_t settle_cmd = roomsvr::GetRoomMethodCmd("RoomDsPlayerSettle");
    int32_t ret = app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, gid, settle_cmd, forward_req, &room_rsp,
                                                 nullptr, app::kGroupAddrRoomSvr, 3000);

    if (ret == app::RPC_SUCCESS)
    {
        rsp.set_ret_code(room_rsp.ret_code());
        rsp.set_gid(room_rsp.gid());
    }
    else
    {
        rsp.set_ret_code(-1);
        rsp.set_gid(gid);
        APP_LOG_WARN(gid, "RoomDsPlayerSettle proxy fail, roomsvr ret(%d)", ret);
    }
    context.ret_code = app::RPC_SUCCESS;
}

void DsaService::RoomDsGameFinishProxy(app::RpcContext& context)
{
    const auto& req = static_cast<const roomsvr::RoomDsGameFinishReq&>(context.GetReq());
    auto& rsp = static_cast<roomsvr::RoomDsGameFinishResp&>(context.GetRsp());

    if (context.index != app::TRANSPORT_DS_TCP)
    {
        APP_LOG_WARN(0, "RoomDsGameFinish rejected non-DS transport(%u)", context.index);
        rsp.set_ret_code(-3);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    DSProcess* ds = ProcessMgr::GetInst().GetDS(req.room_id());
    if (!ds)
    {
        APP_LOG_WARN(0, "RoomDsGameFinish: DS not found, room_id(%llu)",
                     static_cast<unsigned long long>(req.room_id()));
        rsp.set_ret_code(-1);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    roomsvr::RoomDsGameFinishReq forward_req = req;
    if (forward_req.battle_generation() == 0)
        forward_req.set_battle_generation(ds->battle_generation);  // 兼容旧DS，仅限当前DS TCP连接
    else if (forward_req.battle_generation() != ds->battle_generation)
    {
        APP_LOG_WARN(0, "RoomDsGameFinish generation mismatch, room_id(%llu), request(%llu), current(%llu)",
                     static_cast<unsigned long long>(req.room_id()),
                     static_cast<unsigned long long>(req.battle_generation()),
                     static_cast<unsigned long long>(ds->battle_generation));
        rsp.set_ret_code(-2);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    APP_LOG_INFO(0, "RoomDsGameFinish proxy: room_id(%llu), generation(%llu), duration_sec(%u), end_reason(%u)",
                 static_cast<unsigned long long>(req.room_id()),
                 static_cast<unsigned long long>(forward_req.battle_generation()), req.duration_sec(),
                 req.end_reason());

    roomsvr::RoomDsGameFinishResp room_rsp;
    uint32_t finish_cmd = roomsvr::GetRoomMethodCmd("RoomDsGameFinish");
    int32_t ret = app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, 0, finish_cmd, forward_req, &room_rsp,
                                                 nullptr, app::kGroupAddrRoomSvr, 3000);

    if (ret == app::RPC_SUCCESS)
        rsp.set_ret_code(room_rsp.ret_code());
    else
    {
        rsp.set_ret_code(-1);
        APP_LOG_WARN(0, "RoomDsGameFinish proxy fail, roomsvr ret(%d)", ret);
    }
    context.ret_code = app::RPC_SUCCESS;
}

// ============================================================
// DS查询玩家信息代理（转发DS→dbproxy）
// ============================================================

void DsaService::DsGetPlayerInfoProxy(app::RpcContext& context)
{
    const auto& req = static_cast<const roomsvr::DsGetPlayerInfoReq&>(context.GetReq());
    uint64_t room_id = req.room_id();

    APP_LOG_INFO(0, "DsGetPlayerInfo proxy: room_id(%llu), gid_count(%d)", static_cast<unsigned long long>(room_id),
                 req.gid_list_size());

    auto& rsp = static_cast<roomsvr::DsGetPlayerInfoResp&>(context.GetRsp());

    // 空 gid_list：直接返回
    if (req.gid_list_size() == 0)
    {
        rsp.set_ret_code(0);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // gid_list 超限保护
    if (req.gid_list_size() > 50)
    {
        APP_LOG_WARN(0, "DsGetPlayerInfo proxy: gid_list too large(%d), room_id(%llu)", req.gid_list_size(),
                     static_cast<unsigned long long>(room_id));
        rsp.set_ret_code(-2);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    rsp.set_ret_code(0);

    uint32_t get_cmd = dbproxy::GetDBMethodCmd("CommonGetData");

    // 逐个 coroutine RPC 查询 dbproxy（顺序但非阻塞）
    for (int i = 0; i < req.gid_list_size(); ++i)
    {
        uint64_t gid = req.gid_list(i);

        app::protocol::CommonGetDataReq get_req;
        get_req.mutable_key()->set_first(gid);
        get_req.set_table_name("user_info");

        app::protocol::CommonGetDataResp get_rsp;
        int32_t ret = app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, gid, get_cmd, get_req, &get_rsp, nullptr,
                                                     app::kGroupAddrDBProxy, 1000);

        auto* player_info = rsp.add_player_list();
        player_info->set_gid(gid);

        if (ret == app::RPC_SUCCESS && get_rsp.data().size() > 0)
        {
            // 解析 user_info binary: is_new(4B) + role_type(4B) + name_len(4B) + name(NB) + points(8B)
            const auto& data = get_rsp.data();
            size_t offset = 0;

            // skip is_new (4B) — DS不需要
            offset += sizeof(uint32_t);

            if (data.size() >= offset + sizeof(uint32_t))
            {
                uint32_t role_type = 0;
                memcpy(&role_type, data.data() + offset, sizeof(uint32_t));
                player_info->set_role_type(role_type);
                offset += sizeof(uint32_t);
            }

            uint32_t name_len = 0;
            if (data.size() >= offset + sizeof(uint32_t))
            {
                memcpy(&name_len, data.data() + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);
            }

            if (name_len > 0 && data.size() >= offset + name_len)
            {
                std::string display_name;
                display_name.assign(data.data() + offset, name_len);
                player_info->set_display_name(display_name);
                offset += name_len;
            }

            if (data.size() >= offset + sizeof(uint64_t))
            {
                uint64_t points = 0;
                memcpy(&points, data.data() + offset, sizeof(uint64_t));
                player_info->set_points(points);
            }

            player_info->set_ret_code(0);
            APP_LOG_INFO(gid, "DsGetPlayerInfo ok, gid(%llu), display_name(%s), role_type(%u), points(%llu)",
                         static_cast<unsigned long long>(gid), player_info->display_name().c_str(),
                         player_info->role_type(), static_cast<unsigned long long>(player_info->points()));
        }
        else
        {
            player_info->set_ret_code(-1);
            APP_LOG_WARN(gid, "DsGetPlayerInfo dbproxy fail for gid(%llu), ret(%d), data_size(%zu)",
                         static_cast<unsigned long long>(gid), ret, get_rsp.data().size());
        }

        // 昵称兜底：游客不写tcaplus，查库必然拿不到user_name。
        // 用CreateGame时roomsvr带过来的房间内昵称补上，避免DS/战绩只显示裸gid。
        if (player_info->display_name().empty())
        {
            std::string cached_name = ProcessMgr::GetInst().GetPlayerName(gid);
            if (!cached_name.empty())
            {
                player_info->set_display_name(cached_name);
                player_info->set_ret_code(0);
                APP_LOG_INFO(gid, "DsGetPlayerInfo fallback to room name, gid(%llu), display_name(%s)",
                             static_cast<unsigned long long>(gid), cached_name.c_str());
            }
        }
    }

    // 全局 ret_code: 全失败=-1，至少部分成功=0
    bool any_success = false;
    for (const auto& p : rsp.player_list())
    {
        if (p.ret_code() == 0)
        {
            any_success = true;
            break;
        }
    }
    if (!any_success && rsp.player_list_size() > 0)
        rsp.set_ret_code(-1);

    context.ret_code = app::RPC_SUCCESS;
}

}  // namespace dsagent
