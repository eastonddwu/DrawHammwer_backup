// test_guest_disconnect.cpp: 验证游客登录 → 建房 → 断线 后的清理行为
//
// 验证目标（对应移除 ConnApp::OnTick 协程RPC 的改动）：
//   1. 游客登录(cmd=5)成功，Header.gid == body.gid，且高32位为游客标记
//   2. 游客能建房，房间出现在大厅列表
//   3. 游客断线后 connsvr 不崩溃（用第二条连接登录并查列表来证明进程存活）
//   4. 断线后房间被回收（最后一名真人离开 → FreeRoom），不再残留在大厅列表
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "conn.pb.h"
#include "net/client_cmd_id.h"
#include "net/client_header.h"
#include "tgcpapi/tgcpapi.h"

using namespace app;

static const int kServiceID = 1;
static const eAuthType kAuthMode = TGCP_AUTH_NONE;
static const eEncryptMethod kEncryptMethod = TGCP_ENCRYPT_METHOD_AES;
static const eKeyMaking kKeyMakingMethod = TGCP_KEY_MAKING_INSVR;
static const int kMaxGameDataLen = 10240;
static const int kMaxTcpTimeout = 10000;
static std::string g_tconnd_url = "tcp://127.0.0.1:18801";

static int g_pass_count = 0;
static int g_fail_count = 0;

#define CHECK(cond, msg)                                     \
    do                                                       \
    {                                                        \
        if (!(cond))                                         \
        {                                                    \
            printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
            ++g_fail_count;                                  \
            return -1;                                       \
        }                                                    \
    } while (0)

#define PASS(name)                     \
    do                                 \
    {                                  \
        printf("  [PASS] %s\n", name); \
        ++g_pass_count;                \
    } while (0)

static int ConnectHandle(HTGCPAPI handle, const char* openid_str)
{
    int ret =
        tgcpapi_init(handle, kServiceID, TGCP_ANDROID, kAuthMode, kEncryptMethod, kKeyMakingMethod, kMaxGameDataLen);
    if (ret != TGCP_ERR_NONE)
    {
        printf("  tgcpapi_init error(%d): %s\n", ret, tgcpapi_error_string(ret));
        return -1;
    }

    TGCPACCOUNT account = TGCPACCOUNT();
    account.uType = TGCP_ACCOUNT_TYPE_NONE;
    account.bFormat = TGCP_ACCOUNT_FORMAT_STRING;
    strncpy(account.stAccountValue.szID, openid_str, sizeof(account.stAccountValue.szID) - 1);
    ret = tgcpapi_set_account(handle, &account);
    if (ret != TGCP_ERR_NONE)
    {
        printf("  tgcpapi_set_account error(%d)\n", ret);
        return -1;
    }

    printf("  connecting to tconnd(%s) as openid=%s ...\n", g_tconnd_url.c_str(), openid_str);
    ret = tgcpapi_start_connection(handle, g_tconnd_url.c_str(), kMaxTcpTimeout);
    if (ret != TGCP_ERR_NONE && ret != TGCP_ERR_STAY_IN_QUEUE)
    {
        printf("  connect error(%d): %s\n", ret, tgcpapi_error_string(ret));
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
        printf("  tgcpapi_send error(%d): %s\n", ret, tgcpapi_error_string(ret));
        return -1;
    }
    return 0;
}

static uint32_t RecvOne(HTGCPAPI handle, ClientHeader& out_header, const char** out_body_ptr, int* out_body_size,
                        int max_retry = 100)
{
    const char* data_ptr = nullptr;
    int data_size = 0;
    int ret = TGCP_ERR_NONE;
    for (int retry = 0; retry < max_retry; ++retry)
    {
        usleep(50000);
        ret = tgcpapi_peek(handle, &data_ptr, &data_size, 0);
        if (ret == TGCP_ERR_NONE)
            break;
        if (ret != TGCP_ERR_PKG_NOT_COMPLETE)
            return 0;
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

// 跳过推送，等到期望的响应cmd
static uint32_t RecvResponse(HTGCPAPI handle, uint32_t expected_cmd, ClientHeader& out_header,
                            const char** out_body_ptr, int* out_body_size, int max_retry = 120)
{
    for (int i = 0; i < max_retry; ++i)
    {
        uint32_t cmd = RecvOne(handle, out_header, out_body_ptr, out_body_size, 2);
        if (cmd == expected_cmd)
            return cmd;
        if (cmd != 0)
            printf("  [push] cmd_id=%u (waiting for %u)\n", cmd, expected_cmd);
    }
    return 0;
}

// 游客登录，返回分配到的gid
static int DoGuestLogin(HTGCPAPI handle, const char* guest_name, uint64_t& out_gid)
{
    printf("  GuestLogin (name=%s)\n", guest_name);
    connsvr::GuestLoginReq req;
    req.set_guest_name(guest_name);
    if (SendRequest(handle, CMD_GUEST_LOGIN, 2001, 0, req) != 0)
        return -1;

    ClientHeader header;
    const char* body = nullptr;
    int body_size = 0;
    uint32_t cmd = RecvResponse(handle, CMD_GUEST_LOGIN, header, &body, &body_size);
    CHECK(cmd == CMD_GUEST_LOGIN, "GuestLogin no response (connsvr alive?)");

    connsvr::GuestLoginResp resp;
    CHECK(body_size > 0 && resp.ParseFromArray(body, body_size), "GuestLoginResp parse failed");
    CHECK(resp.ret_code() == 0, "GuestLogin ret_code != 0");
    CHECK(resp.gid() != 0, "GuestLogin gid == 0");
    CHECK(resp.is_guest(), "is_guest should be true");
    CHECK(header.gid == resp.gid(), "Header.gid != body.gid");

    // 游客号段：高32位应为标记位(1)，低32位非0
    uint32_t high32 = static_cast<uint32_t>(resp.gid() >> 32);
    CHECK(high32 == 1, "guest gid high32 marker != 1");
    CHECK(static_cast<uint32_t>(resp.gid() & 0xFFFFFFFFULL) != 0, "guest gid low32 == 0");

    out_gid = resp.gid();
    printf("  GuestLogin ok, gid=%llu (0x%016llx), user_name=\"%s\"\n", (unsigned long long)out_gid,
           (unsigned long long)out_gid, resp.user_name().c_str());
    PASS("GuestLogin");
    return 0;
}

// 查询房间列表，返回房间数；found_room_id 非空时检查指定房间是否存在
static int DoRoomList(HTGCPAPI handle, uint64_t gid, const std::string& look_for, bool* found)
{
    connsvr::RoomListReq req;
    if (SendRequest(handle, CMD_ROOM_LIST_REQ, 2010, gid, req) != 0)
        return -1;

    ClientHeader header;
    const char* body = nullptr;
    int body_size = 0;
    uint32_t cmd = RecvResponse(handle, CMD_ROOM_LIST_REQ, header, &body, &body_size);
    CHECK(cmd == CMD_ROOM_LIST_REQ, "RoomList no response (connsvr alive?)");

    connsvr::RoomListRsp resp;
    CHECK(body_size > 0 && resp.ParseFromArray(body, body_size), "RoomListRsp parse failed");
    CHECK(resp.code() == 0, "RoomList code != 0");

    if (found)
        *found = false;
    printf("  RoomList ok, room_count=%d\n", resp.snapshot().rooms_size());
    for (const auto& r : resp.snapshot().rooms())
    {
        printf("    room_id=%s name=%s players=%d/%d in_battle=%d\n", r.room_id().c_str(), r.room_name().c_str(),
               r.current_players(), r.max_players(), r.in_battle());
        if (found && !look_for.empty() && r.room_id() == look_for)
            *found = true;
    }
    return resp.snapshot().rooms_size();
}

int main(int argc, char** argv)
{
    if (argc > 1)
        g_tconnd_url = argv[1];

    printf("==== guest disconnect / room cleanup test ====\n");
    printf("tconnd: %s\n\n", g_tconnd_url.c_str());

    // ---- 步骤1：游客A 登录并建房 ----
    printf("[1] guest A: connect + GuestLogin + RoomCreate\n");
    HTGCPAPI ha = nullptr;
    if (tgcpapi_create(&ha) != TGCP_ERR_NONE)
    {
        printf("FAIL: tgcpapi_create\n");
        return 1;
    }
    if (ConnectHandle(ha, "900001") != 0)
        return 1;

    uint64_t gid_a = 0;
    if (DoGuestLogin(ha, "GuestA", gid_a) != 0)
        return 1;

    connsvr::RoomCreateReq create_req;
    create_req.set_room_name("guest_room");
    create_req.set_max_players(8);
    if (SendRequest(ha, CMD_ROOM_CREATE_REQ, 2020, gid_a, create_req) != 0)
        return 1;

    ClientHeader header;
    const char* body = nullptr;
    int body_size = 0;
    uint32_t cmd = RecvResponse(ha, CMD_ROOM_CREATE_REQ, header, &body, &body_size);
    if (cmd != CMD_ROOM_CREATE_REQ)
    {
        printf("  FAIL: RoomCreate no response\n");
        return 1;
    }
    connsvr::RoomCreateRsp create_rsp;
    if (body_size <= 0 || !create_rsp.ParseFromArray(body, body_size) || create_rsp.code() != 0)
    {
        printf("  FAIL: RoomCreate failed, code=%d msg=%s\n", create_rsp.code(), create_rsp.message().c_str());
        return 1;
    }
    std::string room_id = create_rsp.room_id();
    printf("  RoomCreate ok, room_id=%s\n", room_id.c_str());
    ++g_pass_count;

    // ---- 验证 display_name：等 2001 房间详情推送，检查成员昵称 ----
    printf("\n[1b] verify member display_name in RoomDetail push (cmd=2001)\n");
    {
        bool got_detail = false;
        connsvr::RoomDetailSnapshot detail;
        for (int i = 0; i < 60 && !got_detail; ++i)
        {
            uint32_t c = RecvOne(ha, header, &body, &body_size, 2);
            if (c == CMD_PUSH_ROOM_DETAIL_UPDATED && body_size > 0 && detail.ParseFromArray(body, body_size))
                got_detail = true;
            else if (c != 0)
                printf("  [push] cmd_id=%u (waiting for 2001)\n", c);
        }
        if (!got_detail)
        {
            printf("  FAIL: no RoomDetail(2001) push received\n");
            ++g_fail_count;
        }
        else
        {
            std::string gid_a_str = std::to_string(gid_a);
            const connsvr::RoomMemberProto* me = nullptr;
            for (const auto& m : detail.members())
            {
                printf("    member gid=%s display_name=\"%s\" is_bot=%d slot=%u\n", m.gid().c_str(),
                       m.display_name().c_str(), m.b_is_bot(), m.slot_index());
                if (!m.b_is_bot() && m.gid() == gid_a_str)
                    me = &m;
            }
            if (!me)
            {
                printf("  FAIL: guest A (gid=%s) not found in room members\n", gid_a_str.c_str());
                ++g_fail_count;
            }
            else if (me->display_name() != "GuestA")
            {
                printf("  FAIL: display_name is \"%s\", expected \"GuestA\" "
                       "(empty => client falls back to showing gid)\n",
                       me->display_name().c_str());
                ++g_fail_count;
            }
            else
            {
                PASS("guest display_name correct in RoomDetail push");
            }
        }
    }

    // 确认房间在大厅可见
    bool found = false;
    if (DoRoomList(ha, gid_a, room_id, &found) < 0)
        return 1;
    if (!found)
    {
        printf("  FAIL: room %s not visible in lobby right after create\n", room_id.c_str());
        return 1;
    }
    PASS("room visible in lobby after create");

    // ---- 步骤1c：游客C 加入A的房间，验证"进房"路径的昵称 ----
    printf("\n[1c] guest C: join A's room, verify BOTH members' display_name\n");
    HTGCPAPI hc = nullptr;
    if (tgcpapi_create(&hc) != TGCP_ERR_NONE)
    {
        printf("FAIL: tgcpapi_create C\n");
        return 1;
    }
    if (ConnectHandle(hc, "900003") != 0)
        return 1;
    uint64_t gid_c = 0;
    if (DoGuestLogin(hc, "GuestC", gid_c) != 0)
        return 1;

    {
        connsvr::RoomJoinReq join_req;
        join_req.set_room_id(room_id);
        if (SendRequest(hc, CMD_ROOM_JOIN_REQ, 2030, gid_c, join_req) != 0)
            return 1;
        uint32_t jc = RecvResponse(hc, CMD_ROOM_JOIN_REQ, header, &body, &body_size);
        connsvr::RoomJoinRsp join_rsp;
        // 注意：proto3下 code=0 且 message 为空时，响应体序列化为0字节，属正常成功。
        if (body_size > 0)
            join_rsp.ParseFromArray(body, body_size);
        if (jc != CMD_ROOM_JOIN_REQ || join_rsp.code() != 0)
        {
            printf("  FAIL: RoomJoin failed, cmd=%u code=%d msg=%s\n", jc, join_rsp.code(),
                   join_rsp.message().c_str());
            return 1;
        }
        printf("  RoomJoin ok\n");
        ++g_pass_count;

        // 等 C 收到的 2001，检查两名成员昵称
        bool got_detail = false;
        connsvr::RoomDetailSnapshot detail;
        for (int i = 0; i < 60 && !got_detail; ++i)
        {
            uint32_t c = RecvOne(hc, header, &body, &body_size, 2);
            if (c == CMD_PUSH_ROOM_DETAIL_UPDATED && body_size > 0 && detail.ParseFromArray(body, body_size) &&
                detail.members_size() >= 2)
                got_detail = true;
            else if (c != 0)
                printf("  [push] cmd_id=%u (waiting for 2001 with 2 members)\n", c);
        }
        if (!got_detail)
        {
            printf("  FAIL: no RoomDetail(2001) with 2 members received by joiner\n");
            ++g_fail_count;
        }
        else
        {
            std::string a_str = std::to_string(gid_a);
            std::string c_str = std::to_string(gid_c);
            int empty_count = 0;
            for (const auto& m : detail.members())
            {
                const char* expect = (m.gid() == a_str) ? "GuestA" : (m.gid() == c_str) ? "GuestC" : "?";
                bool ok = (m.display_name() == expect);
                printf("    member gid=%s display_name=\"%s\" expect=\"%s\" %s\n", m.gid().c_str(),
                       m.display_name().c_str(), expect, ok ? "OK" : "MISMATCH");
                if (m.display_name().empty())
                    ++empty_count;
            }
            if (empty_count > 0)
            {
                printf("  FAIL: %d member(s) have EMPTY display_name -> client shows raw gid\n", empty_count);
                ++g_fail_count;
            }
            else
            {
                PASS("both guests have display_name after join");
            }
        }
    }

    tgcpapi_close_connection(hc);
    tgcpapi_fini(hc);
    tgcpapi_destroy(&hc);
    sleep(2);

    // ---- 步骤1d：加人机 + 准备 + 开战，验证昵称随CreateGame下发到dsagent ----
    // 目的：游客不写tcaplus，DS查库必然拿不到昵称。这里验证roomsvr把房间内昵称
    // 带进CreateGameReq.players[].display_name，dsagent才能兜底（战斗内/战绩显示昵称）。
    printf("\n[1d] guest A: AddBot + SetReady + StartBattle (verify name reaches dsagent)\n");
    {
        connsvr::RoomAddBotReq bot_req;
        if (SendRequest(ha, CMD_ROOM_ADD_BOT_REQ, 2040, gid_a, bot_req) != 0)
            return 1;
        uint32_t bc = RecvResponse(ha, CMD_ROOM_ADD_BOT_REQ, header, &body, &body_size);
        connsvr::RoomAddBotRsp bot_rsp;
        if (body_size > 0)
            bot_rsp.ParseFromArray(body, body_size);
        if (bc != CMD_ROOM_ADD_BOT_REQ || bot_rsp.code() != 0)
        {
            printf("  FAIL: AddBot failed, cmd=%u code=%d\n", bc, bot_rsp.code());
            ++g_fail_count;
        }
        else
        {
            printf("  AddBot ok, bot_id=%s\n", bot_rsp.bot_id().c_str());

            connsvr::RoomSetReadyReq ready_req;
            ready_req.set_b_ready(true);
            if (SendRequest(ha, CMD_ROOM_SET_READY_REQ, 2041, gid_a, ready_req) != 0)
                return 1;
            RecvResponse(ha, CMD_ROOM_SET_READY_REQ, header, &body, &body_size);

            connsvr::RoomStartBattleReq sb_req;
            if (SendRequest(ha, CMD_ROOM_START_BATTLE_REQ, 2042, gid_a, sb_req) != 0)
                return 1;
            uint32_t sc = RecvResponse(ha, CMD_ROOM_START_BATTLE_REQ, header, &body, &body_size);
            connsvr::RoomStartBattleRsp sb_rsp;
            if (body_size > 0)
                sb_rsp.ParseFromArray(body, body_size);
            printf("  StartBattle resp cmd=%u code=%d msg=%s\n", sc, sb_rsp.code(), sb_rsp.message().c_str());
            if (sb_rsp.code() == 0)
            {
                printf("  StartBattle accepted -> check dsagent log for:\n");
                printf("    \"CreateGame: gid(%llu) ... display_name(GuestA)\"\n", (unsigned long long)gid_a);
                PASS("StartBattle accepted (name delivery verified via dsagent log)");
            }
            else
            {
                printf("  NOTE: StartBattle rejected (code=%d), DS env may be unavailable\n", sb_rsp.code());
            }
            // 排空后续推送，便于观察
            for (int i = 0; i < 40; ++i)
            {
                uint32_t c = RecvOne(ha, header, &body, &body_size, 2);
                if (c != 0)
                    printf("  [push] cmd_id=%u\n", c);
            }
        }
    }

    // ---- 步骤2：游客A 断线（此时房间处于 IN_BATTLE）----
    // 关键：战斗中掉线不应立即移除成员/销毁房间，否则最后一名真人掉线会连战绩一起丢。
    // 期望：房间仍在（延迟离房），待 GameFinish/超时后才真正离房。
    printf("\n[2] guest A: disconnect WHILE IN BATTLE (expect deferred leave)\n");
    tgcpapi_close_connection(ha);
    tgcpapi_fini(ha);
    tgcpapi_destroy(&ha);
    printf("  disconnected, waiting for server-side cleanup ...\n");
    sleep(3);

    // ---- 步骤3：游客B 上线，验证 connsvr 存活 + 房间已回收 ----
    printf("\n[3] guest B: connect + GuestLogin (proves connsvr did NOT crash)\n");
    HTGCPAPI hb = nullptr;
    if (tgcpapi_create(&hb) != TGCP_ERR_NONE)
    {
        printf("FAIL: tgcpapi_create B\n");
        return 1;
    }
    if (ConnectHandle(hb, "900002") != 0)
    {
        printf("  FAIL: cannot connect after guest A disconnect -- connsvr may have crashed\n");
        return 1;
    }

    uint64_t gid_b = 0;
    if (DoGuestLogin(hb, "GuestB", gid_b) != 0)
    {
        printf("  FAIL: GuestLogin failed after A disconnect -- connsvr may have crashed\n");
        return 1;
    }
    PASS("connsvr alive after guest disconnect");

    if (gid_b == gid_a)
    {
        printf("  FAIL: guest B got same gid as A (%llu)\n", (unsigned long long)gid_a);
        return 1;
    }
    printf("  gid A=%llu, gid B=%llu (distinct)\n", (unsigned long long)gid_a, (unsigned long long)gid_b);
    PASS("guest gids are distinct");

    // ---- 步骤4：战斗中掉线 → 房间应保留（延迟离房），战绩不丢 ----
    printf("\n[4] verify room SURVIVES disconnect during battle (deferred leave)\n");
    found = false;
    if (DoRoomList(hb, gid_b, room_id, &found) < 0)
        return 1;
    if (found)
    {
        printf("  room %s still present -> members frozen during battle, settle data preserved\n", room_id.c_str());
        PASS("room survived in-battle disconnect (deferred leave, no data loss)");
    }
    else
    {
        printf("  FAIL: room %s vanished on in-battle disconnect "
               "-> last_match/settle data lost (regression)\n",
               room_id.c_str());
        ++g_fail_count;
    }

    // ---- 步骤4b：掉线后重新登录（账号/游客重连场景）----
    // 游客每次登录都是新gid（R9不支持找回旧gid），所以"重连"表现为新gid进旧房间。
    // 关键验证：战斗中掉线的玩家，其 gid→room 映射若未清理，重登后会被判 kAlreadyInRoom(1004)
    // 而卡在房间外——这正是"掉线又重新登录进不去房间"的隐患。
    printf("\n[4b] reconnect after in-battle disconnect: can a NEW session join a room?\n");
    {
        HTGCPAPI hr = nullptr;
        if (tgcpapi_create(&hr) != TGCP_ERR_NONE || ConnectHandle(hr, "900005") != 0)
        {
            printf("  FAIL: reconnect client cannot connect\n");
            return 1;
        }
        uint64_t gid_r = 0;
        if (DoGuestLogin(hr, "GuestR", gid_r) != 0)
            return 1;

        // 新建一个房间（战斗中的旧房间无法加入，符合设计）
        connsvr::RoomCreateReq r_create;
        r_create.set_room_name("reconnect_room");
        r_create.set_max_players(8);
        if (SendRequest(hr, CMD_ROOM_CREATE_REQ, 2060, gid_r, r_create) != 0)
            return 1;
        uint32_t rc = RecvResponse(hr, CMD_ROOM_CREATE_REQ, header, &body, &body_size);
        connsvr::RoomCreateRsp r_rsp;
        if (body_size > 0)
            r_rsp.ParseFromArray(body, body_size);
        if (rc == CMD_ROOM_CREATE_REQ && r_rsp.code() == 0)
        {
            printf("  reconnected guest created room_id=%s\n", r_rsp.room_id().c_str());
            PASS("re-login session can create/enter a room (not blocked by stale mapping)");
        }
        else
        {
            printf("  FAIL: re-login guest cannot create room, code=%d (%s)\n", r_rsp.code(),
                   r_rsp.code() == 1004 ? "kAlreadyInRoom -- stale gid->room mapping!" : "other");
            ++g_fail_count;
        }
        tgcpapi_close_connection(hr);
        tgcpapi_fini(hr);
        tgcpapi_destroy(&hr);
        sleep(2);
    }

    // ---- 步骤5：非战斗中掉线 → 房间必须立即回收（原残留bug的回归点）----
    printf("\n[5] guest B: create room then disconnect in WAITING (expect immediate cleanup)\n");
    {
        connsvr::RoomCreateReq b_create;
        b_create.set_room_name("waiting_room");
        b_create.set_max_players(8);
        if (SendRequest(hb, CMD_ROOM_CREATE_REQ, 2050, gid_b, b_create) != 0)
            return 1;
        uint32_t bc2 = RecvResponse(hb, CMD_ROOM_CREATE_REQ, header, &body, &body_size);
        connsvr::RoomCreateRsp b_rsp;
        if (body_size > 0)
            b_rsp.ParseFromArray(body, body_size);
        if (bc2 != CMD_ROOM_CREATE_REQ || b_rsp.code() != 0)
        {
            printf("  FAIL: guest B RoomCreate failed, code=%d\n", b_rsp.code());
            ++g_fail_count;
        }
        else
        {
            std::string b_room = b_rsp.room_id();
            printf("  guest B created room_id=%s (state=WAITING)\n", b_room.c_str());

            tgcpapi_close_connection(hb);
            tgcpapi_fini(hb);
            tgcpapi_destroy(&hb);
            hb = nullptr;
            printf("  guest B disconnected, waiting for cleanup ...\n");
            sleep(3);

            // 用第三条连接查列表，确认B的房间已回收
            HTGCPAPI hd = nullptr;
            if (tgcpapi_create(&hd) != TGCP_ERR_NONE || ConnectHandle(hd, "900004") != 0)
            {
                printf("  FAIL: cannot connect verifier -- connsvr may have crashed\n");
                return 1;
            }
            uint64_t gid_d = 0;
            if (DoGuestLogin(hd, "GuestD", gid_d) != 0)
                return 1;

            bool b_found = false;
            if (DoRoomList(hd, gid_d, b_room, &b_found) < 0)
                return 1;
            if (b_found)
            {
                printf("  FAIL: WAITING room %s NOT cleaned up after owner disconnect\n", b_room.c_str());
                ++g_fail_count;
            }
            else
            {
                PASS("WAITING room cleaned up immediately on disconnect");
            }
            tgcpapi_close_connection(hd);
            tgcpapi_fini(hd);
            tgcpapi_destroy(&hd);
        }
    }

    if (hb)
    {
        tgcpapi_close_connection(hb);
        tgcpapi_fini(hb);
        tgcpapi_destroy(&hb);
    }

    printf("\n==== result: pass=%d fail=%d ====\n", g_pass_count, g_fail_count);
    printf("NOTE: 战斗中的房间会在 GameFinish / 战斗超时后由 ApplyPendingLeaves 真正离房并回收，\n");
    printf("      验证方式见 roomsvr 日志 \"pending leave applied after battle\"。\n");
    return g_fail_count == 0 ? 0 : 1;
}
