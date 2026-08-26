// test_gcp_client.cpp: 用真实GCP协议(AES加密)连接tconnd，收发ClientHeader+protobuf body格式的请求/响应
// 使用2个并发GCP连接(Player A & B)验证房间系统完整生命周期和推送
// 测试流程：Login×2 → RoomCreate → RoomJoin → SetReady → Rename → Leave → HostMigration → RoomDestroy
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "conn.pb.h"
#include "net/client_cmd_id.h"
#include "net/client_header.h"
#include "tgcpapi/tgcpapi.h"

using namespace app;

// ========== GCP 连接参数 ==========
static const int kServiceID = 1;
static const eAuthType kAuthMode = TGCP_AUTH_NONE;
static const eEncryptMethod kEncryptMethod = TGCP_ENCRYPT_METHOD_AES;
static const eKeyMaking kKeyMakingMethod = TGCP_KEY_MAKING_INSVR;
static const int kMaxGameDataLen = 10240;
static const int kMaxTcpTimeout = 10000;
static std::string g_tconnd_url = "tcp://127.0.0.1:18801";

// ========== 测试统计 ==========
static int g_pass_count = 0;
static int g_fail_count = 0;
static int g_selecting_push_count = 0;
// 2006累计收到数。因为212响应与2006广播是两条独立消息，到达顺序不定，
// RecvResponse跳过的推送也要计入，否则断言会被竞态弄假。
static int g_emote_push_count = 0;

struct PushObservation
{
    int detail_count = 0;
    int list_count = 0;
    int battle_ready_count = 0;
    int selecting_count = 0;
    int battle_failed_count = 0;
    int emote_count = 0;
    connsvr::RoomDetailSnapshot last_detail;
    connsvr::RoomListSnapshot last_list;
    connsvr::PushRoomEmote last_emote;
};

#define TEST_ASSERT(cond, msg)                               \
    do                                                       \
    {                                                        \
        if (!(cond))                                         \
        {                                                    \
            printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
            return -1;                                       \
        }                                                    \
    } while (0)

#define PASS(name)                     \
    do                                 \
    {                                  \
        printf("  [PASS] %s\n", name); \
        g_pass_count++;                \
    } while (0)

// ========== GCP 辅助函数 ==========

static int InitHandle(HTGCPAPI handle, const char* openid_str)
{
    int ret =
        tgcpapi_init(handle, kServiceID, TGCP_ANDROID, kAuthMode, kEncryptMethod, kKeyMakingMethod, kMaxGameDataLen);
    if (ret != TGCP_ERR_NONE)
    {
        printf("tgcpapi_init error, ret(%d): %s\n", ret, tgcpapi_error_string(ret));
        return -1;
    }

    TGCPACCOUNT account = TGCPACCOUNT();
    account.uType = TGCP_ACCOUNT_TYPE_NONE;
    account.bFormat = TGCP_ACCOUNT_FORMAT_STRING;
    strncpy(account.stAccountValue.szID, openid_str, sizeof(account.stAccountValue.szID) - 1);

    ret = tgcpapi_set_account(handle, &account);
    if (ret != TGCP_ERR_NONE)
    {
        printf("tgcpapi_set_account error, ret(%d): %s\n", ret, tgcpapi_error_string(ret));
        return -1;
    }
    return 0;
}

static int ConnectHandle(HTGCPAPI handle, const char* openid_str)
{
    if (InitHandle(handle, openid_str) != 0)
        return -1;

    printf("  connecting to tconnd(%s) as openid=%s ...\n", g_tconnd_url.c_str(), openid_str);
    int ret = tgcpapi_start_connection(handle, g_tconnd_url.c_str(), kMaxTcpTimeout);
    if (ret != TGCP_ERR_NONE && ret != TGCP_ERR_STAY_IN_QUEUE)
    {
        printf("  connect error, ret(%d): %s\n", ret, tgcpapi_error_string(ret));
        return -1;
    }
    printf("  connected\n");
    return 0;
}

static int SendRequest(HTGCPAPI handle, uint32_t cmd_id, uint32_t seq_id, uint64_t gid,
                       const google::protobuf::Message& body)
{
    std::string body_bytes;
    body.SerializeToString(&body_bytes);

    ClientHeader header;
    std::memset(&header, 0, sizeof(header));
    header.body_length = static_cast<uint32_t>(body_bytes.size());
    header.cmd_id = cmd_id;
    header.gid = gid;
    header.client_seq_id = seq_id;
    header.magic = CLIENT_HEADER_MAGIC;

    char header_buf[PACKED_CLIENT_HEADER_LENGTH];
    size_t header_len = sizeof(header_buf);
    if (Pack(header, header_buf, header_len) != 0)
        return -1;

    std::string buf;
    buf.append(header_buf, header_len);
    buf.append(body_bytes);

    int ret = tgcpapi_send(handle, buf.data(), static_cast<int>(buf.size()), 0);
    if (ret != TGCP_ERR_NONE)
    {
        printf("  tgcpapi_send error, ret(%d): %s\n", ret, tgcpapi_error_string(ret));
        return -1;
    }
    return 0;
}

// 接收一条消息，返回cmd_id。timeout返回0。
static uint32_t RecvOne(HTGCPAPI handle, ClientHeader& out_header, const char** out_body_ptr, int* out_body_size,
                        int max_retry = 100)
{
    const char* data_ptr = NULL;
    int data_size = 0;
    int ret = TGCP_ERR_NONE;
    for (int retry = 0; retry < max_retry; ++retry)
    {
        usleep(50000);  // 50ms
        ret = tgcpapi_peek(handle, &data_ptr, &data_size, 0);
        if (ret == TGCP_ERR_NONE)
            break;
        if (ret != TGCP_ERR_PKG_NOT_COMPLETE)
        {
            printf("  tgcpapi_peek error, ret(%d)\n", ret);
            return 0;
        }
    }
    if (ret != TGCP_ERR_NONE)
        return 0;

    if (static_cast<size_t>(data_size) < PACKED_CLIENT_HEADER_LENGTH)
        return 0;
    if (Unpack(out_header, data_ptr, static_cast<size_t>(data_size)) != 0)
        return 0;
    if (static_cast<size_t>(data_size) != PACKED_CLIENT_HEADER_LENGTH + out_header.body_length)
        return 0;

    *out_body_ptr = data_ptr + PACKED_CLIENT_HEADER_LENGTH;
    *out_body_size = static_cast<int>(out_header.body_length);
    return out_header.cmd_id;
}

// 排除所有推送消息，等待期望的响应cmd_id
static uint32_t RecvResponse(HTGCPAPI handle, uint32_t expected_cmd, ClientHeader& out_header,
                             const char** out_body_ptr, int* out_body_size, int max_retry = 100)
{
    int total = max_retry;
    while (total > 0)
    {
        uint32_t cmd = RecvOne(handle, out_header, out_body_ptr, out_body_size, 1);
        if (cmd == 0)
        {
            --total;
            continue;
        }
        if (cmd == expected_cmd)
            return cmd;
        if (cmd == CMD_PUSH_ROOM_SELECTING)
            ++g_selecting_push_count;
        if (cmd == CMD_PUSH_ROOM_EMOTE)
            ++g_emote_push_count;
        // 是推送，打印并跳过
        printf("  [push] cmd_id=%u (interleaved, waiting for %u)\n", cmd, expected_cmd);
    }
    return 0;
}

// 排除推送消息，返回最终响应。将推送解析并打印。
static int DrainPushes(HTGCPAPI handle, int max_count = 10, PushObservation* observation = nullptr)
{
    int count = 0;
    for (int i = 0; i < max_count; ++i)
    {
        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvOne(handle, header, &body_ptr, &body_size, 2);
        if (cmd == 0)
            break;

        if (cmd == CMD_PUSH_ROOM_DETAIL_UPDATED)
        {
            connsvr::RoomDetailSnapshot snap;
            if (body_size > 0 && snap.ParseFromArray(body_ptr, body_size))
            {
                if (observation)
                {
                    ++observation->detail_count;
                    observation->last_detail = snap;
                }
                printf("  [push:RoomDetail] room=%s name=%s host=%s members=%d in_battle=%d\n", snap.room_id().c_str(),
                       snap.room_name().c_str(), snap.host_gid().c_str(), snap.members_size(), snap.in_battle());
                for (int j = 0; j < snap.members_size(); ++j)
                {
                    const auto& m = snap.members(j);
                    printf("    member: slot=%u gid=%s name=%s bot=%d bot_id=%s ready=%d battle_role_type=%u\n",
                           m.slot_index(), m.gid().c_str(), m.display_name().c_str(), m.b_is_bot(), m.bot_id().c_str(),
                           m.is_ready(), m.battle_role_type());
                }
            }
        }
        else if (cmd == CMD_PUSH_ROOM_LIST_UPDATED)
        {
            connsvr::RoomListSnapshot snap;
            if (body_size > 0 && snap.ParseFromArray(body_ptr, body_size))
            {
                if (observation)
                {
                    ++observation->list_count;
                    observation->last_list = snap;
                }
                printf("  [push:RoomList] rooms=%d\n", snap.rooms_size());
                for (int j = 0; j < snap.rooms_size(); ++j)
                {
                    const auto& r = snap.rooms(j);
                    printf("    room: id=%s name=%s host=%s host_name=%s %d/%d\n", r.room_id().c_str(),
                           r.room_name().c_str(), r.host_gid().c_str(), r.host_display_name().c_str(),
                           r.current_players(), r.max_players());
                }
            }
        }
        else if (cmd == CMD_PUSH_ROOM_KICKED)
        {
            connsvr::PushRoomKicked msg;
            if (body_size > 0 && msg.ParseFromArray(body_ptr, body_size))
                printf("  [push:Kicked] reason=%s\n", msg.reason().c_str());
        }
        else if (cmd == CMD_PUSH_ROOM_BATTLE_READY)
        {
            connsvr::PushRoomBattleReady msg;
            if (body_size > 0 && msg.ParseFromArray(body_ptr, body_size))
            {
                if (observation)
                    ++observation->battle_ready_count;
                printf("  [push:BattleReady] addr=%s token=%llu battle_id=%s\n", msg.server_address().c_str(),
                       static_cast<unsigned long long>(msg.token()), msg.battle_id().c_str());
            }
        }
        else if (cmd == CMD_PUSH_ROOM_SELECTING)
        {
            ++g_selecting_push_count;
            connsvr::PushRoomSelecting msg;
            if (body_size > 0 && msg.ParseFromArray(body_ptr, body_size))
            {
                if (observation)
                    ++observation->selecting_count;
                printf("  [push:Selecting] deadline_ms=%lld duration_sec=%u\n",
                       static_cast<long long>(msg.select_end_unix_ms()), msg.select_duration_sec());
            }
        }
        else if (cmd == CMD_PUSH_ROOM_BATTLE_FAILED)
        {
            connsvr::PushRoomBattleFailed msg;
            if (body_size > 0 && msg.ParseFromArray(body_ptr, body_size))
            {
                if (observation)
                    ++observation->battle_failed_count;
                printf("  [push:BattleFailed] reason=%d message=%s\n", msg.reason(), msg.message().c_str());
            }
        }
        else if (cmd == CMD_PUSH_ROOM_EMOTE)
        {
            connsvr::PushRoomEmote msg;
            if (body_size > 0 && msg.ParseFromArray(body_ptr, body_size))
            {
                ++g_emote_push_count;
                if (observation)
                {
                    ++observation->emote_count;
                    observation->last_emote = msg;
                }
                printf("  [push:Emote] sender_gid=%s emote_id=%u expire_unix_ms=%lld\n", msg.sender_gid().c_str(),
                       msg.emote_id(), static_cast<long long>(msg.expire_unix_ms()));
            }
        }
        else
        {
            printf("  [push:unknown] cmd_id=%u\n", cmd);
        }
        ++count;
    }
    return count;
}

static const connsvr::RoomMemberProto* FindRealMember(const connsvr::RoomDetailSnapshot& detail, uint64_t gid)
{
    std::string gid_str = std::to_string(gid);
    for (int i = 0; i < detail.members_size(); ++i)
    {
        const auto& member = detail.members(i);
        if (!member.b_is_bot() && member.gid() == gid_str)
            return &member;
    }
    return nullptr;
}

static const connsvr::RoomMemberProto* FindFirstBot(const connsvr::RoomDetailSnapshot& detail)
{
    for (int i = 0; i < detail.members_size(); ++i)
    {
        const auto& member = detail.members(i);
        if (member.b_is_bot())
            return &member;
    }
    return nullptr;
}

static const connsvr::RoomBrief* FindRoomBrief(const connsvr::RoomListSnapshot& list, const std::string& room_id)
{
    for (int i = 0; i < list.rooms_size(); ++i)
    {
        if (list.rooms(i).room_id() == room_id)
            return &list.rooms(i);
    }
    return nullptr;
}

// ========== Client 封装 ==========

struct TestClient
{
    HTGCPAPI handle = nullptr;
    uint64_t gid = 0;
    std::string name;
    uint32_t openid = 0;
    bool is_new_user = false;

    int Connect(uint32_t openid_val, const char* client_name)
    {
        name = client_name;
        openid = openid_val;
        char openid_str[32];
        snprintf(openid_str, sizeof(openid_str), "%u", openid);

        int ret = tgcpapi_create(&handle);
        TEST_ASSERT(ret == TGCP_ERR_NONE, "tgcpapi_create failed");
        if (ConnectHandle(handle, openid_str) != 0)
            return -1;
        return 0;
    }

    void Disconnect()
    {
        if (handle)
        {
            tgcpapi_close_connection(handle);
            tgcpapi_fini(handle);
            tgcpapi_destroy(&handle);
            handle = nullptr;
        }
    }

    int DoLogin()
    {
        printf("  [%s] Login (openid=%u)\n", name.c_str(), openid);
        connsvr::LoginReq req;
        if (SendRequest(handle, CMD_LOGIN, 1001, 0, req) != 0)
            return -1;

        // Login响应可能是CMD_LOGIN(1)或CMD_LOGIN_NEW(2)，需要兼容
        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = 0;
        for (int retry = 0; retry < 200; ++retry)
        {
            cmd = RecvOne(handle, header, &body_ptr, &body_size, 1);
            if (cmd == CMD_LOGIN || cmd == CMD_LOGIN_NEW)
                break;
            if (cmd != 0)
                printf("  [push] cmd_id=%u (interleaved during login)\n", cmd);
            cmd = 0;
        }
        TEST_ASSERT(cmd == CMD_LOGIN || cmd == CMD_LOGIN_NEW, "Login response cmd_id mismatch");

        connsvr::LoginResp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);

        gid = header.gid;
        is_new_user = (cmd == CMD_LOGIN_NEW);
        TEST_ASSERT(gid != 0, "gid is 0 after login");
        printf("  [%s] Login ok, gid=%llu, is_new=%d\n", name.c_str(), static_cast<unsigned long long>(gid),
               is_new_user);

        // 设置测试用户昵称，覆盖新老用户，验证房间成员昵称来自后台会话缓存。
        {
            connsvr::SetUserInfoReq su_req;
            su_req.set_user_name(name);
            su_req.set_role_type(1);
            if (SendRequest(handle, CMD_SET_USER_INFO, 1002, gid, su_req) != 0)
                return -1;
            uint32_t su_cmd = RecvResponse(handle, CMD_SET_USER_INFO, header, &body_ptr, &body_size);
            TEST_ASSERT(su_cmd == CMD_SET_USER_INFO, "SetUserInfo response mismatch");
            printf("  [%s] SetUserInfo ok\n", name.c_str());
        }
        PASS("Login");
        return 0;
    }

    int DoRoomList(int expect_min = -1, connsvr::RoomListSnapshot* out_snapshot = nullptr)
    {
        printf("  [%s] RoomList\n", name.c_str());
        connsvr::RoomListReq req;
        if (SendRequest(handle, CMD_ROOM_LIST_REQ, 1010, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_LIST_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_LIST_REQ, "RoomList response mismatch");

        connsvr::RoomListRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        TEST_ASSERT(resp.code() == 0, "RoomList failed");

        printf("  [%s] RoomList: %d rooms\n", name.c_str(), resp.snapshot().rooms_size());
        for (int i = 0; i < resp.snapshot().rooms_size(); ++i)
        {
            const auto& r = resp.snapshot().rooms(i);
            printf("    room: id=%s name=%s host=%s host_name=%s %d/%d\n", r.room_id().c_str(), r.room_name().c_str(),
                   r.host_gid().c_str(), r.host_display_name().c_str(), r.current_players(), r.max_players());
        }
        if (expect_min >= 0)
            TEST_ASSERT(resp.snapshot().rooms_size() >= expect_min, "RoomList room count too low");
        if (out_snapshot)
            *out_snapshot = resp.snapshot();
        PASS("RoomList");
        return 0;
    }

    /// 单独调SetUserInfo校验昵称闸门；expect_code非0时断言被拒
    int DoSetUserInfo(const std::string& user_name, int expect_code = 0)
    {
        printf("  [%s] SetUserInfo user_name=\"%s\"\n", name.c_str(), user_name.c_str());
        connsvr::SetUserInfoReq req;
        req.set_user_name(user_name);
        req.set_role_type(1);
        if (SendRequest(handle, CMD_SET_USER_INFO, 1003, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_SET_USER_INFO, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_SET_USER_INFO, "SetUserInfo response mismatch");

        connsvr::SetUserInfoResp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        printf("  [%s] SetUserInfo code=%d user_name=\"%s\"\n", name.c_str(), resp.ret_code(),
               resp.user_name().c_str());
        TEST_ASSERT(resp.ret_code() == expect_code, "SetUserInfo code mismatch");
        DrainPushes(handle, 3);
        PASS(expect_code == 0 ? "SetUserInfo" : "SetUserInfo rejected");
        return 0;
    }

    int DoRoomCreate(const std::string& room_name, std::string& out_room_id, int max_players = 0, int expect_code = 0,
                     uint32_t map_id = 0)
    {
        printf("  [%s] RoomCreate name=%s max_players=%d\n", name.c_str(), room_name.c_str(), max_players);
        connsvr::RoomCreateReq req;
        req.set_room_name(room_name);
        req.set_max_players(max_players);
        req.set_map_id(map_id);

        if (SendRequest(handle, CMD_ROOM_CREATE_REQ, 1020, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_CREATE_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_CREATE_REQ, "RoomCreate response mismatch");

        connsvr::RoomCreateRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        TEST_ASSERT(resp.code() == expect_code, "RoomCreate code mismatch");
        if (expect_code != 0)
        {
            PASS("RoomCreate rejected");
            return 0;
        }
        out_room_id = resp.room_id();
        printf("  [%s] RoomCreate ok, room_id=%s\n", name.c_str(), out_room_id.c_str());

        // 接收推送：RoomDetail + RoomList
        int pushes = DrainPushes(handle, 5);
        printf("  [%s] received %d push(es) after create\n", name.c_str(), pushes);
        PASS("RoomCreate");
        return 0;
    }

    int DoRoomJoin(const std::string& room_id, int expect_code = 0)
    {
        printf("  [%s] RoomJoin room_id=%s\n", name.c_str(), room_id.c_str());
        connsvr::RoomJoinReq req;
        req.set_room_id(room_id);

        if (SendRequest(handle, CMD_ROOM_JOIN_REQ, 1030, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_JOIN_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_JOIN_REQ, "RoomJoin response mismatch");

        connsvr::RoomJoinRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        printf("  [%s] RoomJoin code=%d\n", name.c_str(), resp.code());
        TEST_ASSERT(resp.code() == expect_code, "RoomJoin code mismatch");

        // 接收推送
        int pushes = DrainPushes(handle, 5);
        printf("  [%s] received %d push(es) after join\n", name.c_str(), pushes);
        PASS("RoomJoin");
        return 0;
    }

    int DoRoomSetReady(bool ready, int expect_code = 0)
    {
        printf("  [%s] RoomSetReady ready=%d\n", name.c_str(), ready);
        connsvr::RoomSetReadyReq req;
        req.set_b_ready(ready);

        if (SendRequest(handle, CMD_ROOM_SET_READY_REQ, 1040, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_SET_READY_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_SET_READY_REQ, "SetReady response mismatch");

        connsvr::RoomSetReadyRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        TEST_ASSERT(resp.code() == expect_code, "SetReady code mismatch");

        // 接收推送
        int pushes = DrainPushes(handle, 5);
        printf("  [%s] received %d push(es) after setReady\n", name.c_str(), pushes);
        PASS("RoomSetReady");
        return 0;
    }

    int DoRoomAddBot(std::string& out_bot_id, uint32_t* out_slot_index = nullptr, int expect_code = 0)
    {
        printf("  [%s] RoomAddBot\n", name.c_str());
        connsvr::RoomAddBotReq req;
        if (SendRequest(handle, CMD_ROOM_ADD_BOT_REQ, 1045, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_ADD_BOT_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_ADD_BOT_REQ, "AddBot response mismatch");

        connsvr::RoomAddBotRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        printf("  [%s] AddBot code=%d bot_id=%s slot=%u\n", name.c_str(), resp.code(), resp.bot_id().c_str(),
               resp.slot_index());
        TEST_ASSERT(resp.code() == expect_code, "AddBot code mismatch");
        if (expect_code == 0)
        {
            TEST_ASSERT(!resp.bot_id().empty(), "AddBot bot_id is empty");
            TEST_ASSERT(resp.slot_index() > 0, "AddBot slot_index is empty");
            out_bot_id = resp.bot_id();
            if (out_slot_index)
                *out_slot_index = resp.slot_index();
        }
        PASS("RoomAddBot");
        return 0;
    }

    int DoRoomRemoveBot(const std::string& bot_id, uint32_t slot_index = 0, int expect_code = 0)
    {
        printf("  [%s] RoomRemoveBot bot_id=%s slot=%u\n", name.c_str(), bot_id.c_str(), slot_index);
        connsvr::RoomRemoveBotReq req;
        req.set_bot_id(bot_id);
        req.set_slot_index(slot_index);
        if (SendRequest(handle, CMD_ROOM_REMOVE_BOT_REQ, 1046, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_REMOVE_BOT_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_REMOVE_BOT_REQ, "RemoveBot response mismatch");

        connsvr::RoomRemoveBotRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        printf("  [%s] RemoveBot code=%d\n", name.c_str(), resp.code());
        TEST_ASSERT(resp.code() == expect_code, "RemoveBot code mismatch");
        PASS("RoomRemoveBot");
        return 0;
    }

    int DoRoomRename(const std::string& new_name, int expect_code = 0)
    {
        printf("  [%s] RoomRename new_name=%s\n", name.c_str(), new_name.c_str());
        connsvr::RoomRenameReq req;
        req.set_new_name(new_name);

        if (SendRequest(handle, CMD_ROOM_RENAME_REQ, 1050, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_RENAME_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_RENAME_REQ, "Rename response mismatch");

        connsvr::RoomRenameRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        TEST_ASSERT(resp.code() == expect_code, "Rename code mismatch");

        // 接收推送
        int pushes = DrainPushes(handle, 5);
        printf("  [%s] received %d push(es) after rename\n", name.c_str(), pushes);
        PASS("RoomRename");
        return 0;
    }

    int DoRoomLeave(int expect_code = 0)
    {
        printf("  [%s] RoomLeave\n", name.c_str());
        connsvr::RoomLeaveReq req;

        if (SendRequest(handle, CMD_ROOM_LEAVE_REQ, 1060, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_LEAVE_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_LEAVE_REQ, "Leave response mismatch");

        connsvr::RoomLeaveRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        TEST_ASSERT(resp.code() == expect_code, "Leave code mismatch");

        // 接收推送（可能包括Kicked）
        int pushes = DrainPushes(handle, 5);
        printf("  [%s] received %d push(es) after leave\n", name.c_str(), pushes);
        PASS("RoomLeave");
        return 0;
    }

    int DoRoomStartBattle(int expect_code = 0)
    {
        printf("  [%s] RoomStartBattle\n", name.c_str());
        connsvr::RoomStartBattleReq req;

        if (SendRequest(handle, CMD_ROOM_START_BATTLE_REQ, 1070, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_START_BATTLE_REQ, header, &body_ptr, &body_size, 200);
        TEST_ASSERT(cmd == CMD_ROOM_START_BATTLE_REQ, "StartBattle response mismatch");

        connsvr::RoomStartBattleRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        printf("  [%s] StartBattle code=%d\n", name.c_str(), resp.code());
        TEST_ASSERT(resp.code() == expect_code, "StartBattle code mismatch");

        // 不在这里drain推送，由调用方单独处理（需要解析BattleReady）
        PASS("RoomStartBattle");
        return 0;
    }

    // 局内选角
    int DoRoomSetRole(uint32_t battle_role_type, int expect_code = 0)
    {
        printf("  [%s] RoomSetRole battle_role_type=%u\n", name.c_str(), battle_role_type);
        connsvr::RoomSetRoleReq req;
        req.set_battle_role_type(battle_role_type);

        if (SendRequest(handle, CMD_ROOM_SET_ROLE_REQ, 1080, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_SET_ROLE_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_SET_ROLE_REQ, "SetRole response mismatch");

        connsvr::RoomSetRoleRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        printf("  [%s] SetRole code=%d\n", name.c_str(), resp.code());
        TEST_ASSERT(resp.code() == expect_code, "SetRole code mismatch");
        PASS("RoomSetRole");
        return 0;
    }

    int DoRoomSetMap(uint32_t map_id, int expect_code = 0)
    {
        printf("  [%s] RoomSetMap map_id=%u\n", name.c_str(), map_id);
        connsvr::RoomSetMapReq req;
        req.set_map_id(map_id);
        if (SendRequest(handle, CMD_ROOM_SET_MAP_REQ, 1090, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_SET_MAP_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_SET_MAP_REQ, "SetMap response mismatch");

        connsvr::RoomSetMapRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        printf("  [%s] SetMap code=%d\n", name.c_str(), resp.code());
        TEST_ASSERT(resp.code() == expect_code, "SetMap code mismatch");
        DrainPushes(handle, 5);
        PASS("RoomSetMap");
        return 0;
    }

    // 房间快捷表情（cmd=212）。不在这里drain推送：广播2006需要由各连接单独观察。
    int DoRoomSendEmote(uint32_t emote_id, int expect_code = 0)
    {
        printf("  [%s] RoomSendEmote emote_id=%u\n", name.c_str(), emote_id);
        connsvr::RoomSendEmoteReq req;
        req.set_emote_id(emote_id);
        if (SendRequest(handle, CMD_ROOM_SEND_EMOTE_REQ, 1100, gid, req) != 0)
            return -1;

        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        uint32_t cmd = RecvResponse(handle, CMD_ROOM_SEND_EMOTE_REQ, header, &body_ptr, &body_size);
        TEST_ASSERT(cmd == CMD_ROOM_SEND_EMOTE_REQ, "SendEmote response mismatch");

        connsvr::RoomSendEmoteRsp resp;
        if (body_size > 0)
            resp.ParseFromArray(body_ptr, body_size);
        printf("  [%s] SendEmote code=%d message=%s\n", name.c_str(), resp.code(), resp.message().c_str());
        TEST_ASSERT(resp.code() == expect_code, "SendEmote code mismatch");
        PASS(expect_code == 0 ? "RoomSendEmote" : "RoomSendEmote rejected");
        return 0;
    }

    // 等待并解析BattleReady推送；收到已停用的Selecting(2004)立即失败。
    // max_retry每次最多等待约50ms，用于覆盖DS分配和进程启动耗时。
    int WaitBattleReady(std::string& out_addr, uint64_t& out_token, std::string& out_battle_id, int max_retry = 600)
    {
        printf("  [%s] waiting for BattleReady push...\n", name.c_str());
        for (int i = 0; i < max_retry; ++i)
        {
            ClientHeader header;
            const char* body_ptr = nullptr;
            int body_size = 0;
            uint32_t cmd = RecvOne(handle, header, &body_ptr, &body_size, 1);
            if (cmd == 0)
                continue;
            if (cmd == CMD_PUSH_ROOM_BATTLE_READY)
            {
                connsvr::PushRoomBattleReady msg;
                if (body_size > 0 && msg.ParseFromArray(body_ptr, body_size))
                {
                    out_addr = msg.server_address();
                    out_token = msg.token();
                    out_battle_id = msg.battle_id();
                    printf("  [%s] BattleReady: addr=%s token=%llu battle_id=%s\n", name.c_str(), out_addr.c_str(),
                           static_cast<unsigned long long>(out_token), out_battle_id.c_str());
                    PASS("BattleReady push");
                    return 0;
                }
            }
            else if (cmd == CMD_PUSH_ROOM_SELECTING)
            {
                ++g_selecting_push_count;
                printf("  [%s] ERROR: unexpected Selecting(2004) push\n", name.c_str());
                return -2;
            }
            else
            {
                printf("  [%s] [push] cmd_id=%u (while waiting for BattleReady)\n", name.c_str(), cmd);
            }
        }
        printf("  [%s] ERROR: BattleReady push timeout\n", name.c_str());
        return -1;
    }
};

// ========== 主测试流程 ==========

int main(int argc, char* argv[])
{
    const char* host = nullptr;
    int port = 0;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc)
            host = argv[++i];
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
            port = atoi(argv[++i]);
    }

    if (host || port)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "tcp://%s:%d", host ? host : "127.0.0.1", port ? port : 18801);
        g_tconnd_url = buf;
    }

    srand(static_cast<unsigned>(time(nullptr)));

    TestClient client_a, client_b;
    int rc = 0;

    // ===== Phase 1: 两个客户端登录 =====
    printf("\n=== Phase 1: Login (2 clients) ===\n");
    if (client_a.Connect(12345, "PlayerA") != 0)
    {
        rc = 1;
        goto cleanup;
    }
    if (client_a.DoLogin() != 0)
    {
        rc = 1;
        goto cleanup;
    }

    if (client_b.Connect(12346, "PlayerB") != 0)
    {
        rc = 2;
        goto cleanup;
    }
    if (client_b.DoLogin() != 0)
    {
        rc = 2;
        goto cleanup;
    }

    // ===== Phase 2: RoomCreate + 推送验证 =====
    printf("\n=== Phase 2: RoomCreate + push ===\n");
    {
        std::string invalid_room_id;
        if (client_a.DoRoomCreate("bad_room", invalid_room_id, 9, 1015) != 0)
        {
            rc = 3;
            goto cleanup;
        }
        if (client_a.DoRoomCreate("bad_room", invalid_room_id, -1, 1015) != 0)
        {
            rc = 3;
            goto cleanup;
        }
        if (client_a.DoRoomCreate("bad_room", invalid_room_id, 0, 1017, 999) != 0)
        {
            rc = 3;
            goto cleanup;
        }
        // 房间名12字超长 → 1010，房间不创建
        if (client_a.DoRoomCreate("twelve_chars", invalid_room_id, 0, 1010) != 0)
        {
            rc = 3;
            goto cleanup;
        }

        std::string room_id;
        if (client_a.DoRoomCreate("test_room", room_id) != 0)
        {
            rc = 3;
            goto cleanup;
        }
        TEST_ASSERT(!room_id.empty(), "room_id is empty");

        // Client B 也应该收到 RoomList 推送
        printf("  [PlayerB] draining pushes from RoomCreate...\n");
        int b_pushes = DrainPushes(client_b.handle, 5);
        printf("  [PlayerB] received %d push(es)\n", b_pushes);

        // Client B 查询房间列表，应能看到房间
        if (client_b.DoRoomList(1) != 0)
        {
            rc = 3;
            goto cleanup;
        }

        // 保存room_id给后续用
        // 利用静态变量传递（简化，避免全局）
        // 后续通过直接传参
        // 重新设计：用局部变量
        // 这里room_id需要传到后续phase，用goto+全局不优雅，改用嵌套作用域
        // 为了简洁，把room_id存入全局
        static std::string g_room_id;
        g_room_id = room_id;

        // ===== Phase 3: RoomJoin + 推送验证 =====
        printf("\n=== Phase 3: RoomJoin + push ===\n");
        if (client_b.DoRoomJoin(g_room_id) != 0)
        {
            rc = 4;
            goto cleanup;
        }

        // Client A 应该收到 RoomDetail 推送（新成员加入）
        printf("  [PlayerA] draining pushes from RoomJoin...\n");
        int a_pushes = DrainPushes(client_a.handle, 5);
        printf("  [PlayerA] received %d push(es)\n", a_pushes);
        TEST_ASSERT(a_pushes >= 1, "PlayerA should receive push after B joins");

        // ===== Phase 4: AddBot / RemoveBot =====
        printf("\n=== Phase 4: AddBot / RemoveBot ===\n");
        std::string bot_id;
        uint32_t bot_slot = 0;
        if (client_a.DoRoomAddBot(bot_id, &bot_slot) != 0)
        {
            rc = 5;
            goto cleanup;
        }
        int bot_pushes_a = DrainPushes(client_a.handle, 5);
        int bot_pushes_b = DrainPushes(client_b.handle, 5);
        TEST_ASSERT(bot_pushes_a >= 1 && bot_pushes_b >= 1, "both clients should receive AddBot push");
        TEST_ASSERT(bot_slot == 3, "first bot should occupy slot 3");
        std::string second_bot_id;
        uint32_t second_bot_slot = 0;
        if (client_a.DoRoomAddBot(second_bot_id, &second_bot_slot) != 0)
        {
            rc = 5;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);
        DrainPushes(client_b.handle, 5);
        TEST_ASSERT(second_bot_slot == 4, "second bot should occupy slot 4");
        if (client_a.DoRoomRemoveBot("", bot_slot) != 0)
        {
            rc = 5;
            goto cleanup;
        }
        bot_pushes_a = DrainPushes(client_a.handle, 5);
        bot_pushes_b = DrainPushes(client_b.handle, 5);
        TEST_ASSERT(bot_pushes_a >= 1 && bot_pushes_b >= 1, "both clients should receive RemoveBot push");
        std::string reused_bot_id;
        uint32_t reused_bot_slot = 0;
        if (client_a.DoRoomAddBot(reused_bot_id, &reused_bot_slot) != 0)
        {
            rc = 5;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);
        DrainPushes(client_b.handle, 5);
        TEST_ASSERT(reused_bot_slot == bot_slot, "new bot should reuse smallest free slot");
        if (client_a.DoRoomRemoveBot(reused_bot_id) != 0)
        {
            rc = 5;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);
        DrainPushes(client_b.handle, 5);

        // ===== Phase 5: RoomSetReady =====
        printf("\n=== Phase 4: RoomSetReady ===\n");
        if (client_b.DoRoomSetReady(true) != 0)
        {
            rc = 5;
            goto cleanup;
        }

        // Client A 收到 Detail 推送（B is_ready=true）
        printf("  [PlayerA] draining pushes from SetReady...\n");
        a_pushes = DrainPushes(client_a.handle, 5);
        printf("  [PlayerA] received %d push(es)\n", a_pushes);

        // ===== Phase 5: RoomRename (host only) =====
        printf("\n=== Phase 5: RoomRename ===\n");
        // 12字超长 → 1010，房间名保持旧值
        if (client_a.DoRoomRename("twelve_chars", 1010) != 0)
        {
            rc = 6;
            goto cleanup;
        }
        // 纯空白 → 1010（不恢复默认名）
        if (client_a.DoRoomRename("   ", 1010) != 0)
        {
            rc = 6;
            goto cleanup;
        }
        // 11字刚好通过
        if (client_a.DoRoomRename("11_char_max") != 0)
        {
            rc = 6;
            goto cleanup;
        }

        // 非 host 不能 rename
        printf("  [PlayerB] RoomRename (should fail: not host)\n");
        connsvr::RoomRenameReq rename_req;
        rename_req.set_new_name("hack_name");
        if (SendRequest(client_b.handle, CMD_ROOM_RENAME_REQ, 1051, client_b.gid, rename_req) != 0)
        {
            rc = 6;
            goto cleanup;
        }
        ClientHeader header;
        const char* body_ptr = nullptr;
        int body_size = 0;
        (void)RecvResponse(client_b.handle, CMD_ROOM_RENAME_REQ, header, &body_ptr, &body_size);
        connsvr::RoomRenameRsp rename_resp;
        if (body_size > 0)
            rename_resp.ParseFromArray(body_ptr, body_size);
        printf("  [PlayerB] Rename as non-host: code=%d\n", rename_resp.code());
        TEST_ASSERT(rename_resp.code() == 1006, "non-host rename should fail with kNotHost(1006)");
        DrainPushes(client_b.handle, 3);  // drain any pushes
        PASS("RoomRename (non-host rejected)");

        // B 也收到 rename 推送
        printf("  [PlayerB] draining pushes from rename...\n");
        int b_push2 = DrainPushes(client_b.handle, 5);
        printf("  [PlayerB] received %d push(es)\n", b_push2);

        // ===== Phase 6: RoomLeave (non-host) =====
        printf("\n=== Phase 6: RoomLeave (non-host) ===\n");
        if (client_b.DoRoomLeave() != 0)
        {
            rc = 7;
            goto cleanup;
        }

        // Client A 收到 Detail+List 推送
        printf("  [PlayerA] draining pushes from B leave...\n");
        a_pushes = DrainPushes(client_a.handle, 5);
        printf("  [PlayerA] received %d push(es)\n", a_pushes);

        // 验证 A 还在房间中（RoomList 应有 1 房间 1 人）
        if (client_a.DoRoomList(1) != 0)
        {
            rc = 7;
            goto cleanup;
        }

        // ===== Phase 7: Host Migration =====
        printf("\n=== Phase 7: Host Migration ===\n");
        // B 重新加入
        if (client_b.DoRoomJoin(g_room_id) != 0)
        {
            rc = 8;
            goto cleanup;
        }
        // A 也收到 push
        DrainPushes(client_a.handle, 5);

        // B 设 ready
        if (client_b.DoRoomSetReady(true) != 0)
        {
            rc = 8;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);

        // A（房主）离开 → B 应成为新房主
        printf("  [PlayerA] Leave as host (should trigger migration)\n");
        if (client_a.DoRoomLeave() != 0)
        {
            rc = 8;
            goto cleanup;
        }

        // B 收到 Detail（新host=B）+ List
        printf("  [PlayerB] draining pushes after host migration...\n");
        int b_push3 = DrainPushes(client_b.handle, 5);
        printf("  [PlayerB] received %d push(es) after host left\n", b_push3);
        TEST_ASSERT(b_push3 >= 1, "PlayerB should receive push after host migration");

        // A 收到 Kicked
        printf("  [PlayerA] draining Kicked push...\n");
        int a_push2 = DrainPushes(client_a.handle, 5);
        printf("  [PlayerA] received %d push(es) after leaving\n", a_push2);

        PASS("Host Migration");

        // ===== Phase 8: Last member leaves → room destroyed =====
        printf("\n=== Phase 8: Last member leaves → room destroyed ===\n");
        if (client_b.DoRoomLeave() != 0)
        {
            rc = 9;
            goto cleanup;
        }

        // A 的 RoomList 应为空
        if (client_a.DoRoomList(0) != 0)
        {
            rc = 9;
            goto cleanup;
        }

        // B 的 RoomList 也应为空
        if (client_b.DoRoomList(0) != 0)
        {
            rc = 9;
            goto cleanup;
        }

        PASS("Room Destroyed");
    }

    // ===== Phase 8.5: 默认房间名 + 大厅列表房主昵称 + 昵称长度闸门 =====
    printf("\n=== Phase 8.5: default room name + host_display_name + name length ===\n");
    {
        // 昵称闸门：9字拒(1301)、纯空白拒(1301)、8字过
        if (client_a.DoSetUserInfo("NineChars", 1301) != 0 || client_a.DoSetUserInfo("   ", 1301) != 0 ||
            client_a.DoSetUserInfo("EightChr") != 0)
        {
            rc = 91;
            goto cleanup;
        }

        // 空 room_name → 后台生成 "{昵称}的房间"
        std::string room_id;
        if (client_a.DoRoomCreate("", room_id) != 0)
        {
            rc = 92;
            goto cleanup;
        }
        DrainPushes(client_b.handle, 5);

        // 大厅里的 B（不在房内）应能看到默认房间名与房主昵称
        connsvr::RoomListSnapshot list;
        if (client_b.DoRoomList(1, &list) != 0)
        {
            rc = 92;
            goto cleanup;
        }
        const auto* room_brief = FindRoomBrief(list, room_id);
        TEST_ASSERT(room_brief != nullptr, "default-name room missing from lobby list");
        TEST_ASSERT(room_brief->room_name() == "EightChr的房间", "default room_name should be {nickname}的房间");
        TEST_ASSERT(room_brief->host_display_name() == "EightChr", "host_display_name should be host nickname");
        TEST_ASSERT(room_brief->host_display_name() != room_brief->host_gid(), "host_display_name must not be gid");
        PASS("default room name + host_display_name");

        // 换房主：host_display_name 跟着变，房间名不变
        if (client_b.DoRoomJoin(room_id) != 0)
        {
            rc = 93;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);
        if (client_a.DoRoomLeave() != 0)
        {
            rc = 93;
            goto cleanup;
        }
        PushObservation migrate_observation;
        DrainPushes(client_b.handle, 8, &migrate_observation);
        TEST_ASSERT(migrate_observation.list_count >= 1, "B should receive RoomList after host migration");
        const auto* after_migrate = FindRoomBrief(migrate_observation.last_list, room_id);
        TEST_ASSERT(after_migrate != nullptr, "room missing from list after host migration");
        TEST_ASSERT(after_migrate->host_display_name() == "PlayerB", "host_display_name should follow new host");
        TEST_ASSERT(after_migrate->room_name() == "EightChr的房间", "room_name must NOT change on host migration");
        PASS("host migration updates host_display_name only");

        // 清场：B 离开销毁房间；A 昵称改回 PlayerA 供后续 phase 使用
        if (client_b.DoRoomLeave() != 0)
        {
            rc = 93;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);
        if (client_a.DoSetUserInfo("PlayerA") != 0)
        {
            rc = 93;
            goto cleanup;
        }
    }

    // ===== Phase 9: Waiting选角 → 全员准备 → 直接BattleReady =====
    printf("\n=== Phase 9: Waiting SetRole -> Ready -> BattleReady (no 2004) ===\n");
    {
        int selecting_count_before = g_selecting_push_count;
        std::string room_id;
        if (client_a.DoRoomCreate("battle_room", room_id, 0, 0, 102) != 0)
        {
            rc = 10;
            goto cleanup;
        }
        DrainPushes(client_b.handle, 5);

        if (client_b.DoRoomJoin(room_id) != 0)
        {
            rc = 10;
            goto cleanup;
        }
        PushObservation join_observation;
        DrainPushes(client_a.handle, 5, &join_observation);
        const auto* member_a = FindRealMember(join_observation.last_detail, client_a.gid);
        const auto* member_b = FindRealMember(join_observation.last_detail, client_b.gid);
        TEST_ASSERT(join_observation.detail_count >= 1, "A should receive RoomDetail after B joins");
        TEST_ASSERT(member_a && member_a->battle_role_type() == 1 && member_a->is_ready(),
                    "host should enter with role=1 and ready=true");
        TEST_ASSERT(member_b && member_b->battle_role_type() == 1 && !member_b->is_ready(),
                    "joined player should enter with role=1 and ready=false");
        TEST_ASSERT(join_observation.last_detail.map_id() == 102, "room map should be 102");
        TEST_ASSERT(member_a && member_a->display_name() == "PlayerA" && member_b &&
                        member_b->display_name() == "PlayerB",
                    "room detail should contain both real member display names");

        std::string battle_bot_id;
        uint32_t battle_bot_slot = 0;
        if (client_a.DoRoomAddBot(battle_bot_id, &battle_bot_slot) != 0)
        {
            rc = 10;
            goto cleanup;
        }
        TEST_ASSERT(battle_bot_slot == 3, "battle bot should occupy slot 3");
        PushObservation bot_observation;
        DrainPushes(client_b.handle, 5, &bot_observation);
        const auto* bot = FindFirstBot(bot_observation.last_detail);
        TEST_ASSERT(bot_observation.detail_count >= 1, "B should receive RoomDetail after AddBot");
        TEST_ASSERT(bot && bot->battle_role_type() == 1 && bot->is_ready() && bot->display_name().empty(),
                    "bot should have role=1, ready=true and empty display name");

        // Waiting且未准备时任意非零角色ID均可选，全房可见；0不是合法客户端输入。
        if (client_a.DoRoomSetRole(2) != 0)
        {
            rc = 11;
            goto cleanup;
        }
        PushObservation role_observation;
        DrainPushes(client_b.handle, 5, &role_observation);
        member_a = FindRealMember(role_observation.last_detail, client_a.gid);
        TEST_ASSERT(role_observation.detail_count >= 1, "B should receive RoomDetail after A SetRole");
        TEST_ASSERT(member_a && member_a->battle_role_type() == 2 && member_a->is_ready(),
                    "host SetRole must preserve ready state");
        if (client_a.DoRoomSetRole(0, 1012) != 0)
        {
            rc = 11;
            goto cleanup;
        }

        // ---- 房间表情轮盘（cmd=212 / push=2006）----
        // 服务端不限频，只校验：在房 / Waiting / emote_id∈[1,6]。
        if (client_a.DoRoomSendEmote(0, 1019) != 0 || client_a.DoRoomSendEmote(7, 1019) != 0)
        {
            rc = 11;
            goto cleanup;
        }
        {
            int emote_before = g_emote_push_count;
            if (client_a.DoRoomSendEmote(3) != 0)
            {
                rc = 11;
                goto cleanup;
            }
            PushObservation emote_a;
            PushObservation emote_b;
            DrainPushes(client_a.handle, 5, &emote_a);
            DrainPushes(client_b.handle, 5, &emote_b);
            // 发送者自己也要收到一份（多端一致），且sender_gid用string承载A的gid
            TEST_ASSERT(g_emote_push_count - emote_before == 2, "emote must be pushed to both A (sender) and B");
            const connsvr::PushRoomEmote& seen_b = emote_b.last_emote;
            TEST_ASSERT(emote_b.emote_count >= 1, "B should receive emote push");
            TEST_ASSERT(seen_b.sender_gid() == std::to_string(client_a.gid), "emote sender_gid mismatch");
            TEST_ASSERT(seen_b.emote_id() == 3, "emote_id mismatch");
            TEST_ASSERT(seen_b.expire_unix_ms() == 0, "expire_unix_ms must stay 0 this iteration");
            // 表情是瞬时事件：不得触发2001刷新
            TEST_ASSERT(emote_a.detail_count == 0 && emote_b.detail_count == 0,
                        "emote must not trigger RoomDetail(2001) push");

            // 无服务端限频：同一人立刻连发也必须成功并各自广播
            int burst_before = g_emote_push_count;
            if (client_a.DoRoomSendEmote(4) != 0 || client_a.DoRoomSendEmote(6) != 0)
            {
                rc = 11;
                goto cleanup;
            }
            DrainPushes(client_a.handle, 8);
            DrainPushes(client_b.handle, 8);
            TEST_ASSERT(g_emote_push_count - burst_before == 4, "back-to-back emotes must all broadcast (no cooldown)");

            // 他人发送互不影响
            int b_before = g_emote_push_count;
            if (client_b.DoRoomSendEmote(5) != 0)
            {
                rc = 11;
                goto cleanup;
            }
            DrainPushes(client_a.handle, 5);
            DrainPushes(client_b.handle, 5);
            TEST_ASSERT(g_emote_push_count - b_before == 2, "B's emote broadcasts to both clients");
        }

        // 同图切换不清准备；切换到101后清所有真人Ready，Bot保持Ready。
        if (client_a.DoRoomSetMap(102) != 0 || client_a.DoRoomSetMap(101) != 0)
        {
            rc = 11;
            goto cleanup;
        }
        if (client_a.DoRoomSetRole(2) != 0 || client_a.DoRoomSetMap(102) != 0 || client_a.DoRoomSetRole(2) != 0)
        {
            rc = 11;
            goto cleanup;
        }

        // Ready后角色锁定；取消准备后可再次选角。
        if (client_b.DoRoomSetReady(true) != 0)
        {
            rc = 11;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);
        if (client_b.DoRoomSetRole(1, 1016) != 0)
        {
            rc = 11;
            goto cleanup;
        }
        if (client_b.DoRoomSetReady(false) != 0)
        {
            rc = 11;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);
        if (client_b.DoRoomSetRole(1) != 0)
        {
            rc = 11;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);
        if (client_b.DoRoomSetReady(true) != 0 || client_a.DoRoomSetReady(true) != 0)
        {
            rc = 11;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);

        // 改图会清空所有真人Ready；双方重新准备后，房主可开始战斗。
        if (client_a.DoRoomStartBattle(0) != 0)
        {
            rc = 12;
            goto cleanup;
        }

        std::string a_addr, a_battle_id;
        uint64_t a_token = 0;
        if (client_a.WaitBattleReady(a_addr, a_token, a_battle_id) != 0)
        {
            rc = 13;
            goto cleanup;
        }
        TEST_ASSERT(!a_addr.empty(), "BattleReady addr is empty");
        TEST_ASSERT(a_token != 0, "BattleReady token is 0");

        std::string b_addr, b_battle_id;
        uint64_t b_token = 0;
        if (client_b.WaitBattleReady(b_addr, b_token, b_battle_id) != 0)
        {
            rc = 13;
            goto cleanup;
        }
        TEST_ASSERT(!b_addr.empty(), "BattleReady addr is empty");
        TEST_ASSERT(b_token != 0, "BattleReady token is 0");
        TEST_ASSERT(a_addr == b_addr, "A and B should get same DS address");
        TEST_ASSERT(a_token != b_token, "A and B should get different tokens");
        TEST_ASSERT(a_battle_id == b_battle_id, "A and B should get same battle_id");

        // BattleReady后仍禁止换角和修改准备状态。
        if (client_b.DoRoomSetRole(1, 1003) != 0)
        {
            rc = 13;
            goto cleanup;
        }
        if (client_b.DoRoomSetReady(false, 1003) != 0)
        {
            rc = 13;
            goto cleanup;
        }
        // 非Waiting禁止发表情，统一回1003，且不广播
        {
            int battle_emote_before = g_emote_push_count;
            if (client_b.DoRoomSendEmote(2, 1003) != 0)
            {
                rc = 13;
                goto cleanup;
            }
            TEST_ASSERT(g_emote_push_count == battle_emote_before, "emote in battle must not broadcast");
        }
        PushObservation final_a_observation;
        PushObservation final_b_observation;
        DrainPushes(client_a.handle, 5, &final_a_observation);
        DrainPushes(client_b.handle, 5, &final_b_observation);
        TEST_ASSERT(final_a_observation.battle_ready_count == 0 && final_b_observation.battle_ready_count == 0,
                    "each client should receive exactly one BattleReady");
        TEST_ASSERT(g_selecting_push_count == selecting_count_before, "roomsvr must not send Selecting(2004)");

        // 清理本次E2E创建的战斗房间和DS，保证测试可重复运行。
        if (client_b.DoRoomLeave() != 0)
        {
            rc = 13;
            goto cleanup;
        }
        DrainPushes(client_a.handle, 5);
        if (client_a.DoRoomLeave() != 0)
        {
            rc = 13;
            goto cleanup;
        }

        printf("  Waiting->BattleReady verified without Selecting, DS addr=%s\n", a_addr.c_str());
    }

    printf("\n=== ALL TESTS PASSED ===\n");

cleanup:
    client_a.Disconnect();
    client_b.Disconnect();

    printf("\nResults: %d passed, %d failed\n", g_pass_count, g_fail_count);
    if (rc != 0)
        printf("test_gcp_client FAILED at step %d\n", rc);
    else
        printf("test_gcp_client finished\n");
    return rc;
}
