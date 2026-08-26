// bench_client.cpp: app_server 并发压测客户端
//
// 与 test_gcp_client 的区别（后者是功能验证，不能用来测性能）：
//   1. test_gcp_client 的 RecvOne 里有 usleep(50000)，每次收包先睡 50ms，
//      任何 RPC 的延迟都会被固定放大到 50ms 以上，完全测不出真实延迟；
//      这里改成 tgcpapi_peek(timeout=0) 真轮询 + 极短退避。
//   2. 支持可配并发连接数/运行时长，每连接一个线程一个 GCP 连接。
//   3. 采集延迟分布（P50/P95/P99/P999）而不是只判断成功失败。
//
// 用法:
//   ./bench_client --scenario=login --conns=50 --duration=30
//   ./bench_client --scenario=roomlist --conns=100 --duration=60 --url=tcp://127.0.0.1:18801
//
// 场景:
//   login    — 每个连接反复登录，压 connsvr → rolesvr 链路
//   roomlist — 所有连接登录后建房，然后反复改房名触发 DoPushRoomList 全局广播，
//              用来验证房间数×在线数的扇出开销（roomsvr 每次都会构建全量房间快照，
//              connsvr 再对每个在线玩家单独序列化一次）
//   idle     — 登录后只保持连接接收推送，用于观察纯推送负载

#include <arpa/inet.h>
#include <getopt.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "conn.pb.h"
#include "net/client_cmd_id.h"
#include "net/client_header.h"
#include "tgcpapi/tgcpapi.h"

using namespace app;

// ========== GCP 连接参数（与 test_gcp_client 保持一致） ==========
static const int kServiceID = 1;
static const eAuthType kAuthMode = TGCP_AUTH_NONE;
static const eEncryptMethod kEncryptMethod = TGCP_ENCRYPT_METHOD_AES;
static const eKeyMaking kKeyMakingMethod = TGCP_KEY_MAKING_INSVR;
static const int kMaxGameDataLen = 10240;
static const int kMaxTcpTimeout = 10000;

// ========== 运行参数 ==========
struct Options
{
    std::string url = "tcp://127.0.0.1:18801";
    std::string scenario = "login";
    int conns = 10;
    int duration_sec = 30;
    // 每次请求之间的思考时间，模拟真实玩家节奏；0 表示尽最大速率打
    int think_ms = 0;
    // 单个请求等待响应的超时
    int timeout_ms = 5000;
    // 起始 openid，多次压测用不同区间可避免复用同一批账号
    uint32_t base_openid = 900000;
};

static Options g_opt;
static std::atomic<bool> g_stop{false};

// ========== 延迟统计 ==========
// 与服务端 libsrc/common/metrics.h 相同的对数分桶思路：内存恒定，量级足够定位问题。
class Histogram
{
public:
    static constexpr size_t kBucketNum = 40;

    void Add(uint64_t micro_sec)
    {
        ++count_;
        total_ += micro_sec;
        if (micro_sec > max_)
            max_ = micro_sec;
        ++buckets_[Index(micro_sec)];
    }

    void Merge(const Histogram& other)
    {
        count_ += other.count_;
        total_ += other.total_;
        if (other.max_ > max_)
            max_ = other.max_;
        for (size_t i = 0; i < kBucketNum; ++i)
            buckets_[i] += other.buckets_[i];
    }

    uint64_t Count() const { return count_; }
    uint64_t Avg() const { return count_ ? total_ / count_ : 0; }
    uint64_t Max() const { return max_; }

    uint64_t Percentile(double ratio) const
    {
        if (count_ == 0)
            return 0;
        // 向上取整取第 ceil(count*ratio) 个样本，否则长尾会被漏掉
        uint64_t target = static_cast<uint64_t>(std::ceil(static_cast<double>(count_) * ratio));
        if (target == 0)
            target = 1;
        if (target > count_)
            target = count_;

        uint64_t accum = 0;
        for (size_t i = 0; i < kBucketNum; ++i)
        {
            accum += buckets_[i];
            if (accum >= target)
            {
                if (i == 0)
                    return 0;
                return std::min<uint64_t>(1ULL << i, max_);
            }
        }
        return max_;
    }

private:
    static size_t Index(uint64_t v)
    {
        if (v == 0)
            return 0;
        size_t idx = 64 - static_cast<size_t>(__builtin_clzll(v));
        return idx < kBucketNum ? idx : kBucketNum - 1;
    }

    uint64_t count_ = 0;
    uint64_t total_ = 0;
    uint64_t max_ = 0;
    uint64_t buckets_[kBucketNum] = {0};
};

// 每个操作类型一份统计
struct OpStat
{
    Histogram latency;
    uint64_t success = 0;
    uint64_t failed = 0;
    uint64_t timeout = 0;
};

struct WorkerResult
{
    // 按 cmd_id 索引，用 vector 避免热路径 map 查找
    std::vector<std::pair<uint32_t, OpStat>> ops;
    uint64_t push_received = 0;
    bool connect_failed = false;

    OpStat& Op(uint32_t cmd)
    {
        for (auto& item : ops)
        {
            if (item.first == cmd)
                return item.second;
        }
        ops.emplace_back(cmd, OpStat{});
        return ops.back().second;
    }
};

static uint64_t NowMicro()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// ========== GCP 收发原语 ==========

static int InitHandle(HTGCPAPI handle, const char* openid_str)
{
    int ret =
        tgcpapi_init(handle, kServiceID, TGCP_ANDROID, kAuthMode, kEncryptMethod, kKeyMakingMethod, kMaxGameDataLen);
    if (ret != TGCP_ERR_NONE)
        return -1;

    TGCPACCOUNT account = TGCPACCOUNT();
    account.uType = TGCP_ACCOUNT_TYPE_NONE;
    account.bFormat = TGCP_ACCOUNT_FORMAT_STRING;
    strncpy(account.stAccountValue.szID, openid_str, sizeof(account.stAccountValue.szID) - 1);

    return tgcpapi_set_account(handle, &account) == TGCP_ERR_NONE ? 0 : -1;
}

static int ConnectHandle(HTGCPAPI handle, const char* openid_str)
{
    if (InitHandle(handle, openid_str) != 0)
        return -1;
    int ret = tgcpapi_start_connection(handle, g_opt.url.c_str(), kMaxTcpTimeout);
    if (ret != TGCP_ERR_NONE && ret != TGCP_ERR_STAY_IN_QUEUE)
        return -1;
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
    buf.reserve(header_len + body_bytes.size());
    buf.append(header_buf, header_len);
    buf.append(body_bytes);

    return tgcpapi_send(handle, buf.data(), static_cast<int>(buf.size()), 0) == TGCP_ERR_NONE ? 0 : -1;
}

// 非阻塞收一条消息。无数据返回 0。
// 关键：timeout 传 0 且不做固定 sleep —— test_gcp_client 里的 usleep(50ms) 会把
// 每个请求的延迟都抬到 50ms 以上，压测必须避免。
static uint32_t TryRecvOne(HTGCPAPI handle, ClientHeader& out_header, const char** out_body, int* out_size)
{
    const char* data_ptr = nullptr;
    int data_size = 0;
    int ret = tgcpapi_peek(handle, &data_ptr, &data_size, 0);
    if (ret != TGCP_ERR_NONE)
        return 0;
    if (static_cast<size_t>(data_size) < PACKED_CLIENT_HEADER_LENGTH)
        return 0;
    if (Unpack(out_header, data_ptr, static_cast<size_t>(data_size)) != 0)
        return 0;
    if (static_cast<size_t>(data_size) != PACKED_CLIENT_HEADER_LENGTH + out_header.body_length)
        return 0;

    *out_body = data_ptr + PACKED_CLIENT_HEADER_LENGTH;
    *out_size = static_cast<int>(out_header.body_length);
    return out_header.cmd_id;
}

// 等待指定 cmd 的响应，途中收到的推送计入 push_count 后跳过。
// 返回 0 表示超时。
static uint32_t WaitResponse(HTGCPAPI handle, uint32_t expect_cmd, ClientHeader& header, const char** body,
                             int* size, uint64_t* push_count, bool accept_alt, uint32_t alt_cmd)
{
    uint64_t deadline = NowMicro() + static_cast<uint64_t>(g_opt.timeout_ms) * 1000;
    int idle_spins = 0;
    while (NowMicro() < deadline && !g_stop.load(std::memory_order_relaxed))
    {
        uint32_t cmd = TryRecvOne(handle, header, body, size);
        if (cmd == 0)
        {
            // 短暂退避，避免空转吃满 CPU 影响被测服务；
            // 前若干次纯自旋以保证低延迟场景的测量精度。
            if (++idle_spins > 100)
                usleep(200);
            continue;
        }
        idle_spins = 0;
        if (cmd == expect_cmd || (accept_alt && cmd == alt_cmd))
            return cmd;
        // 是推送
        if (push_count)
            ++(*push_count);
    }
    return 0;
}

// 发一个请求并等响应，记录延迟。返回收到的 cmd（0 表示失败/超时）。
static uint32_t DoRequest(HTGCPAPI handle, WorkerResult& result, uint32_t cmd_id, uint32_t seq_id, uint64_t gid,
                          const google::protobuf::Message& req, ClientHeader& header, const char** body, int* size,
                          bool accept_alt = false, uint32_t alt_cmd = 0)
{
    OpStat& stat = result.Op(cmd_id);

    uint64_t begin = NowMicro();
    if (SendRequest(handle, cmd_id, seq_id, gid, req) != 0)
    {
        ++stat.failed;
        return 0;
    }

    uint32_t got = WaitResponse(handle, cmd_id, header, body, size, &result.push_received, accept_alt, alt_cmd);
    uint64_t cost = NowMicro() - begin;

    if (got == 0)
    {
        ++stat.timeout;
        return 0;
    }
    stat.latency.Add(cost);
    ++stat.success;
    return got;
}

// ========== 压测场景 ==========

// 登录并设置昵称，成功返回 gid，失败返回 0
static uint64_t DoLoginFlow(HTGCPAPI handle, WorkerResult& result, const std::string& name, uint32_t seq_base)
{
    ClientHeader header;
    const char* body = nullptr;
    int size = 0;

    connsvr::LoginReq req;
    // 登录响应可能是 CMD_LOGIN(老用户) 或 CMD_LOGIN_NEW(新用户)，两者都算成功
    uint32_t cmd = DoRequest(handle, result, CMD_LOGIN, seq_base, 0, req, header, &body, &size, true, CMD_LOGIN_NEW);
    if (cmd == 0)
        return 0;

    uint64_t gid = header.gid;
    if (gid == 0)
        return 0;

    connsvr::SetUserInfoReq su;
    su.set_user_name(name);
    su.set_role_type(1);
    DoRequest(handle, result, CMD_SET_USER_INFO, seq_base + 1, gid, su, header, &body, &size);
    return gid;
}

// login 场景：反复「连接→登录→断开」，压 connsvr → rolesvr 链路
static void RunLoginScenario(int worker_id, WorkerResult& result)
{
    uint32_t seq = 1;
    char openid_buf[64];
    std::string name = "bench" + std::to_string(worker_id);

    while (!g_stop.load(std::memory_order_relaxed))
    {
        // 每轮换一个 openid，避免服务端把同一账号的重复登录当作重连特殊处理
        snprintf(openid_buf, sizeof(openid_buf), "%u", g_opt.base_openid + worker_id * 10000 + seq);

        HTGCPAPI handle = nullptr;
        if (tgcpapi_create(&handle) != TGCP_ERR_NONE || !handle)
            break;

        if (ConnectHandle(handle, openid_buf) != 0)
        {
            result.connect_failed = true;
            tgcpapi_destroy(&handle);
            break;
        }

        DoLoginFlow(handle, result, name, seq);

        tgcpapi_close_connection(handle);
        tgcpapi_destroy(&handle);

        seq += 2;
        if (g_opt.think_ms > 0)
            usleep(static_cast<useconds_t>(g_opt.think_ms) * 1000);
    }
}

// roomlist 场景：登录后建房，随后反复改房名。
// 每次改名都会触发 roomsvr 的 DoPushRoomList()：构建全量房间快照 → 发给 connsvr →
// connsvr 对每个在线玩家单独序列化并推送。并发连接数越多、房间越多，扇出越大。
static void RunRoomListScenario(int worker_id, WorkerResult& result)
{
    char openid_buf[64];
    snprintf(openid_buf, sizeof(openid_buf), "%u", g_opt.base_openid + worker_id);
    std::string name = "bench" + std::to_string(worker_id);

    HTGCPAPI handle = nullptr;
    if (tgcpapi_create(&handle) != TGCP_ERR_NONE || !handle)
        return;
    if (ConnectHandle(handle, openid_buf) != 0)
    {
        result.connect_failed = true;
        tgcpapi_destroy(&handle);
        return;
    }

    uint32_t seq = 1;
    uint64_t gid = DoLoginFlow(handle, result, name, seq);
    seq += 2;
    if (gid == 0)
    {
        tgcpapi_destroy(&handle);
        return;
    }

    ClientHeader header;
    const char* body = nullptr;
    int size = 0;

    // 建房。房间名受 roomsvr 限制：trim 后最多 11 个 Unicode 码点
    // （room_service.cpp 的 kMaxRoomNameLen），"bench_room_N" 会超长被拒，故用短名。
    connsvr::RoomCreateReq create_req;
    create_req.set_room_name("br" + std::to_string(worker_id));
    create_req.set_max_players(8);
    uint32_t cmd = DoRequest(handle, result, CMD_ROOM_CREATE_REQ, seq++, gid, create_req, header, &body, &size);
    bool has_room = false;
    if (cmd != 0 && size > 0)
    {
        connsvr::RoomCreateRsp resp;
        if (resp.ParseFromArray(body, size))
        {
            has_room = (resp.code() == 0);
            // 业务错误码不能静默吞掉，否则场景退化成 RoomList 却看不出原因
            if (!has_room && worker_id == 0)
                printf("  [warn] RoomCreate rejected, code=%d msg=%s (fallback to RoomList)\n", resp.code(),
                       resp.message().c_str());
        }
    }

    // 反复改房名 → 每次都触发全局 RoomList 广播
    while (!g_stop.load(std::memory_order_relaxed))
    {
        if (has_room)
        {
            // 同样受 11 码点限制，用取模保证长度始终够短
            connsvr::RoomRenameReq rename_req;
            rename_req.set_new_name("r" + std::to_string(worker_id % 1000) + "_" +
                                    std::to_string(seq % 100000));
            DoRequest(handle, result, CMD_ROOM_RENAME_REQ, seq++, gid, rename_req, header, &body, &size);
        }
        else
        {
            // 没建成房就退化为查询房间列表，同样能压 connsvr
            connsvr::RoomListReq list_req;
            DoRequest(handle, result, CMD_ROOM_LIST_REQ, seq++, gid, list_req, header, &body, &size);
        }

        if (g_opt.think_ms > 0)
            usleep(static_cast<useconds_t>(g_opt.think_ms) * 1000);
    }

    tgcpapi_close_connection(handle);
    tgcpapi_destroy(&handle);
}

// idle 场景：登录后只收推送，用于观察纯广播负载下客户端收到的推送量
static void RunIdleScenario(int worker_id, WorkerResult& result)
{
    char openid_buf[64];
    snprintf(openid_buf, sizeof(openid_buf), "%u", g_opt.base_openid + worker_id);
    std::string name = "bench" + std::to_string(worker_id);

    HTGCPAPI handle = nullptr;
    if (tgcpapi_create(&handle) != TGCP_ERR_NONE || !handle)
        return;
    if (ConnectHandle(handle, openid_buf) != 0)
    {
        result.connect_failed = true;
        tgcpapi_destroy(&handle);
        return;
    }

    uint64_t gid = DoLoginFlow(handle, result, name, 1);
    if (gid == 0)
    {
        tgcpapi_destroy(&handle);
        return;
    }

    ClientHeader header;
    const char* body = nullptr;
    int size = 0;
    while (!g_stop.load(std::memory_order_relaxed))
    {
        if (TryRecvOne(handle, header, &body, &size) != 0)
            ++result.push_received;
        else
            usleep(500);
    }

    tgcpapi_close_connection(handle);
    tgcpapi_destroy(&handle);
}

static void Worker(int worker_id, WorkerResult* result)
{
    if (g_opt.scenario == "login")
        RunLoginScenario(worker_id, *result);
    else if (g_opt.scenario == "roomlist")
        RunRoomListScenario(worker_id, *result);
    else
        RunIdleScenario(worker_id, *result);
}

// ========== 结果汇总 ==========

static const char* CmdName(uint32_t cmd)
{
    switch (cmd)
    {
        case CMD_LOGIN: return "Login";
        case CMD_SET_USER_INFO: return "SetUserInfo";
        case CMD_ROOM_LIST_REQ: return "RoomList";
        case CMD_ROOM_CREATE_REQ: return "RoomCreate";
        case CMD_ROOM_RENAME_REQ: return "RoomRename";
        default: return "?";
    }
}

static void Report(const std::vector<WorkerResult>& results, double elapsed_sec)
{
    // 合并所有线程的同名 cmd 统计
    std::vector<std::pair<uint32_t, OpStat>> merged;
    uint64_t total_push = 0;
    int connect_failed = 0;

    for (const auto& r : results)
    {
        total_push += r.push_received;
        if (r.connect_failed)
            ++connect_failed;
        for (const auto& item : r.ops)
        {
            auto iter = std::find_if(merged.begin(), merged.end(),
                                     [&](const auto& m) { return m.first == item.first; });
            if (iter == merged.end())
            {
                merged.emplace_back(item);
            }
            else
            {
                iter->second.latency.Merge(item.second.latency);
                iter->second.success += item.second.success;
                iter->second.failed += item.second.failed;
                iter->second.timeout += item.second.timeout;
            }
        }
    }

    uint64_t total_ok = 0;
    uint64_t total_bad = 0;
    for (const auto& item : merged)
    {
        total_ok += item.second.success;
        total_bad += item.second.failed + item.second.timeout;
    }

    printf("\n========== bench result ==========\n");
    printf("scenario=%s conns=%d duration=%.1fs think=%dms\n", g_opt.scenario.c_str(), g_opt.conns, elapsed_sec,
           g_opt.think_ms);
    printf("total_ok=%llu total_fail=%llu qps=%.1f push_recv=%llu connect_failed=%d\n",
           static_cast<unsigned long long>(total_ok), static_cast<unsigned long long>(total_bad),
           elapsed_sec > 0 ? static_cast<double>(total_ok) / elapsed_sec : 0.0,
           static_cast<unsigned long long>(total_push), connect_failed);

    printf("\n%-14s %8s %10s %10s %10s %10s %10s %10s %8s %8s\n", "op", "count", "avg_us", "p50_us", "p95_us",
           "p99_us", "p999_us", "max_us", "fail", "timeout");
    std::sort(merged.begin(), merged.end(),
              [](const auto& a, const auto& b) { return a.second.success > b.second.success; });
    for (const auto& item : merged)
    {
        const auto& s = item.second;
        printf("%-14s %8llu %10llu %10llu %10llu %10llu %10llu %10llu %8llu %8llu\n", CmdName(item.first),
               static_cast<unsigned long long>(s.latency.Count()), static_cast<unsigned long long>(s.latency.Avg()),
               static_cast<unsigned long long>(s.latency.Percentile(0.50)),
               static_cast<unsigned long long>(s.latency.Percentile(0.95)),
               static_cast<unsigned long long>(s.latency.Percentile(0.99)),
               static_cast<unsigned long long>(s.latency.Percentile(0.999)),
               static_cast<unsigned long long>(s.latency.Max()), static_cast<unsigned long long>(s.failed),
               static_cast<unsigned long long>(s.timeout));
    }
    printf("==================================\n");
}

static void Usage(const char* prog)
{
    printf("usage: %s [options]\n", prog);
    printf("  --scenario=login|roomlist|idle  压测场景 (default: login)\n");
    printf("  --conns=N                       并发连接数 (default: 10)\n");
    printf("  --duration=SEC                  运行时长秒 (default: 30)\n");
    printf("  --think=MS                      每次请求间隔毫秒 (default: 0)\n");
    printf("  --timeout=MS                    单请求超时毫秒 (default: 5000)\n");
    printf("  --url=URL                       tconnd地址 (default: tcp://127.0.0.1:18801)\n");
    printf("  --base-openid=N                 起始openid (default: 900000)\n");
}

int main(int argc, char* argv[])
{
    static struct option long_opts[] = {
        {"scenario", required_argument, nullptr, 's'},   {"conns", required_argument, nullptr, 'c'},
        {"duration", required_argument, nullptr, 'd'},   {"think", required_argument, nullptr, 't'},
        {"timeout", required_argument, nullptr, 'o'},    {"url", required_argument, nullptr, 'u'},
        {"base-openid", required_argument, nullptr, 'b'},{"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}};

    int c = 0;
    while ((c = getopt_long(argc, argv, "s:c:d:t:o:u:b:h", long_opts, nullptr)) != -1)
    {
        switch (c)
        {
            case 's': g_opt.scenario = optarg; break;
            case 'c': g_opt.conns = atoi(optarg); break;
            case 'd': g_opt.duration_sec = atoi(optarg); break;
            case 't': g_opt.think_ms = atoi(optarg); break;
            case 'o': g_opt.timeout_ms = atoi(optarg); break;
            case 'u': g_opt.url = optarg; break;
            case 'b': g_opt.base_openid = static_cast<uint32_t>(strtoul(optarg, nullptr, 10)); break;
            case 'h': Usage(argv[0]); return 0;
            default: Usage(argv[0]); return 1;
        }
    }

    if (g_opt.conns <= 0 || g_opt.duration_sec <= 0)
    {
        Usage(argv[0]);
        return 1;
    }

    printf("bench start: scenario=%s conns=%d duration=%ds url=%s\n", g_opt.scenario.c_str(), g_opt.conns,
           g_opt.duration_sec, g_opt.url.c_str());

    std::vector<WorkerResult> results(static_cast<size_t>(g_opt.conns));
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(g_opt.conns));

    uint64_t begin = NowMicro();
    for (int i = 0; i < g_opt.conns; ++i)
        threads.emplace_back(Worker, i, &results[static_cast<size_t>(i)]);

    // 主线程按秒打点，便于观察是否有随时间劣化的趋势
    for (int elapsed = 0; elapsed < g_opt.duration_sec; ++elapsed)
    {
        sleep(1);
        if ((elapsed + 1) % 10 == 0)
            printf("  ... %ds elapsed\n", elapsed + 1);
    }
    g_stop.store(true);

    for (auto& t : threads)
        t.join();

    double elapsed_sec = static_cast<double>(NowMicro() - begin) / 1000000.0;
    Report(results, elapsed_sec);
    return 0;
}
