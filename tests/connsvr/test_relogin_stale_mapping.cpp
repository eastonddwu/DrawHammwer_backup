// test_relogin_stale_mapping.cpp: 验证「战斗中掉线 → 用同一账号重新登录」是否还能进房。
//
// 背景：账号玩家 gid == tconnd openid，重连后 gid 不变（游客每次是新gid，掩盖了本问题）。
// 战斗中掉线走的是延迟离房（只打pending标记，不移除成员、不清gid→room映射），
// 因此同一gid重登后 JoinRoom/CreateRoom 会命中 kAlreadyInRoom(1004)，卡在房间外。
//
// 用法: ./test_relogin_stale_mapping [tconnd_url]
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

static int g_pass = 0;
static int g_fail = 0;

static int ConnectHandle(HTGCPAPI handle, const char* openid_str)
{
    if (tgcpapi_init(handle, kServiceID, TGCP_ANDROID, kAuthMode, kEncryptMethod, kKeyMakingMethod, kMaxGameDataLen) !=
        TGCP_ERR_NONE)
        return -1;

    TGCPACCOUNT account = TGCPACCOUNT();
    account.uType = TGCP_ACCOUNT_TYPE_NONE;
    account.bFormat = TGCP_ACCOUNT_FORMAT_STRING;
    strncpy(account.stAccountValue.szID, openid_str, sizeof(account.stAccountValue.szID) - 1);
    if (tgcpapi_set_account(handle, &account) != TGCP_ERR_NONE)
        return -1;

    int ret = tgcpapi_start_connection(handle, g_tconnd_url.c_str(), kMaxTcpTimeout);
    if (ret != TGCP_ERR_NONE && ret != TGCP_ERR_STAY_IN_QUEUE)
    {
        printf("  connect error(%d): %s\n", ret, tgcpapi_error_string(ret));
        return -1;
    }
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

    char hbuf[PACKED_CLIENT_HEADER_LENGTH];
    size_t hlen = sizeof(hbuf);
    if (Pack(header, hbuf, hlen) != 0)
        return -1;

    std::string buf;
    buf.append(hbuf, hlen);
    buf.append(body_bytes);
    return tgcpapi_send(handle, buf.data(), static_cast<int>(buf.size()), 0) == TGCP_ERR_NONE ? 0 : -1;
}

static uint32_t RecvOne(HTGCPAPI handle, ClientHeader& out, const char** bp, int* bs, int max_retry = 100)
{
    const char* dp = nullptr;
    int ds = 0;
    int ret = TGCP_ERR_NONE;
    for (int i = 0; i < max_retry; ++i)
    {
        usleep(50000);
        ret = tgcpapi_peek(handle, &dp, &ds, 0);
        if (ret == TGCP_ERR_NONE)
            break;
        if (ret != TGCP_ERR_PKG_NOT_COMPLETE)
            return 0;
    }
    if (ret != TGCP_ERR_NONE || static_cast<size_t>(ds) < PACKED_CLIENT_HEADER_LENGTH)
        return 0;
    if (Unpack(out, dp, static_cast<size_t>(ds)) != 0)
        return 0;
    if (static_cast<size_t>(ds) != PACKED_CLIENT_HEADER_LENGTH + out.body_length)
        return 0;
    *bp = dp + PACKED_CLIENT_HEADER_LENGTH;
    *bs = static_cast<int>(out.body_length);
    return out.cmd_id;
}

static uint32_t RecvResp(HTGCPAPI handle, uint32_t expect, ClientHeader& out, const char** bp, int* bs,
                         int max_retry = 120)
{
    for (int i = 0; i < max_retry; ++i)
    {
        uint32_t c = RecvOne(handle, out, bp, bs, 2);
        if (c == expect)
            return c;
    }
    return 0;
}

// 账号登录：gid == openid，重连后不变
static int AccountLogin(HTGCPAPI h, uint64_t& gid, const char* tag)
{
    connsvr::LoginReq req;
    if (SendRequest(h, CMD_LOGIN, 1001, 0, req) != 0)
        return -1;

    ClientHeader hd;
    const char* bp = nullptr;
    int bs = 0;
    uint32_t cmd = 0;
    for (int i = 0; i < 200; ++i)
    {
        cmd = RecvOne(h, hd, &bp, &bs, 2);
        if (cmd == CMD_LOGIN || cmd == CMD_LOGIN_NEW)
            break;
        cmd = 0;
    }
    if (cmd == 0)
    {
        printf("  [%s] FAIL: no Login response\n", tag);
        return -1;
    }
    gid = hd.gid;
    printf("  [%s] Login ok, gid=%llu\n", tag, (unsigned long long)gid);
    return gid == 0 ? -1 : 0;
}

int main(int argc, char** argv)
{
    if (argc > 1)
        g_tconnd_url = argv[1];

    const char* kOpenId = "778899";  // 固定账号，模拟真实玩家重连
    printf("==== re-login after in-battle disconnect ====\n");
    printf("tconnd: %s, openid=%s (account gid is STABLE across reconnects)\n\n", g_tconnd_url.c_str(), kOpenId);

    ClientHeader hd;
    const char* bp = nullptr;
    int bs = 0;

    // ---- 1. 账号登录 + 建房 + 加人机 + 准备 + 开战 ----
    printf("[1] login, create room, add bot, start battle\n");
    HTGCPAPI h1 = nullptr;
    if (tgcpapi_create(&h1) != TGCP_ERR_NONE || ConnectHandle(h1, kOpenId) != 0)
    {
        printf("FAIL: connect\n");
        return 1;
    }
    uint64_t gid = 0;
    if (AccountLogin(h1, gid, "session1") != 0)
        return 1;

    connsvr::RoomCreateReq creq;
    creq.set_room_name("relogin_room");
    creq.set_max_players(8);
    if (SendRequest(h1, CMD_ROOM_CREATE_REQ, 1020, gid, creq) != 0)
        return 1;
    uint32_t c = RecvResp(h1, CMD_ROOM_CREATE_REQ, hd, &bp, &bs);
    connsvr::RoomCreateRsp crsp;
    if (bs > 0)
        crsp.ParseFromArray(bp, bs);
    if (c != CMD_ROOM_CREATE_REQ || crsp.code() != 0)
    {
        printf("  FAIL: RoomCreate code=%d\n", crsp.code());
        return 1;
    }
    std::string room_id = crsp.room_id();
    printf("  room created, room_id=%s\n", room_id.c_str());

    connsvr::RoomAddBotReq breq;
    if (SendRequest(h1, CMD_ROOM_ADD_BOT_REQ, 1045, gid, breq) != 0)
        return 1;
    RecvResp(h1, CMD_ROOM_ADD_BOT_REQ, hd, &bp, &bs);

    connsvr::RoomSetReadyReq rreq;
    rreq.set_b_ready(true);
    if (SendRequest(h1, CMD_ROOM_SET_READY_REQ, 1040, gid, rreq) != 0)
        return 1;
    RecvResp(h1, CMD_ROOM_SET_READY_REQ, hd, &bp, &bs);

    connsvr::RoomStartBattleReq sreq;
    if (SendRequest(h1, CMD_ROOM_START_BATTLE_REQ, 1050, gid, sreq) != 0)
        return 1;
    uint32_t sc = RecvResp(h1, CMD_ROOM_START_BATTLE_REQ, hd, &bp, &bs);
    connsvr::RoomStartBattleRsp srsp;
    if (bs > 0)
        srsp.ParseFromArray(bp, bs);
    printf("  StartBattle cmd=%u code=%d\n", sc, srsp.code());
    if (srsp.code() != 0)
    {
        printf("  FAIL: cannot enter battle, aborting (code=%d)\n", srsp.code());
        return 1;
    }
    // 等房间真正进入 IN_BATTLE
    for (int i = 0; i < 40; ++i)
        RecvOne(h1, hd, &bp, &bs, 2);
    printf("  room is now IN_BATTLE\n");
    ++g_pass;

    // ---- 2. 战斗中掉线 ----
    printf("\n[2] disconnect WHILE IN BATTLE (deferred leave expected)\n");
    tgcpapi_close_connection(h1);
    tgcpapi_fini(h1);
    tgcpapi_destroy(&h1);
    sleep(3);
    printf("  disconnected\n");

    // ---- 3. 同一账号重新登录，尝试进房 ----
    printf("\n[3] SAME account re-login, then try to enter a room\n");
    HTGCPAPI h2 = nullptr;
    if (tgcpapi_create(&h2) != TGCP_ERR_NONE || ConnectHandle(h2, kOpenId) != 0)
    {
        printf("  FAIL: cannot reconnect\n");
        return 1;
    }
    uint64_t gid2 = 0;
    if (AccountLogin(h2, gid2, "session2") != 0)
        return 1;
    if (gid2 != gid)
    {
        printf("  NOTE: gid changed (%llu -> %llu), not a stable-account case\n", (unsigned long long)gid,
               (unsigned long long)gid2);
    }

    // 3a. 能否查到自己还在房间里
    connsvr::RoomListReq lreq;
    if (SendRequest(h2, CMD_ROOM_LIST_REQ, 1060, gid2, lreq) != 0)
        return 1;
    uint32_t lc = RecvResp(h2, CMD_ROOM_LIST_REQ, hd, &bp, &bs);
    connsvr::RoomListRsp lrsp;
    if (bs > 0)
        lrsp.ParseFromArray(bp, bs);
    printf("  RoomList code=%d room_count=%d\n", lrsp.code(), lrsp.snapshot().rooms_size());
    for (const auto& r : lrsp.snapshot().rooms())
        printf("    room_id=%s players=%d/%d in_battle=%d\n", r.room_id().c_str(), r.current_players(),
               r.max_players(), r.in_battle());

    // 3b. 关键：重登后尝试建新房 —— 若gid→room映射残留会返回 kAlreadyInRoom(1004)
    printf("\n[4] KEY CHECK: can the re-logged-in player enter a room?\n");
    connsvr::RoomCreateReq creq2;
    creq2.set_room_name("after_relogin");
    creq2.set_max_players(8);
    if (SendRequest(h2, CMD_ROOM_CREATE_REQ, 1070, gid2, creq2) != 0)
        return 1;
    uint32_t c2 = RecvResp(h2, CMD_ROOM_CREATE_REQ, hd, &bp, &bs);
    connsvr::RoomCreateRsp crsp2;
    if (bs > 0)
        crsp2.ParseFromArray(bp, bs);
    printf("  RoomCreate after re-login: cmd=%u code=%d room_id=%s\n", c2, crsp2.code(), crsp2.room_id().c_str());

    if (crsp2.code() == 0)
    {
        printf("  [PASS] re-logged-in player CAN enter a room\n");
        ++g_pass;
    }
    else if (crsp2.code() == 1004)
    {
        printf("  [FAIL] kAlreadyInRoom(1004) -- STALE gid->room mapping!\n");
        printf("         玩家被永久挡在房间外，直到战斗超时(30分钟)才恢复。\n");
        ++g_fail;
    }
    else
    {
        printf("  [FAIL] unexpected code=%d\n", crsp2.code());
        ++g_fail;
    }

    // 3c. 再试加入战斗中的旧房间（预期被拒 kRoomInBattle=1003，属正常设计）
    connsvr::RoomJoinReq jreq;
    jreq.set_room_id(room_id);
    if (SendRequest(h2, CMD_ROOM_JOIN_REQ, 1080, gid2, jreq) == 0)
    {
        uint32_t jc = RecvResp(h2, CMD_ROOM_JOIN_REQ, hd, &bp, &bs);
        connsvr::RoomJoinRsp jrsp;
        if (bs > 0)
            jrsp.ParseFromArray(bp, bs);
        printf("  RoomJoin(old battling room): cmd=%u code=%d (1003=in battle, expected)\n", jc, jrsp.code());
    }

    tgcpapi_close_connection(h2);
    tgcpapi_fini(h2);
    tgcpapi_destroy(&h2);

    printf("\n==== result: pass=%d fail=%d ====\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
