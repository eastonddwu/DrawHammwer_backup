/*
 * * file name: conn_service.cpp
 * * description: ConnService各RPC handler实现
 * *              Login流程: 提取openid → 调用rolesvr.Login → 返回结果给客户端。
 * *              房间操作: 转发到roomsvr，透传请求/响应。
 * *              推送: 接收roomsvr推送，通过tconnd转发给客户端。
 * *              user_info的读写逻辑在rolesvr中，connsvr只做openid提取和转发。
 * */

#include "conn_service.h"
#include "conn.pb.h"
#include "conn_app.h"
#include "conn_constants.h"
#include "conn_rpc_meta.h"
#include "common/text_util.h"
#include "core/log.h"
#include "core/pkg_flag.h"
#include "core/rpc_error.h"
#include "core/rpc_service.h"
#include "core/svr_type.h"
#include "core/transport_type.h"
#include "net/client_cmd_id.h"
#include "net/pkg_framing.h"
#include "net/tconnd_channel.h"
#include "pkg_head.pb.h"
#include "role.pb.h"
#include "role_rpc_meta.h"
#include "room.pb.h"
#include "room_error.h"
#include "room_rpc_meta.h"

#include <cctype>
#include <string>
#include <unordered_map>

namespace connsvr
{

namespace
{

static bool ContainsSensitiveWord(const std::string& /*s*/)
{
    // 当前代码库无现成敏感词库，此处保留扩展位。
    return false;
}

}  // namespace

// 房间操作转发的通用响应映射：所有Room*Rsp都含 int32 code + string message。
// rpc_ret：转发RPC本身的返回码；room_ret_code：roomsvr业务返回码；fail_msg：业务失败时的提示文案。
// 无论后端是否可达，connsvr对客户端一律返回RPC_SUCCESS（错误通过rsp.code体现）。
template <typename RspT>
static void FillRoomForwardRsp(app::RpcContext& context, RspT& rsp, int32_t rpc_ret, int32_t room_ret_code,
                               const char* fail_msg)
{
    if (rpc_ret == app::RPC_SUCCESS)
    {
        rsp.set_code(room_ret_code);
        if (room_ret_code != 0)
            rsp.set_message(fail_msg);
    }
    else
    {
        rsp.set_code(kCodeBackendUnreachable);
        rsp.set_message("roomsvr unreachable");
    }
    context.ret_code = app::RPC_SUCCESS;
}

// ============================================================
// 登录/注册
// ============================================================

void ConnService::Login(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const connsvr::LoginReq&>(context.GetReq());
    auto& app = ConnApp::GetInst();
    auto& rsp = dynamic_cast<connsvr::LoginResp&>(context.GetRsp());

    const bool from_tconnd = (context.head.pkg_flag & app::FLAG_FROM_TCONND) != 0;
    int32_t session_id = 0;
    if (from_tconnd)
    {
        session_id = static_cast<int32_t>(context.head.src);
        if (app.HasSessionIdentity(session_id))
        {
            rsp.set_ret_code(kCodeDuplicateLogin);
            rsp.set_gid(0);
            context.head.gid = 0;
            context.ret_code = app::RPC_SUCCESS;
            return;
        }
    }

    uint64_t gopenid = 0;
    if (from_tconnd)
        gopenid = app.GetTconndChannel().GetSessionOpenid(session_id);
    if (gopenid == 0)
        gopenid = static_cast<uint64_t>(req.gopenid());

    uint64_t gid = gopenid;

    APP_LOG_INFO(gid, "Login recv, src(%u), gopenid(%llu), from_tconnd(%d), forwarding to rolesvr",
                 context.head.src, static_cast<unsigned long long>(gopenid), from_tconnd ? 1 : 0);

    rolesvr::LoginReq role_req;
    role_req.set_gid(gid);
    role_req.set_gopenid(gopenid);
    if (from_tconnd)
        role_req.set_session_id(static_cast<uint32_t>(context.head.src));

    rolesvr::LoginResp role_rsp;
    uint32_t role_cmd = rolesvr::GetRoleMethodCmd("Login");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, role_cmd, role_req, &role_rsp, nullptr,
        app::kGroupAddrRoleSvr, kForwardTimeoutMs);

    context.head.gid = gid;

    if (ret == app::RPC_SUCCESS)
    {
        rsp.set_ret_code(0);
        rsp.set_gid(role_rsp.gid());
        rsp.set_is_new(role_rsp.is_new());
        rsp.set_role_type(role_rsp.role_type());
        rsp.set_user_name(role_rsp.user_name());
        rsp.set_points(role_rsp.points());
        app.SetGidUserName(gid, role_rsp.user_name());
        context.ret_code = app::RPC_SUCCESS;

        if (role_rsp.is_new())
            context.head.cmd = app::CMD_LOGIN_NEW;

        if (from_tconnd)
        {
            if (!app.BindSessionIdentity(session_id, gid, false))
            {
                rsp.set_ret_code(kCodeDuplicateLogin);
                rsp.set_gid(0);
                context.head.gid = 0;
                context.ret_code = app::RPC_SUCCESS;
                return;
            }
            app.SetGidSession(gid, session_id);
        }

        APP_LOG_INFO(gid, "Login ok, gid(%llu) is_new(%d) role_type(%u) user_name(\"%s\") points(%llu), "
                         "ClientHeader.gid will be %llu (=openid)",
                     static_cast<unsigned long long>(gid), rsp.is_new(), rsp.role_type(),
                     rsp.user_name().c_str(), static_cast<unsigned long long>(rsp.points()),
                     static_cast<unsigned long long>(gid));
    }
    else
    {
        APP_LOG_WARN(gid, "Login rolesvr unreachable, ret(%d), returning fallback", ret);
        rsp.set_ret_code(0);
        rsp.set_gid(gid);
        rsp.set_is_new(true);
        context.ret_code = app::RPC_SUCCESS;
        context.head.cmd = app::CMD_LOGIN_NEW;

        if (from_tconnd)
        {
            if (!app.BindSessionIdentity(session_id, gid, false))
            {
                rsp.set_ret_code(kCodeDuplicateLogin);
                rsp.set_gid(0);
                context.head.gid = 0;
                context.ret_code = app::RPC_SUCCESS;
                return;
            }
            app.SetGidSession(gid, session_id);
        }
    }
}

void ConnService::GuestLogin(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const connsvr::GuestLoginReq&>(context.GetReq());
    auto& app = ConnApp::GetInst();
    auto& rsp = dynamic_cast<connsvr::GuestLoginResp&>(context.GetRsp());

    const bool from_tconnd = (context.head.pkg_flag & app::FLAG_FROM_TCONND) != 0;
    int32_t session_id = 0;
    if (from_tconnd)
    {
        session_id = static_cast<int32_t>(context.head.src);
        if (app.HasSessionIdentity(session_id))
        {
            rsp.set_ret_code(kCodeDuplicateLogin);
            rsp.set_gid(0);
            context.head.gid = 0;
            context.ret_code = app::RPC_SUCCESS;
            return;
        }
    }

    std::string user_name;
    if (!app::text::ValidateLength(req.guest_name(), app::text::kMaxUserNameLen, &user_name))
    {
        rsp.set_ret_code(kCodeGuestInvalidName);
        rsp.set_gid(0);
        rsp.set_user_name("");
        rsp.set_is_guest(false);
        context.head.gid = 0;
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (ContainsSensitiveWord(user_name))
    {
        rsp.set_ret_code(kCodeGuestSensitiveName);
        rsp.set_gid(0);
        rsp.set_user_name("");
        rsp.set_is_guest(false);
        context.head.gid = 0;
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (!app.CanAcceptMoreGuests())
    {
        rsp.set_ret_code(kCodeGuestLimitReached);
        rsp.set_gid(0);
        rsp.set_user_name("");
        rsp.set_is_guest(false);
        context.head.gid = 0;
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (from_tconnd && !app.CheckAndConsumeGuestLoginRate(session_id))
    {
        rsp.set_ret_code(kCodeGuestRateLimited);
        rsp.set_gid(0);
        rsp.set_user_name("");
        rsp.set_is_guest(false);
        context.head.gid = 0;
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    uint64_t gid = 0;
    if (!app.AllocateGuestGid(gid))
    {
        rsp.set_ret_code(kCodeGuestLimitReached);
        rsp.set_gid(0);
        rsp.set_user_name("");
        rsp.set_is_guest(false);
        context.head.gid = 0;
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    if (from_tconnd && !app.BindSessionIdentity(session_id, gid, true))
    {
        rsp.set_ret_code(kCodeDuplicateLogin);
        rsp.set_gid(0);
        rsp.set_user_name("");
        rsp.set_is_guest(false);
        context.head.gid = 0;
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    context.head.gid = gid;
    if (from_tconnd)
        app.SetGidSession(gid, session_id);
    app.SetGidUserName(gid, user_name);

    rsp.set_ret_code(0);
    rsp.set_gid(gid);
    rsp.set_user_name(user_name);
    rsp.set_is_guest(true);
    context.ret_code = app::RPC_SUCCESS;

    APP_LOG_INFO(gid, "GuestLogin ok, gid(%llu), user_name(\"%s\")", static_cast<unsigned long long>(gid),
                 user_name.c_str());
}

void ConnService::SetUserInfo(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const connsvr::SetUserInfoReq&>(context.GetReq());
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "SetUserInfo recv, gid(%llu), user_name(\"%s\"), role_type(%u)",
                 static_cast<unsigned long long>(gid), req.user_name().c_str(), req.role_type());

    auto& rsp = dynamic_cast<connsvr::SetUserInfoResp&>(context.GetRsp());
    if (ConnApp::GetInst().IsGuestGid(gid))
    {
        rsp.set_gid(gid);
        rsp.set_ret_code(kCodeGuestSetUserInfoForbidden);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    // 昵称闸门：trim后须1~8个Unicode码点，超长直接拒绝，不截断也不落库
    std::string user_name;
    if (!app::text::ValidateLength(req.user_name(), app::text::kMaxUserNameLen, &user_name))
    {
        APP_LOG_INFO(gid, "SetUserInfo reject, invalid user_name(\"%s\")", req.user_name().c_str());
        rsp.set_gid(gid);
        rsp.set_ret_code(kCodeInvalidUserName);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    if (ContainsSensitiveWord(user_name))
    {
        rsp.set_gid(gid);
        rsp.set_ret_code(kCodeGuestSensitiveName);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    rolesvr::SetUserInfoReq role_req;
    role_req.set_gid(gid);
    role_req.set_user_name(user_name);
    role_req.set_role_type(req.role_type());

    rolesvr::SetUserInfoResp role_rsp;
    uint32_t role_cmd = rolesvr::GetRoleMethodCmd("SetUserInfo");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, role_cmd, role_req, &role_rsp, nullptr,
        app::kGroupAddrRoleSvr, kForwardTimeoutMs);

    if (ret == app::RPC_SUCCESS)
    {
        rsp.set_gid(role_rsp.gid());
        rsp.set_user_name(role_rsp.user_name());
        rsp.set_role_type(role_rsp.role_type());
        rsp.set_ret_code(role_rsp.ret_code());
        if (role_rsp.ret_code() == 0)
        {
            ConnApp::GetInst().SetGidUserName(gid, role_rsp.user_name());
            // 若该玩家在房间内，同步刷新成员display_name与（其为房主时的）host_display_name；房间名不动
            roomsvr::UpdateMemberNameReq room_req;
            room_req.set_gid(gid);
            room_req.set_display_name(role_rsp.user_name());
            roomsvr::UpdateMemberNameResp room_rsp;
            uint32_t room_cmd = roomsvr::GetRoomMethodCmd("UpdateMemberName");
            int32_t room_ret = app::RpcService::GetInst().Rpc(
                app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
                app::kGroupAddrRoomSvr, kForwardTimeoutMs);
            if (room_ret != app::RPC_SUCCESS)
                APP_LOG_WARN(gid, "UpdateMemberName to roomsvr failed, ret(%d)", room_ret);
        }
        context.ret_code = app::RPC_SUCCESS;
        APP_LOG_INFO(gid, "SetUserInfo ok, gid(%llu), user_name(\"%s\"), role_type(%u)",
                     static_cast<unsigned long long>(gid), rsp.user_name().c_str(), rsp.role_type());
    }
    else
    {
        rsp.set_gid(gid);
        rsp.set_ret_code(ret);
        context.ret_code = app::RPC_SUCCESS;
        APP_LOG_WARN(gid, "SetUserInfo rolesvr unreachable, ret(%d)", ret);
    }
}

// ============================================================
// 房间列表查询
// ============================================================

void ConnService::RoomList(app::RpcContext& context)
{
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "RoomList recv, gid(%llu)", static_cast<unsigned long long>(gid));

    // 向roomsvr查询房间列表
    roomsvr::QueryRoomListReq room_req;
    roomsvr::QueryRoomListResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("QueryRoomList");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomListRsp&>(context.GetRsp());
    if (ret == app::RPC_SUCCESS)
    {
        rsp.set_code(0);
        auto* snapshot = rsp.mutable_snapshot();
        for (const auto& r : room_rsp.rooms())
        {
            auto* brief = snapshot->add_rooms();
            brief->set_room_id(std::to_string(r.room_id()));
            brief->set_room_name(r.room_name());
            brief->set_host_gid(std::to_string(r.host_gid()));
            brief->set_current_players(r.current_players());
            brief->set_max_players(r.max_players());
            brief->set_in_battle(r.in_battle());
            brief->set_map_id(r.map_id());
            brief->set_host_display_name(r.host_display_name());
        }
        context.ret_code = app::RPC_SUCCESS;
    }
    else
    {
        rsp.set_code(kCodeBackendUnreachable);
        context.ret_code = app::RPC_SUCCESS;
        APP_LOG_WARN(gid, "RoomList roomsvr unreachable, ret(%d)", ret);
    }
}

// ============================================================
// 房间操作（代理到roomsvr）
// ============================================================

void ConnService::RoomCreate(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const RoomCreateReq&>(context.GetReq());
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "RoomCreate forwarding to roomsvr, gid(%llu), room_name(%s), max_players(%d)",
                 static_cast<unsigned long long>(gid), req.room_name().c_str(), req.max_players());

    roomsvr::CreateRoomReq room_req;
    room_req.set_gid(gid);
    room_req.set_room_name(req.room_name());
    if (req.max_players() < 0 || req.max_players() > 8)
    {
        auto& rsp = dynamic_cast<RoomCreateRsp&>(context.GetRsp());
        rsp.set_code(roomsvr::kInvalidMaxPlayers);
        rsp.set_message("max_players must be between 1 and 8");
        context.ret_code = app::RPC_SUCCESS;
        return;
    }
    room_req.set_max_players(static_cast<uint32_t>(req.max_players()));
    room_req.set_map_id(req.map_id());
    room_req.set_display_name(ConnApp::GetInst().GetGidUserName(gid));

    roomsvr::CreateRoomResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("CreateRoom");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomCreateRsp&>(context.GetRsp());
    if (ret == app::RPC_SUCCESS)
    {
        if (room_rsp.ret_code() == 0)
        {
            rsp.set_code(0);
            rsp.set_room_id(std::to_string(room_rsp.room_id()));
        }
        else
        {
            rsp.set_code(room_rsp.ret_code());
            rsp.set_message("create room failed");
        }
        context.ret_code = app::RPC_SUCCESS;
    }
    else
    {
        rsp.set_code(kCodeBackendUnreachable);
        rsp.set_message("roomsvr unreachable");
        context.ret_code = app::RPC_SUCCESS;
        APP_LOG_WARN(gid, "RoomCreate roomsvr unreachable, ret(%d)", ret);
    }
}

void ConnService::RoomJoin(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const RoomJoinReq&>(context.GetReq());
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "RoomJoin forwarding to roomsvr, gid(%llu), room_id(%s)",
                 static_cast<unsigned long long>(gid), req.room_id().c_str());

    roomsvr::JoinRoomReq room_req;
    uint64_t room_id = 0;
    try { room_id = std::stoull(req.room_id()); } catch (...) {}
    room_req.set_room_id(room_id);
    room_req.set_gid(gid);
    room_req.set_display_name(ConnApp::GetInst().GetGidUserName(gid));

    roomsvr::JoinRoomResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("JoinRoom");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomJoinRsp&>(context.GetRsp());
    FillRoomForwardRsp(context, rsp, ret, room_rsp.ret_code(), "join room failed");
}

void ConnService::RoomLeave(app::RpcContext& context)
{
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "RoomLeave forwarding to roomsvr, gid(%llu)", static_cast<unsigned long long>(gid));

    roomsvr::LeaveRoomReq room_req;
    room_req.set_gid(gid);
    // room_id 需要从gid→room映射获取，先设0让roomsvr自行查找
    room_req.set_room_id(0);

    roomsvr::LeaveRoomResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("LeaveRoom");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomLeaveRsp&>(context.GetRsp());
    FillRoomForwardRsp(context, rsp, ret, room_rsp.ret_code(), "leave room failed");
}

void ConnService::RoomSetReady(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const RoomSetReadyReq&>(context.GetReq());
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "RoomSetReady forwarding to roomsvr, gid(%llu), b_ready(%d)",
                 static_cast<unsigned long long>(gid), req.b_ready());

    roomsvr::SetReadyReq room_req;
    room_req.set_gid(gid);
    room_req.set_room_id(0);
    room_req.set_b_ready(req.b_ready());

    roomsvr::SetReadyResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("SetReady");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomSetReadyRsp&>(context.GetRsp());
    FillRoomForwardRsp(context, rsp, ret, room_rsp.ret_code(), "set ready failed");
}

void ConnService::RoomAddBot(app::RpcContext& context)
{
    uint64_t gid = context.head.gid;
    roomsvr::AddBotReq room_req;
    room_req.set_gid(gid);
    room_req.set_room_id(0);

    roomsvr::AddBotResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("AddBot");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomAddBotRsp&>(context.GetRsp());
    if (ret == app::RPC_SUCCESS)
    {
        rsp.set_code(room_rsp.ret_code());
        rsp.set_bot_id(room_rsp.bot_id());
        rsp.set_slot_index(room_rsp.slot_index());
        if (room_rsp.ret_code() != 0)
            rsp.set_message("add bot failed");
    }
    else
    {
        rsp.set_code(kCodeBackendUnreachable);
        rsp.set_message("roomsvr unreachable");
        APP_LOG_WARN(gid, "RoomAddBot roomsvr unreachable, ret(%d)", ret);
    }
    context.ret_code = app::RPC_SUCCESS;
}

void ConnService::RoomRemoveBot(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const RoomRemoveBotReq&>(context.GetReq());
    uint64_t gid = context.head.gid;
    roomsvr::RemoveBotReq room_req;
    room_req.set_gid(gid);
    room_req.set_room_id(0);
    room_req.set_bot_id(req.bot_id());
    room_req.set_slot_index(req.slot_index());

    roomsvr::RemoveBotResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("RemoveBot");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomRemoveBotRsp&>(context.GetRsp());
    if (ret == app::RPC_SUCCESS)
    {
        rsp.set_code(room_rsp.ret_code());
        if (room_rsp.ret_code() != 0)
            rsp.set_message("remove bot failed");
    }
    else
    {
        rsp.set_code(kCodeBackendUnreachable);
        rsp.set_message("roomsvr unreachable");
        APP_LOG_WARN(gid, "RoomRemoveBot roomsvr unreachable, ret(%d)", ret);
    }
    context.ret_code = app::RPC_SUCCESS;
}

void ConnService::RoomStartBattle(app::RpcContext& context)
{
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "RoomStartBattle forwarding to roomsvr, gid(%llu)", static_cast<unsigned long long>(gid));

    roomsvr::StartBattleReq room_req;
    room_req.set_gid(gid);
    room_req.set_room_id(0);

    roomsvr::StartBattleResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("StartBattle");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kStartBattleTimeoutMs);

    auto& rsp = dynamic_cast<RoomStartBattleRsp&>(context.GetRsp());
    FillRoomForwardRsp(context, rsp, ret, room_rsp.ret_code(), "start battle failed");
}

void ConnService::RoomSetRole(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const RoomSetRoleReq&>(context.GetReq());
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "RoomSetRole forwarding to roomsvr, gid(%llu), battle_role_type(%u)",
                 static_cast<unsigned long long>(gid), req.battle_role_type());

    roomsvr::RoomSetRoleReq room_req;
    room_req.set_gid(gid);
    room_req.set_room_id(0);
    room_req.set_battle_role_type(req.battle_role_type());

    roomsvr::RoomSetRoleResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("RoomSetRole");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomSetRoleRsp&>(context.GetRsp());
    FillRoomForwardRsp(context, rsp, ret, room_rsp.ret_code(), "set role failed");
}

void ConnService::RoomSetMap(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const RoomSetMapReq&>(context.GetReq());
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "RoomSetMap forwarding to roomsvr, gid(%llu), map_id(%u)",
                 static_cast<unsigned long long>(gid), req.map_id());

    roomsvr::RoomSetMapReq room_req;
    room_req.set_gid(gid);
    room_req.set_room_id(0);
    room_req.set_map_id(req.map_id());

    roomsvr::RoomSetMapResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("RoomSetMap");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomSetMapRsp&>(context.GetRsp());
    FillRoomForwardRsp(context, rsp, ret, room_rsp.ret_code(), "set map failed");
}

void ConnService::RoomSendEmote(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const RoomSendEmoteReq&>(context.GetReq());
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "RoomSendEmote forwarding to roomsvr, gid(%llu), emote_id(%u)",
                 static_cast<unsigned long long>(gid), req.emote_id());

    // 发送者gid只取包头，不信body（body里没有gid字段，防伪造）
    roomsvr::RoomSendEmoteReq room_req;
    room_req.set_gid(gid);
    room_req.set_room_id(0);
    room_req.set_emote_id(req.emote_id());

    roomsvr::RoomSendEmoteResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("RoomSendEmote");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomSendEmoteRsp&>(context.GetRsp());
    FillRoomForwardRsp(context, rsp, ret, room_rsp.ret_code(), "send emote failed");
}

void ConnService::RoomRename(app::RpcContext& context)
{
    const auto& req = dynamic_cast<const RoomRenameReq&>(context.GetReq());
    uint64_t gid = context.head.gid;

    APP_LOG_INFO(gid, "RoomRename forwarding to roomsvr, gid(%llu), new_name(%s)",
                 static_cast<unsigned long long>(gid), req.new_name().c_str());

    roomsvr::RenameRoomReq room_req;
    room_req.set_gid(gid);
    room_req.set_room_id(0);
    room_req.set_new_name(req.new_name());

    roomsvr::RenameRoomResp room_rsp;
    uint32_t room_cmd = roomsvr::GetRoomMethodCmd("RenameRoom");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, room_cmd, room_req, &room_rsp, nullptr,
        app::kGroupAddrRoomSvr, kForwardTimeoutMs);

    auto& rsp = dynamic_cast<RoomRenameRsp&>(context.GetRsp());
    FillRoomForwardRsp(context, rsp, ret, room_rsp.ret_code(), "rename room failed");
}

// ============================================================
// 推送接收（roomsvr→connsvr→客户端）
// ============================================================

void ConnService::OnPushRoomDetail(app::RpcContext& context)
{
    const auto& ntf = static_cast<const roomsvr::PushRoomDetailNtf&>(context.GetReq());

    APP_LOG_INFO(0, "OnPushRoomDetail recv, room_id(%llu), target_count(%d)",
                 static_cast<unsigned long long>(ntf.room_id()), ntf.target_gids_size());

    // 转换为客户端格式
    connsvr::RoomDetailSnapshot snapshot;
    snapshot.set_room_id(std::to_string(ntf.room_id()));
    snapshot.set_room_name(ntf.room_name());
    snapshot.set_host_gid(std::to_string(ntf.host_gid()));
    snapshot.set_max_players(ntf.max_players());
    snapshot.set_map_id(ntf.map_id());
    snapshot.set_in_battle(ntf.in_battle());
    snapshot.set_battle_server_address(ntf.battle_server_address());
    for (const auto& m : ntf.members())
    {
        auto* member = snapshot.add_members();
        member->set_gid(std::to_string(m.gid()));
        member->set_join_timestamp(m.join_timestamp());
        member->set_is_ready(m.is_ready());
        member->set_battle_role_type(m.battle_role_type());
        member->set_b_is_bot(m.b_is_bot());
        member->set_bot_id(m.bot_id());
        member->set_slot_index(m.slot_index());
        member->set_display_name(m.display_name());
    }

    // Add last_match if present
    if (ntf.has_last_match())
    {
        auto* last_match = snapshot.mutable_last_match();
        last_match->set_duration_sec(ntf.last_match().duration_sec());
        last_match->set_end_reason(ntf.last_match().end_reason());
        for (const auto& p : ntf.last_match().players())
        {
            auto* player = last_match->add_players();
            player->set_gid(std::to_string(p.gid()));
            player->set_display_name(p.display_name());
            player->set_kills(p.kills());
            player->set_deaths(p.deaths());
            player->set_rank(p.rank());
            player->set_battle_role_type(p.battle_role_type());
            player->set_b_is_bot(p.b_is_bot());
            player->set_bot_id(p.bot_id());
        }
    }

    // 推送给目标gid
    std::vector<uint64_t> gids;
    for (uint64_t gid : ntf.target_gids())
        gids.push_back(gid);

    ConnApp::GetInst().PushToGids(gids, app::CMD_PUSH_ROOM_DETAIL_UPDATED, snapshot);

    auto& rsp = static_cast<roomsvr::PushRoomDetailNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void ConnService::OnPushRoomList(app::RpcContext& context)
{
    const auto& ntf = static_cast<const roomsvr::PushRoomListNtf&>(context.GetReq());

    APP_LOG_INFO(0, "OnPushRoomList recv, room_count(%d)", ntf.rooms_size());

    // 转换为客户端格式
    connsvr::RoomListSnapshot snapshot;
    for (const auto& r : ntf.rooms())
    {
        auto* brief = snapshot.add_rooms();
        brief->set_room_id(std::to_string(r.room_id()));
        brief->set_room_name(r.room_name());
        brief->set_host_gid(std::to_string(r.host_gid()));
        brief->set_current_players(r.current_players());
        brief->set_max_players(r.max_players());
        brief->set_in_battle(r.in_battle());
        brief->set_has_last_match(r.has_last_match());
        brief->set_map_id(r.map_id());
        brief->set_host_display_name(r.host_display_name());
    }

    // 推送给所有在线玩家
    std::vector<uint64_t> gids;
    for (const auto& [gid, session] : ConnApp::GetInst().GetGidSessionMap())
        gids.push_back(gid);

    ConnApp::GetInst().PushToGids(gids, app::CMD_PUSH_ROOM_LIST_UPDATED, snapshot);

    auto& rsp = static_cast<roomsvr::PushRoomListNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void ConnService::OnPushBattleReady(app::RpcContext& context)
{
    const auto& ntf = static_cast<const roomsvr::PushBattleReadyNtf&>(context.GetReq());

    APP_LOG_INFO(0, "OnPushBattleReady recv, room_id(%llu), server_address(%s), token(%llu), battle_id(%s), target_count(%d)",
                 static_cast<unsigned long long>(ntf.room_id()), ntf.server_address().c_str(),
                 static_cast<unsigned long long>(ntf.token()), ntf.battle_id().c_str(),
                 ntf.target_gids_size());

    // 客户端关键调试信息：逐个记录推送目标
    for (uint64_t gid : ntf.target_gids())
    {
        APP_LOG_INFO(gid, "PushBattleReady -> gid(%llu): ClientTravel(%s), token(%llu), battle_id(%s)",
                     static_cast<unsigned long long>(gid), ntf.server_address().c_str(),
                     static_cast<unsigned long long>(ntf.token()), ntf.battle_id().c_str());
    }

    // 构造客户端推送消息
    connsvr::PushRoomBattleReady push_msg;
    push_msg.set_server_address(ntf.server_address());
    push_msg.set_token(ntf.token());
    push_msg.set_battle_id(ntf.battle_id());

    // 推送给目标gid
    std::vector<uint64_t> gids;
    for (uint64_t gid : ntf.target_gids())
        gids.push_back(gid);

    ConnApp::GetInst().PushToGids(gids, app::CMD_PUSH_ROOM_BATTLE_READY, push_msg);

    auto& rsp = static_cast<roomsvr::PushBattleReadyNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void ConnService::OnPushRoomKicked(app::RpcContext& context)
{
    const auto& ntf = static_cast<const roomsvr::PushRoomKickedNtf&>(context.GetReq());

    APP_LOG_INFO(0, "OnPushRoomKicked recv, room_id(%llu), reason(%s), target_count(%d)",
                 static_cast<unsigned long long>(ntf.room_id()), ntf.reason().c_str(),
                 ntf.target_gids_size());

    connsvr::PushRoomKicked push_msg;
    push_msg.set_reason(ntf.reason());

    std::vector<uint64_t> gids;
    for (uint64_t gid : ntf.target_gids())
        gids.push_back(gid);

    ConnApp::GetInst().PushToGids(gids, app::CMD_PUSH_ROOM_KICKED, push_msg);

    auto& rsp = static_cast<roomsvr::PushRoomKickedNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void ConnService::OnPushRoomSelecting(app::RpcContext& context)
{
    const auto& ntf = static_cast<const roomsvr::PushRoomSelectingNtf&>(context.GetReq());

    APP_LOG_INFO(0, "OnPushRoomSelecting recv, room_id(%llu), deadline_ms(%lld), duration(%u), target_count(%d)",
                 static_cast<unsigned long long>(ntf.room_id()),
                 static_cast<long long>(ntf.select_end_unix_ms()),
                 ntf.select_duration_sec(), ntf.target_gids_size());

    connsvr::PushRoomSelecting push_msg;
    push_msg.set_select_end_unix_ms(ntf.select_end_unix_ms());
    push_msg.set_select_duration_sec(ntf.select_duration_sec());

    std::vector<uint64_t> gids;
    for (uint64_t gid : ntf.target_gids())
        gids.push_back(gid);

    ConnApp::GetInst().PushToGids(gids, app::CMD_PUSH_ROOM_SELECTING, push_msg);

    auto& rsp = static_cast<roomsvr::PushRoomSelectingNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void ConnService::OnPushRoomBattleFailed(app::RpcContext& context)
{
    const auto& ntf = static_cast<const roomsvr::PushRoomBattleFailedNtf&>(context.GetReq());

    APP_LOG_INFO(0, "OnPushRoomBattleFailed recv, room_id(%llu), reason(%d), msg(%s), target_count(%d)",
                 static_cast<unsigned long long>(ntf.room_id()), ntf.reason(),
                 ntf.message().c_str(), ntf.target_gids_size());

    connsvr::PushRoomBattleFailed push_msg;
    push_msg.set_reason(ntf.reason());
    push_msg.set_message(ntf.message());

    std::vector<uint64_t> gids;
    for (uint64_t gid : ntf.target_gids())
        gids.push_back(gid);

    ConnApp::GetInst().PushToGids(gids, app::CMD_PUSH_ROOM_BATTLE_FAILED, push_msg);

    auto& rsp = static_cast<roomsvr::PushRoomBattleFailedNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

void ConnService::OnPushRoomEmote(app::RpcContext& context)
{
    const auto& ntf = static_cast<const roomsvr::PushRoomEmoteNtf&>(context.GetReq());

    APP_LOG_INFO(0, "OnPushRoomEmote recv, room_id(%llu), sender_gid(%llu), emote_id(%u), target_count(%d)",
                 static_cast<unsigned long long>(ntf.room_id()),
                 static_cast<unsigned long long>(ntf.sender_gid()), ntf.emote_id(), ntf.target_gids_size());

    connsvr::PushRoomEmote push_msg;
    push_msg.set_sender_gid(std::to_string(ntf.sender_gid()));
    push_msg.set_emote_id(ntf.emote_id());
    // expire_unix_ms 本期不下发（恒为0）：绝对墙上时钟受端上时钟偏移影响，客户端按收到时刻+3s计时

    std::vector<uint64_t> gids;
    for (uint64_t gid : ntf.target_gids())
        gids.push_back(gid);

    ConnApp::GetInst().PushToGids(gids, app::CMD_PUSH_ROOM_EMOTE, push_msg);

    auto& rsp = static_cast<roomsvr::PushRoomEmoteNtfResp&>(context.GetRsp());
    rsp.set_ret_code(0);
    context.ret_code = app::RPC_SUCCESS;
}

}  // namespace connsvr
