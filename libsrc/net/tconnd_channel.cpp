/*
 * * file name: tconnd_channel.cpp
 * * description: TconndChannel实现，见tconnd_channel.h说明
 * */

#include "tconnd_channel.h"
#include <arpa/inet.h>
#include <cstring>
#include "client_header.h"
#include "core/log.h"
#include "core/pkg_flag.h"
#include "net/pkg_framing.h"
#include "pkg_head.pb.h"

namespace app
{

namespace
{
const char* GetAccountTypeName(uint16_t type)
{
    switch (type)
    {
        case TCONNAPI_ACCOUNT_WX_OPENID:
            return "WX";
        case TCONNAPI_ACCOUNT_QQ_OPENID:
            return "QQ";
        case TCONNAPI_ACCOUNT_QQ_OPENID_HL:
            return "QQ_HL";
        case TCONNAPI_ACCOUNT_QQ_UIN:
            return "QQ_UIN";
        case TCONNAPI_ACCOUNT_QQ_UIN_PTLOGIN:
            return "QQ_PTLOGIN";
        case TCONNAPI_ACCOUNT_APPLE_OPENID:
            return "APPLE";
        case TCONNAPI_ACCOUNT_IOS_GUEST:
            return "IOS_GUEST";
        case TCONNAPI_ACCOUNT_FACEBOOK:
            return "FACEBOOK";
        case TCONNAPI_ACCOUNT_GOOGLEPLAY:
            return "GOOGLEPLAY";
        case TCONNAPI_ACCOUNT_GARENA:
            return "GARENA";
        case 0:
            return "NONE";
        default:
            return "UNKNOWN";
    }
}

const char* GetClientTypeName(int32_t type)
{
    switch (type)
    {
        case TCONNAPI_CLIENT_TYPE_PC:
            return "PC";
        case TCONNAPI_CLIENT_TYPE_MOBILE:
            return "MOBILE";
        case TCONNAPI_CLIENT_TYPE_ANDROID:
            return "ANDROID";
        case TCONNAPI_CLIENT_TYPE_IOS:
            return "IOS";
        case TCONNAPI_CLIENT_TYPE_MAC:
            return "MAC";
        case TCONNAPI_CLIENT_TYPE_WINDOWS:
            return "WINDOWS";
        case TCONNAPI_CLIENT_TYPE_SWITCH:
            return "SWITCH";
        case TCONNAPI_CLIENT_TYPE_PS:
            return "PS";
        case TCONNAPI_CLIENT_TYPE_XBOX:
            return "XBOX";
        case TCONNAPI_CLIENT_TYPE_HARMONY:
            return "HARMONY";
        case TCONNAPI_CLIENT_TYPE_UNKNOWN:
            return "UNKNOWN";
        default:
            return "OTHER";
    }
}

const char* GetStopReasonName(int32_t reason)
{
    switch (reason)
    {
        case TFRAMEHEAD_REASON_NONE:
            return "NONE";
        case TFRAMEHEAD_REASON_IDLE_CLOSE:
            return "IDLE_CLOSE";
        case TFRAMEHEAD_REASON_PEER_CLOSE:
            return "PEER_CLOSE";
        case TFRAMEHEAD_REASON_NETWORK_FAIL:
            return "NETWORK_FAIL";
        case TFRAMEHEAD_REASON_BAD_PKGLEN:
            return "BAD_PKGLEN";
        case TFRAMEHEAD_REASON_EXCEED_LIMIT:
            return "EXCEED_LIMIT";
        case TFRAMEHEAD_REASON_TCONND_SHUTDOWN:
            return "TCONND_SHUTDOWN";
        case TFRAMEHEAD_REASON_SELF_CLOSE:
            return "SELF_CLOSE";
        case TFRAMEHEAD_REASON_AUTH_FAIL:
            return "AUTH_FAIL";
        case TFRAMEHEAD_REASON_CLIENT_CLOSE:
            return "CLIENT_CLOSE";
        case TFRAMEHEAD_REASON_CLIENT_RECONNECT:
            return "CLIENT_RECONNECT";
        default:
            return "OTHER";
    }
}

std::string FormatIpPort(uint32_t ip, uint16_t port)
{
    struct in_addr addr;
    addr.s_addr = ip;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s:%u", inet_ntoa(addr), ntohs(port));
    return buf;
}

}  // namespace

TconndChannel::~TconndChannel()
{
    if (handle_ != -1)
    {
        tconnapi_free(&handle_);
    }
}

bool TconndChannel::Init(const Options& options)
{
    bind_addr_ = options.bind_addr;
    tconnd_addr_ = options.tconnd_addr;
    std::memset(&recv_head_, 0, sizeof(recv_head_));

    int ret = tconnapi_init(options.shm_key);
    if (ret != 0)
    {
        APP_LOG_ERROR(0, "tconnapi_init failed, shm_key(%d), ret(%d)", options.shm_key, ret);
        return false;
    }

    ret = tconnapi_create(bind_addr_, &handle_);
    if (ret != 0)
    {
        APP_LOG_ERROR(0, "tconnapi_create failed, bind_addr(%d), ret(%d)", bind_addr_, ret);
        return false;
    }

    int opt = 1;
    ret = tconnapi_set_handle_opt(handle_, TCONNAPI_OPT_NAME_TBUS_EXCLUSIVE_CHANNELS, &opt, sizeof(opt));
    if (ret != 0)
    {
        APP_LOG_ERROR(0, "tconnapi_set_handle_opt failed, ret(%d)", ret);
        return false;
    }

    ret = tconnapi_connect(handle_, tconnd_addr_);
    if (ret != 0)
    {
        APP_LOG_ERROR(0, "tconnapi_connect failed, tconnd_addr(%d), ret(%d)", tconnd_addr_, ret);
        return false;
    }

    APP_LOG_INFO(0, "tconnd channel init ok, bind_addr(%d), tconnd_addr(%d)", bind_addr_, tconnd_addr_);
    return true;
}

int32_t TconndChannel::Send(uint32_t dest_id, const char* buff, size_t buff_len)
{
    int32_t session_id = static_cast<int32_t>(dest_id);
    auto iter = sessions_.find(session_id);
    if (iter == sessions_.end())
    {
        APP_LOG_ERROR(0, "send failed, session(%d) not found", session_id);
        return -1;
    }

    Session& session = iter->second;

    // buff是RPC框架MethodFinish()通过PbSendCodec::Encode()生成的FramePrefix+PkgHead+body帧
    // 需要解码PkgHead，提取cmd/gid/seq_id/flag等字段，构造ClientHeader+body发给客户端
    if (buff_len < sizeof(FramePrefix))
    {
        APP_LOG_WARN(0, "send failed, session(%d) frame too short(%zu)", session_id, buff_len);
        return -1;
    }

    FramePrefix prefix;
    std::memcpy(&prefix, buff, sizeof(FramePrefix));

    if (buff_len < sizeof(FramePrefix) + prefix.head_len + prefix.body_len)
    {
        APP_LOG_WARN(0, "send failed, session(%d) incomplete frame", session_id);
        return -1;
    }

    const char* head_data = buff + sizeof(FramePrefix);
    const char* body_data = head_data + prefix.head_len;

    app::protocol::PkgHead pkg_head;
    if (!pkg_head.ParseFromArray(head_data, static_cast<int>(prefix.head_len)))
    {
        APP_LOG_WARN(0, "send failed, session(%d) parse PkgHead failed", session_id);
        return -1;
    }

    // 用PkgHead字段构造ClientHeader
    ClientHeader client_header;
    std::memset(&client_header, 0, sizeof(client_header));
    client_header.cmd_id = pkg_head.cmd();
    client_header.gid = pkg_head.gid();
    client_header.client_seq_id = static_cast<uint32_t>(pkg_head.seq_id());
    client_header.server_seq_id = next_server_serial_++;
    client_header.body_length = prefix.body_len;
    client_header.pkg_flag = pkg_head.flag();
    client_header.magic = CLIENT_HEADER_MAGIC;

    // 如果PkgHead.gid非0（登录成功后Login handler设置了gid），更新session中的gid
    // 后续该session的所有回包都会带上这个gid作为ClientHeader.gid(player_id)
    if (pkg_head.gid() != 0 && session.gid == 0)
    {
        session.gid = pkg_head.gid();
    }

    // 打包ClientHeader + 拷贝body
    char header_buf[PACKED_CLIENT_HEADER_LENGTH];
    size_t header_len = sizeof(header_buf);
    if (Pack(client_header, header_buf, header_len) != 0)
    {
        APP_LOG_WARN(0, "send failed, session(%d) Pack ClientHeader failed", session_id);
        return -1;
    }

    std::string send_buf;
    send_buf.resize(header_len + prefix.body_len);
    std::memcpy(&send_buf[0], header_buf, header_len);
    if (prefix.body_len > 0)
        std::memcpy(&send_buf[header_len], body_data, prefix.body_len);

    // 通过tconnapi_send发送给客户端
    TFRAMEHEAD frame;
    std::memset(&frame, 0, sizeof(frame));
    frame.chVer = TDR_METALIB_TFRAMEHEAD_VERSION;
    frame.chCmd = TFRAMEHEAD_CMD_INPROC;
    frame.iID = session_id;
    frame.iConnIdx = session.conn_idx;
    frame.ullCallBack = session.tconnd_callback;

    int ret = tconnapi_send(handle_, tconnd_addr_, send_buf.data(), static_cast<int>(send_buf.size()), &frame);
    if (ret != 0)
    {
        APP_LOG_WARN(0, "tconnapi_send failed, session(%d), conn_idx(%d), ret(%d)", session_id, session.conn_idx, ret);
        return -1;
    }
    return 0;
}

size_t TconndChannel::Broadcast(const std::vector<std::pair<uint64_t, int32_t>>& gid_sessions, uint32_t cmd_id,
                                const std::string& body_bytes, uint32_t pkg_flag)
{
    if (gid_sessions.empty())
        return 0;

    // 复用同一个发送缓冲：所有收件人的body完全相同，只有ClientHeader里的gid不同。
    // 因此body只拷贝一次，循环内仅重写头部34字节。
    std::string send_buf;
    send_buf.resize(PACKED_CLIENT_HEADER_LENGTH + body_bytes.size());
    if (!body_bytes.empty())
        std::memcpy(&send_buf[PACKED_CLIENT_HEADER_LENGTH], body_bytes.data(), body_bytes.size());

    size_t sent = 0;
    for (const auto& item : gid_sessions)
    {
        uint64_t gid = item.first;
        int32_t session_id = item.second;

        auto iter = sessions_.find(session_id);
        if (iter == sessions_.end())
        {
            APP_LOG_WARN(gid, "broadcast skip, session(%d) not found, cmd(%u)", session_id, cmd_id);
            continue;
        }
        Session& session = iter->second;

        ClientHeader client_header;
        std::memset(&client_header, 0, sizeof(client_header));
        client_header.cmd_id = cmd_id;
        client_header.gid = gid;
        client_header.client_seq_id = 0;  // 推送无对应请求
        client_header.server_seq_id = next_server_serial_++;
        client_header.body_length = static_cast<uint32_t>(body_bytes.size());
        client_header.pkg_flag = pkg_flag;
        client_header.magic = CLIENT_HEADER_MAGIC;

        char header_buf[PACKED_CLIENT_HEADER_LENGTH];
        size_t header_len = sizeof(header_buf);
        if (Pack(client_header, header_buf, header_len) != 0)
        {
            APP_LOG_WARN(gid, "broadcast Pack failed, session(%d), cmd(%u)", session_id, cmd_id);
            continue;
        }
        std::memcpy(&send_buf[0], header_buf, header_len);

        if (gid != 0 && session.gid == 0)
            session.gid = gid;

        TFRAMEHEAD frame;
        std::memset(&frame, 0, sizeof(frame));
        frame.chVer = TDR_METALIB_TFRAMEHEAD_VERSION;
        frame.chCmd = TFRAMEHEAD_CMD_INPROC;
        frame.iID = session_id;
        frame.iConnIdx = session.conn_idx;
        frame.ullCallBack = session.tconnd_callback;

        int ret = tconnapi_send(handle_, tconnd_addr_, send_buf.data(), static_cast<int>(send_buf.size()), &frame);
        if (ret != 0)
        {
            APP_LOG_WARN(gid, "broadcast tconnapi_send failed, session(%d), ret(%d)", session_id, ret);
            continue;
        }
        ++sent;
    }

    return sent;
}

size_t TconndChannel::Loop(uint32_t max_recv_count)
{
    if (handle_ == -1)
        return 0;

    static char recv_buf[MAX_PKG_LEN];

    size_t num = 0;
    while (num < max_recv_count)
    {
        int src = 0;
        int len = static_cast<int>(sizeof(recv_buf));
        int ret = tconnapi_recv(handle_, &src, recv_buf, &len, &recv_head_);
        if (ret == -1)
        {
            // 没有包了
            break;
        }
        if (ret != 0)
        {
            APP_LOG_ERROR(0, "tconnapi_recv failed, ret(%d)", ret);
            break;
        }

        ++num;

        switch (recv_head_.chCmd)
        {
            case TFRAMEHEAD_CMD_START:
                OnSessionStart();
                break;
            case TFRAMEHEAD_CMD_STOP:
                OnSessionStop();
                break;
            case TFRAMEHEAD_CMD_RELAY:
                OnSessionRelay();
                break;
            case TFRAMEHEAD_CMD_INPROC:
                OnRecvMessage(recv_buf, static_cast<size_t>(len));
                break;
            default:
                APP_LOG_WARN(0, "unhandled tconnd cmd(%d), ignore", recv_head_.chCmd);
                break;
        }
    }

    return num;
}

bool TconndChannel::ExtractAccountInfo(uint64_t& gopenid, uint16_t& account_type, int32_t& client_type) const
{
    const TFRAMECMDSTART& start = recv_head_.stCmdData.stStart;
    const TFRAMEHEADACCOUNT& account = start.stUserAccount;

    account_type = account.wType;
    client_type = start.iClientType;

    if (account.bFormat != TFRAMEHEAD_ID_STRING)
    {
        APP_LOG_WARN(0, "account format not string, bFormat(%u), wType(%u)", account.bFormat, account.wType);
        return false;
    }

    gopenid = std::strtoull(account.stValue.szSTRING, nullptr, 0);
    if (gopenid == 0)
    {
        APP_LOG_WARN(0, "parse gopenid failed, szSTRING(%s)", account.stValue.szSTRING);
        return false;
    }

    APP_LOG_INFO(0, "extract account info ok, gopenid(%llu), account_type(%s/%u), client_type(%s/%d)",
                 static_cast<unsigned long long>(gopenid), GetAccountTypeName(account_type), account_type,
                 GetClientTypeName(client_type), client_type);
    return true;
}

uint64_t TconndChannel::GetSessionOpenid(int32_t session_id) const
{
    auto iter = sessions_.find(session_id);
    if (iter == sessions_.end())
        return 0;
    return iter->second.gopenid;
}

uint32_t TconndChannel::GetSessionClientIp(int32_t session_id) const
{
    auto iter = sessions_.find(session_id);
    if (iter == sessions_.end())
        return 0;
    return iter->second.client_ip;
}

void TconndChannel::OnSessionStart()
{
    int32_t session_id = next_session_id_++;

    Session session;
    session.conn_idx = recv_head_.iConnIdx;
    session.tconnd_callback = recv_head_.ullCallBack;

    // 从tconnd CMD_START提取账号信息
    uint64_t gopenid = 0;
    uint16_t account_type = 0;
    int32_t client_type = 0;
    if (ExtractAccountInfo(gopenid, account_type, client_type))
    {
        session.gopenid = gopenid;
        session.account_type = account_type;
        session.client_type = client_type;
    }
    else
    {
        APP_LOG_WARN(0, "session start, extract account info failed, session_id(%d), conn_idx(%d)", session_id,
                     session.conn_idx);
    }

    // 提取客户端真实IP和tconnd监听类型
    const TFRAMECMDSTART& start = recv_head_.stCmdData.stStart;
    session.client_ip = start.stOriClientIPInfo.ulIp;
    session.client_port = start.stOriClientIPInfo.wPort;
    session.listen_type = start.dwListenType;

    sessions_[session_id] = session;

    APP_LOG_INFO(0,
                 "client connected, session_id(%d), conn_idx(%d), gopenid(%llu), "
                 "account(%s/%u), client(%s/%d), ip(%s), listen_type(%s/%u), total_sessions(%zu)",
                 session_id, session.conn_idx, static_cast<unsigned long long>(session.gopenid),
                 GetAccountTypeName(session.account_type), session.account_type, GetClientTypeName(session.client_type),
                 session.client_type, FormatIpPort(session.client_ip, session.client_port).c_str(),
                 session.listen_type == 0 ? "TCP" : "LWIP", session.listen_type, sessions_.size());

    // 直接回ACK，认为登录成功
    TFRAMEHEAD frame = recv_head_;
    frame.chVer = TDR_METALIB_TFRAMEHEAD_VERSION;
    frame.chCmd = TFRAMEHEAD_CMD_START;
    frame.iID = session_id;
    int ret = tconnapi_send(handle_, tconnd_addr_, nullptr, 0, &frame);
    if (ret != 0)
    {
        APP_LOG_WARN(0, "send start response failed, session_id(%d), ret(%d)", session_id, ret);
    }
}

void TconndChannel::OnSessionStop()
{
    int32_t session_id = recv_head_.iID;
    int32_t reason = recv_head_.stCmdData.stStop.iReason;

    auto iter = sessions_.find(session_id);
    if (iter == sessions_.end())
    {
        APP_LOG_WARN(0, "client disconnect, session(%d) not found, reason(%s/%d)", session_id,
                     GetStopReasonName(reason), reason);
        return;
    }

    const Session& session = iter->second;
    APP_LOG_INFO(0,
                 "client disconnected, session_id(%d), conn_idx(%d), gopenid(%llu), gid(%llu), "
                 "ip(%s), reason(%s/%d), total_sessions(%zu)",
                 session_id, session.conn_idx, static_cast<unsigned long long>(session.gopenid),
                 static_cast<unsigned long long>(session.gid),
                 FormatIpPort(session.client_ip, session.client_port).c_str(), GetStopReasonName(reason), reason,
                 sessions_.size() - 1);

    uint64_t gid = session.gid;
    sessions_.erase(iter);

    // 通知上层gid断连，用于自动LeaveRoom等清理（gid=0表示未登录，跳过）
    if (disconnect_callback_ && gid != 0)
        disconnect_callback_(gid);
}

void TconndChannel::OnSessionRelay()
{
    APP_LOG_WARN(0, "relay not supported, session_id(%d), conn_idx(%d), ignore", recv_head_.iID, recv_head_.iConnIdx);
}

void TconndChannel::OnRecvMessage(const char* recv_buf, size_t len)
{
    int32_t session_id = recv_head_.iID;
    auto iter = sessions_.find(session_id);
    if (iter == sessions_.end())
    {
        APP_LOG_WARN(0, "recv msg, session(%d) not found", session_id);
        return;
    }

    if (!recv_callback_)
        return;

    // 客户端发来的是ClientHeader(34B) + protobuf body格式
    // 解析ClientHeader，构造FramePrefix+PkgHead+body帧交给RPC框架
    if (len < PACKED_CLIENT_HEADER_LENGTH)
    {
        APP_LOG_WARN(0, "recv msg, session(%d) pkg too short(%zu), drop", session_id, len);
        return;
    }

    ClientHeader client_header;
    if (Unpack(client_header, recv_buf, len) != 0)
    {
        APP_LOG_WARN(0, "recv msg, session(%d) Unpack ClientHeader failed (bad magic?), drop", session_id);
        return;
    }

    // 校验长度一致性
    if (len != PACKED_CLIENT_HEADER_LENGTH + client_header.body_length)
    {
        APP_LOG_WARN(0, "recv msg, session(%d) length mismatch: total(%zu) != header(%zu) + body(%u), drop", session_id,
                     len, PACKED_CLIENT_HEADER_LENGTH, client_header.body_length);
        return;
    }

    // ClientHeader → PkgHead字段映射
    app::protocol::PkgHead pkg_head;
    pkg_head.set_cmd(client_header.cmd_id);
    pkg_head.set_seq_id(client_header.client_seq_id);
    pkg_head.set_gid(client_header.gid);
    pkg_head.set_src(static_cast<uint32_t>(session_id));
    pkg_head.set_dst(0);
    pkg_head.set_ret_code(0);
    pkg_head.set_timeout(0);
    pkg_head.set_flag(FLAG_FROM_TCONND);

    const char* body_data = recv_buf + PACKED_CLIENT_HEADER_LENGTH;
    uint32_t body_len = client_header.body_length;

    // 序列化为FramePrefix+PkgHead+body帧
    std::string head_bytes;
    pkg_head.SerializeToString(&head_bytes);

    FramePrefix frame_prefix;
    frame_prefix.magic = PKG_MAGIC;
    frame_prefix.head_len = static_cast<uint32_t>(head_bytes.size());
    frame_prefix.body_len = body_len;

    std::string frame;
    frame.resize(sizeof(FramePrefix) + head_bytes.size() + body_len);
    char* buf = &frame[0];
    std::memcpy(buf, &frame_prefix, sizeof(FramePrefix));
    std::memcpy(buf + sizeof(FramePrefix), head_bytes.data(), head_bytes.size());
    if (body_len > 0)
        std::memcpy(buf + sizeof(FramePrefix) + head_bytes.size(), body_data, body_len);

    recv_callback_(frame.data(), frame.size(), static_cast<uint32_t>(session_id), 0);
}

}  // namespace app
