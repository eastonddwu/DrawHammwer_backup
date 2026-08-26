/*
 * * file name: conn_app.cpp
 * * description: ConnApp::Setup/OnInit/OnProc实现，见conn_app.h说明
 * */

#include "conn_app.h"
#include "conn.pb.h"
#include "conn_rpc_meta.h"
#include "conn_service.h"
#include "conn_constants.h"
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
#include "room.pb.h"
#include "room_error.h"
#include "room_rpc_meta.h"
#include "svr_base/default_init.h"
#include "common/clock.h"

#include <google/protobuf/message.h>

namespace connsvr
{
void ConnApp::Setup(int tconnd_addr, int shm_key, const std::string& tbus2_agent_url)
{
    tconnd_addr_ = tconnd_addr;
    shm_key_ = shm_key;
    tbus2_agent_url_ = tbus2_agent_url;
}

void ConnApp::SetGidSession(uint64_t gid, int32_t session_id)
{
    gid_to_session_[gid] = session_id;
}

int32_t ConnApp::GetGidSession(uint64_t gid) const
{
    auto it = gid_to_session_.find(gid);
    if (it == gid_to_session_.end())
        return 0;
    return it->second;
}

void ConnApp::RemoveGidSession(uint64_t gid)
{
    gid_to_session_.erase(gid);
}

void ConnApp::SetGidUserName(uint64_t gid, const std::string& user_name)
{
    gid_to_user_name_[gid] = user_name;
}

std::string ConnApp::GetGidUserName(uint64_t gid) const
{
    auto it = gid_to_user_name_.find(gid);
    return it == gid_to_user_name_.end() ? std::string() : it->second;
}

void ConnApp::RemoveGidUserName(uint64_t gid)
{
    gid_to_user_name_.erase(gid);
}

bool ConnApp::HasSessionIdentity(int32_t session_id) const
{
    return session_identity_.find(session_id) != session_identity_.end();
}

bool ConnApp::BindSessionIdentity(int32_t session_id, uint64_t gid, bool is_guest)
{
    if (session_id <= 0 || gid == 0)
        return false;
    if (HasSessionIdentity(session_id))
        return false;

    session_identity_.emplace(session_id, SessionIdentity{gid, is_guest});
    if (is_guest)
        ++guest_online_count_;
    return true;
}

void ConnApp::ClearSessionIdentity(int32_t session_id)
{
    auto it = session_identity_.find(session_id);
    if (it == session_identity_.end())
        return;

    if (it->second.is_guest && guest_online_count_ > 0)
        --guest_online_count_;
    session_identity_.erase(it);
}

bool ConnApp::AllocateGuestGid(uint64_t& gid)
{
    for (uint32_t i = 0; i < kGuestLow32Mask; ++i)
    {
        uint32_t low = next_guest_low32_seq_++;
        if (next_guest_low32_seq_ == 0)
            next_guest_low32_seq_ = 1;
        if (low == 0)
            continue;

        uint64_t candidate = kGuestGidMarker | static_cast<uint64_t>(low);
        if (gid_to_session_.find(candidate) != gid_to_session_.end())
            continue;

        gid = candidate;
        return true;
    }
    gid = 0;
    return false;
}

bool ConnApp::IsGuestGid(uint64_t gid) const
{
    return (gid & ~kGuestLow32Mask) == kGuestGidMarker && (gid & kGuestLow32Mask) != 0;
}

bool ConnApp::CanAcceptMoreGuests() const
{
    return guest_online_count_ < kGuestMaxOnline;
}

bool ConnApp::CheckAndConsumeGuestLoginRate(int32_t session_id)
{
    uint32_t ip = tconnd_channel_.GetSessionClientIp(session_id);
    if (ip == 0)
        return true;

    uint64_t now_ms = app::Clock::GetInst().CurrentMilliSec();
    IpRateWindow& window = guest_login_rate_window_[ip];
    if (window.window_start_ms == 0 || now_ms - window.window_start_ms >= 60000)
    {
        window.window_start_ms = now_ms;
        window.count = 0;
    }

    if (window.count >= kGuestIpRateLimitPerMin)
        return false;

    ++window.count;
    return true;
}

void ConnApp::PushToGids(const std::vector<uint64_t>& gids, uint32_t cmd_id, const google::protobuf::Message& msg)
{
    if (gids.empty())
        return;

    // 同一份消息发给多个玩家：body 只序列化一次。
    // 此前是逐个调用 PushToGid，每个收件人都要重做「body 序列化 + PkgHead 序列化 +
    // BuildFrame 拼帧」，而 TconndChannel::Send 收到帧后又把 PkgHead 反序列化回来——
    // 同进程内的序列化往返，且随在线人数线性放大。压测显示 40 连接时
    // connsvr 处理一次改名要 7.5ms，而 roomsvr 侧业务只要 33us，开销几乎全在这里。
    std::string body_bytes;
    msg.SerializeToString(&body_bytes);

    std::vector<std::pair<uint64_t, int32_t>> targets;
    targets.reserve(gids.size());
    for (uint64_t gid : gids)
    {
        int32_t session_id = GetGidSession(gid);
        if (session_id == 0)
        {
            APP_LOG_WARN(gid, "PushToGids: gid(%llu) has no session, skip push, cmd_id(%u)",
                         static_cast<unsigned long long>(gid), cmd_id);
            continue;
        }
        targets.emplace_back(gid, session_id);
    }

    if (targets.empty())
        return;

    GetTconndChannel().Broadcast(targets, cmd_id, body_bytes, app::FLAG_FROM_TCONND);
}

void ConnApp::PushToGid(uint64_t gid, uint32_t cmd_id, const google::protobuf::Message& msg)
{
    int32_t session_id = GetGidSession(gid);
    if (session_id == 0)
    {
        APP_LOG_WARN(gid, "PushToGid: gid(%llu) has no session, skip push, cmd_id(%u)",
                     static_cast<unsigned long long>(gid), cmd_id);
        return;
    }

    // 序列化body
    std::string body_bytes;
    msg.SerializeToString(&body_bytes);

    // 构造PkgHead
    app::protocol::PkgHead pkg_head;
    pkg_head.set_cmd(cmd_id);
    pkg_head.set_seq_id(0);  // 推送 seq_id=0
    pkg_head.set_gid(gid);
    pkg_head.set_src(0);
    pkg_head.set_dst(static_cast<uint32_t>(session_id));
    pkg_head.set_ret_code(0);
    pkg_head.set_timeout(0);
    pkg_head.set_flag(app::FLAG_FROM_TCONND);

    std::string head_bytes;
    pkg_head.SerializeToString(&head_bytes);

    // 按帧格式拼装 [FramePrefix][head][body]
    std::string frame = app::BuildFrame(head_bytes, body_bytes);

    int32_t send_ret = GetTconndChannel().Send(static_cast<uint32_t>(session_id), frame.data(), frame.size());

    if (send_ret != 0)
    {
        APP_LOG_WARN(gid, "PushToGid: send to session(%d) failed, ret(%d), cmd_id(%u)", session_id, send_ret, cmd_id);
    }
}

bool ConnApp::OnInit()
{
    if (!UseDefaultInit(*this, MySvrID(), tbus2_agent_url_))
        return false;

    // tconnd初始化
    app::TconndChannel::Options options;
    options.shm_key = shm_key_;
    options.bind_addr = static_cast<int>(app::InstFromBusid(MySvrID()));
    options.tconnd_addr = tconnd_addr_;
    if (!tconnd_channel_.Init(options))
    {
        APP_LOG_WARN(0, "tconnd channel init fail, svr_id(%u), tconnd_addr(%d), continue without real tconnd",
                     MySvrID(), tconnd_addr_);
    }

    // 设置断连回调：客户端断连时自动LeaveRoom
    tconnd_channel_.SetDisconnectCallback([this](uint64_t gid) { OnClientDisconnect(gid); });

    if (!AddTransportInfo(app::TRANSPORT_TCONND, {&tconnd_channel_, &recv_codec_, &send_codec_}))
    {
        APP_LOG_ERROR(0, "add transport info fail");
        return false;
    }

    // ---- 登录/注册 ----
    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("Login"),
            {ConnService::Login, &LoginReq::default_instance(), &LoginResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register Login fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("SetUserInfo"),
            {ConnService::SetUserInfo, &SetUserInfoReq::default_instance(), &SetUserInfoResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register SetUserInfo fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("GuestLogin"),
            {ConnService::GuestLogin, &GuestLoginReq::default_instance(), &GuestLoginResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register GuestLogin fail");
        return false;
    }

    // ---- 房间列表查询 ----
    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomList"),
            {ConnService::RoomList, &RoomListReq::default_instance(), &RoomListRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomList fail");
        return false;
    }

    // ---- 房间操作 ----
    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomCreate"),
            {ConnService::RoomCreate, &RoomCreateReq::default_instance(), &RoomCreateRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomCreate fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomJoin"),
            {ConnService::RoomJoin, &RoomJoinReq::default_instance(), &RoomJoinRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomJoin fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomLeave"),
            {ConnService::RoomLeave, &RoomLeaveReq::default_instance(), &RoomLeaveRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomLeave fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomSetReady"),
            {ConnService::RoomSetReady, &RoomSetReadyReq::default_instance(), &RoomSetReadyRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomSetReady fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomAddBot"), {ConnService::RoomAddBot, &RoomAddBotReq::default_instance(),
                                             &RoomAddBotRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomAddBot fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomRemoveBot"), {ConnService::RoomRemoveBot, &RoomRemoveBotReq::default_instance(),
                                                &RoomRemoveBotRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomRemoveBot fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomStartBattle"), {ConnService::RoomStartBattle, &RoomStartBattleReq::default_instance(),
                                                  &RoomStartBattleRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomStartBattle fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomSetRole"),
            {ConnService::RoomSetRole, &RoomSetRoleReq::default_instance(), &RoomSetRoleRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomSetRole fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomSetMap"),
            {ConnService::RoomSetMap, &RoomSetMapReq::default_instance(), &RoomSetMapRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomSetMap fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(GetConnMethodCmd("RoomSendEmote"),
                                                  {ConnService::RoomSendEmote, &RoomSendEmoteReq::default_instance(),
                                                   &RoomSendEmoteRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomSendEmote fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetConnMethodCmd("RoomRename"),
            {ConnService::RoomRename, &RoomRenameReq::default_instance(), &RoomRenameRsp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register RoomRename fail");
        return false;
    }

    // ---- 推送接收 ----
    if (!app::RpcService::GetInst().RegisterMethod(
            roomsvr::GetRoomMethodCmd("PushRoomDetail"),
            {ConnService::OnPushRoomDetail, &roomsvr::PushRoomDetailNtf::default_instance(),
             &roomsvr::PushRoomDetailNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushRoomDetail fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            roomsvr::GetRoomMethodCmd("PushRoomList"),
            {ConnService::OnPushRoomList, &roomsvr::PushRoomListNtf::default_instance(),
             &roomsvr::PushRoomListNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushRoomList fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            roomsvr::GetRoomMethodCmd("PushBattleReady"),
            {ConnService::OnPushBattleReady, &roomsvr::PushBattleReadyNtf::default_instance(),
             &roomsvr::PushBattleReadyNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushBattleReady fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            roomsvr::GetRoomMethodCmd("PushRoomKicked"),
            {ConnService::OnPushRoomKicked, &roomsvr::PushRoomKickedNtf::default_instance(),
             &roomsvr::PushRoomKickedNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushRoomKicked fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            roomsvr::GetRoomMethodCmd("PushRoomSelecting"),
            {ConnService::OnPushRoomSelecting, &roomsvr::PushRoomSelectingNtf::default_instance(),
             &roomsvr::PushRoomSelectingNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushRoomSelecting fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            roomsvr::GetRoomMethodCmd("PushRoomBattleFailed"),
            {ConnService::OnPushRoomBattleFailed, &roomsvr::PushRoomBattleFailedNtf::default_instance(),
             &roomsvr::PushRoomBattleFailedNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushRoomBattleFailed fail");
        return false;
    }
    if (!app::RpcService::GetInst().RegisterMethod(
            roomsvr::GetRoomMethodCmd("PushRoomEmote"),
            {ConnService::OnPushRoomEmote, &roomsvr::PushRoomEmoteNtf::default_instance(),
             &roomsvr::PushRoomEmoteNtfResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register PushRoomEmote fail");
        return false;
    }

    APP_LOG_INFO(0, "ConnApp init ok, svr_id(%u), tconnd_addr(%d), shm_key(%d), tbus2_busid(%u), agent_url(%s)",
                 MySvrID(), tconnd_addr_, shm_key_, MySvrID(), tbus2_agent_url_.c_str());
    return true;
}

size_t ConnApp::OnProc(uint64_t now_ms, bool stop)
{
    if (stop)
        return 0;
    return tconnd_channel_.Loop(option_.max_deal_pkg_num);
}

void ConnApp::OnClientDisconnect(uint64_t gid)
{
    APP_LOG_INFO(gid, "client disconnect, auto-leave room, gid(%llu)", static_cast<unsigned long long>(gid));

    // 游客与账号玩家走完全相同的断线清理路径：立即清会话 + fire-and-forget LeaveRoom。
    // 注意：此回调可能运行在非协程上下文，严禁在这里使用需要等待响应的协程RPC(rsp非空)，
    // 否则ContextController::Pending()会取到空协程并触发assert，直接打挂connsvr。
    ClearSessionIdentity(GetGidSession(gid));
    RemoveGidSession(gid);
    RemoveGidUserName(gid);

    roomsvr::LeaveRoomReq leave_req;
    leave_req.set_gid(gid);
    leave_req.set_room_id(0);  // room_id=0 触发按gid查找所在房间
    uint32_t leave_cmd = roomsvr::GetRoomMethodCmd("LeaveRoom");
    app::RpcService::GetInst().Rpc(app::TRANSPORT_PB_TBUSPP, gid, leave_cmd, leave_req, nullptr, nullptr,
                                   app::kGroupAddrRoomSvr, 1000);
}

}  // namespace connsvr
