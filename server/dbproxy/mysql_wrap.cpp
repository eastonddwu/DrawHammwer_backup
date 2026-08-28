/*
 * * file name: mysql_wrap.cpp
 * * description: MySQL业务层封装实现，见mysql_wrap.h说明
 * */

#include "mysql_wrap.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include "common/clock.h"
#include "common/id_generator.h"
#include "core/log.h"
#include "core/rpc_error.h"
#include "utils/db_error.h"

namespace dbproxy
{

// 一帧最多收割的socket事件数
static constexpr int kMaxEventsOnce = 64;
// 建连超时（秒）。重连发生在协程外的阻塞路径上，不能太久
static constexpr unsigned int kConnectTimeoutSec = 2;
// 重连节流间隔（毫秒）：MySQL挂掉时避免每个请求都去阻塞connect一次
static constexpr uint64_t kReconnectIntervalMs = 1000;

struct StmtDef
{
    StmtSlot slot;
    const char* sql;
};

// 语句里带 data_version=data_version+1 是关键：它保证匹配到行时affected_rows必然>0，
// 不会出现MySQL「新值与旧值相同所以0行受影响」的歧义，CAS才能靠affected_rows判定。
static const StmtDef kStmtDefs[] = {
    {kStmtGetLogin, "SELECT login_flag, data_version FROM login WHERE gid=?"},
    {kStmtUpsertLogin,
     "INSERT INTO login (gid, login_flag, data_version) VALUES (?,?,1) "
     "ON DUPLICATE KEY UPDATE login_flag=?, data_version=data_version+1"},
    {kStmtCasLogin,
     "UPDATE login SET login_flag=?, data_version=data_version+1 WHERE gid=? AND data_version=?"},
    {kStmtGetLoginVer, "SELECT data_version FROM login WHERE gid=?"},

    {kStmtGetUserInfo, "SELECT is_new, role_type, user_name, points, data_version FROM user_info WHERE gid=?"},
    {kStmtUpsertUserInfo,
     "INSERT INTO user_info (gid, is_new, role_type, user_name, points, data_version) VALUES (?,?,?,?,?,1) "
     "ON DUPLICATE KEY UPDATE is_new=?, role_type=?, user_name=?, points=?, data_version=data_version+1"},
    {kStmtCasUserInfo,
     "UPDATE user_info SET is_new=?, role_type=?, user_name=?, points=?, data_version=data_version+1 "
     "WHERE gid=? AND data_version=?"},
    {kStmtGetUserInfoVer, "SELECT data_version FROM user_info WHERE gid=?"},
};
static_assert(sizeof(kStmtDefs) / sizeof(kStmtDefs[0]) == kStmtNum, "kStmtDefs must cover all slots");

// ============================================================================
// MYSQL_BIND 绑定小工具（绑定是纯本地操作，不产生网络IO）
// ============================================================================

static void BindU64(MYSQL_BIND& b, uint64_t& v)
{
    b.buffer_type = MYSQL_TYPE_LONGLONG;
    b.buffer = &v;
    b.is_unsigned = 1;
}

static void BindU32(MYSQL_BIND& b, uint32_t& v)
{
    b.buffer_type = MYSQL_TYPE_LONG;
    b.buffer = &v;
    b.is_unsigned = 1;
}

static void BindI32(MYSQL_BIND& b, int32_t& v)
{
    b.buffer_type = MYSQL_TYPE_LONG;
    b.buffer = &v;
}

static void BindStr(MYSQL_BIND& b, char* buf, unsigned long& len, unsigned long capacity)
{
    b.buffer_type = MYSQL_TYPE_STRING;
    b.buffer = buf;
    b.buffer_length = capacity;
    b.length = &len;
}

// mysql错误码是否属于连接级错误（客户端侧2000~2999，如CR_SERVER_LOST/CR_SERVER_GONE）。
// 语句级错误（死锁、约束冲突等）不影响连接协议状态，连接可以继续复用。
static bool IsConnLevelError(unsigned int err)
{
    return err >= 2000 && err < 3000;
}

// ============================================================================
// 生命周期
// ============================================================================

int MysqlWrap::Init(const MysqlConf& conf, app::ContextController* context_ctrl)
{
    if (!context_ctrl)
    {
        APP_LOG_ERROR(0, "mysql init fail: context_ctrl is null");
        return DB_ERR_MYSQL;
    }
    conf_ = conf;
    context_ctrl_ = context_ctrl;

    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0)
    {
        APP_LOG_ERROR(0, "epoll_create1 fail, errno=%d", errno);
        return DB_ERR_MYSQL;
    }

    size_t ok_num = 0;
    conns_.reserve(conf_.conn_num);
    for (uint32_t i = 0; i < conf_.conn_num; i++)
    {
        std::unique_ptr<MysqlConn> c(new MysqlConn);
        c->index = static_cast<int>(i);
        if (ConnectOne(*c))
            ok_num++;
        else
            APP_LOG_WARN(0, "mysql conn[%u] connect fail at init, will retry on demand", i);
        conns_.push_back(std::move(c));
        free_conns_.push_back(conns_.back().get());
    }

    if (ok_num == 0)
    {
        APP_LOG_ERROR(0, "mysql init fail: no usable connection, host=%s:%u, db=%s",
                      conf_.host.c_str(), conf_.port, conf_.database.c_str());
        return DB_ERR_MYSQL;
    }

    std::string tables;
    for (size_t i = 0; i < conf_.table_names.size(); i++)
    {
        if (i > 0) tables += ",";
        tables += conf_.table_names[i];
    }
    APP_LOG_INFO(0, "mysql init succ, host=%s:%u, db=%s, tables=[%s], conn=%zu/%u, op_timeout=%ums",
                 conf_.host.c_str(), conf_.port, conf_.database.c_str(), tables.c_str(),
                 ok_num, conf_.conn_num, conf_.op_timeout_ms);
    return 0;
}

void MysqlWrap::Finish()
{
    for (auto&& c : conns_)
        CloseOne(*c);
    conns_.clear();
    free_conns_.clear();
    conn_waiters_.clear();

    if (epoll_fd_ >= 0)
    {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

bool MysqlWrap::ConnectOne(MysqlConn& c)
{
    CloseOne(c);

    c.conn = mysql_init(nullptr);
    if (!c.conn)
    {
        APP_LOG_ERROR(0, "mysql_init fail, conn[%d]", c.index);
        return false;
    }

    unsigned int connect_timeout = kConnectTimeoutSec;
    mysql_options(c.conn, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout);
    // 不开MYSQL_OPT_RECONNECT：隐式重连会在非阻塞调用内部偷偷做一次阻塞建连，
    // 那样整个reactor会被卡住。断线由broken标记 + AcquireConn里的显式重连处理。

    if (!mysql_real_connect(c.conn, conf_.host.c_str(), conf_.user.c_str(), conf_.password.c_str(),
                            conf_.database.c_str(), conf_.port, nullptr, 0))
    {
        APP_LOG_ERROR(0, "mysql_real_connect fail, conn[%d], host=%s:%u, db=%s, err=%s", c.index,
                      conf_.host.c_str(), conf_.port, conf_.database.c_str(), mysql_error(c.conn));
        mysql_close(c.conn);
        c.conn = nullptr;
        return false;
    }

    // 写入已经压成单条原子语句，不再需要显式事务，开autocommit省掉一次COMMIT往返
    if (mysql_autocommit(c.conn, 1) != 0)
    {
        APP_LOG_ERROR(0, "mysql_autocommit(on) fail, conn[%d], err=%s", c.index, mysql_error(c.conn));
        mysql_close(c.conn);
        c.conn = nullptr;
        return false;
    }

    // 全部语句在这里一次性prepare好，热路径只做execute/fetch。
    // 此时还没开非阻塞，prepare走的是阻塞路径——只发生在启动和重连，可以接受。
    for (auto&& def : kStmtDefs)
    {
        MYSQL_STMT* stmt = mysql_stmt_init(c.conn);
        if (!stmt)
        {
            APP_LOG_ERROR(0, "mysql_stmt_init fail, conn[%d], slot=%d", c.index, def.slot);
            CloseOne(c);
            return false;
        }
        if (mysql_stmt_prepare(stmt, def.sql, strlen(def.sql)) != 0)
        {
            APP_LOG_ERROR(0, "mysql_stmt_prepare fail, conn[%d], slot=%d, err=%s, sql=%s", c.index, def.slot,
                          mysql_stmt_error(stmt), def.sql);
            mysql_stmt_close(stmt);
            CloseOne(c);
            return false;
        }
        c.stmt[def.slot] = stmt;
    }

    // 最后才开非阻塞：之后所有查询都必须走 mysql_xxx_start/cont
    if (mysql_optionsv(c.conn, MYSQL_OPT_NONBLOCK, 0) != 0)
    {
        APP_LOG_ERROR(0, "enable MYSQL_OPT_NONBLOCK fail, conn[%d], err=%s", c.index, mysql_error(c.conn));
        CloseOne(c);
        return false;
    }

    c.broken = false;
    return true;
}

void MysqlWrap::CloseOne(MysqlConn& c)
{
    for (int i = 0; i < kStmtNum; i++)
    {
        if (c.stmt[i])
        {
            mysql_stmt_close(c.stmt[i]);
            c.stmt[i] = nullptr;
        }
    }
    if (c.conn)
    {
        mysql_close(c.conn);
        c.conn = nullptr;
    }
    c.broken = true;
}

// ============================================================================
// 每帧驱动
// ============================================================================

size_t MysqlWrap::Proc()
{
    if (epoll_fd_ < 0)
        return 0;

    struct epoll_event events[kMaxEventsOnce];
    int event_num = epoll_wait(epoll_fd_, events, kMaxEventsOnce, 0);

    for (int i = 0; i < event_num; i++)
    {
        uint32_t idx = events[i].data.u32;
        if (idx >= conns_.size())
            continue;
        MysqlConn* c = conns_[idx].get();
        if (c->wait_seq == 0)
            continue;

        uint32_t e = events[i].events;
        int status = 0;
        if (e & (EPOLLIN | EPOLLHUP | EPOLLERR))
            status |= MYSQL_WAIT_READ;
        if (e & EPOLLOUT)
            status |= MYSQL_WAIT_WRITE;
        if (e & EPOLLPRI)
            status |= MYSQL_WAIT_EXCEPT;
        if (status == 0)
            status = MYSQL_WAIT_READ;  // 不该发生，兜底避免把0喂给_cont

        auto* ctx = static_cast<MysqlWaitContext*>(context_ctrl_->Awake(c->wait_seq, app::RPC_SUCCESS));
        if (!ctx)
            continue;
        ctx->ready_status = status;
        // 恢复协程，它会一直跑到下一次挂起或整个handler结束
        ctx->Run();
    }

    // 派发空闲连接给排队的协程。必须放在这里（主协程上下文）而不是ReleaseConn里：
    // ReleaseConn是在协程内被调用的，若在那里直接Resume另一个协程，
    // CoroImpl::Resume()的 current==nullptr 断言会挂（见app_coroutine.cpp）。
    while (!conn_waiters_.empty() && !free_conns_.empty())
    {
        uint64_t seq = conn_waiters_.front();
        conn_waiters_.pop_front();
        auto* ctx = static_cast<MysqlWaitContext*>(context_ctrl_->Awake(seq, app::RPC_SUCCESS));
        if (!ctx)
            continue;  // 已经排队超时被清掉了
        ctx->conn = free_conns_.back();
        free_conns_.pop_back();
        ctx->Run();
    }

    // 有在飞操作或有人在排队时返回非0，让tapp主循环跳过iIdleSleep保持热转
    return static_cast<size_t>(event_num > 0 ? event_num : 0) + in_flight_ + conn_waiters_.size();
}

// ============================================================================
// 连接池
// ============================================================================

MysqlConn* MysqlWrap::AcquireConn(uint64_t gid)
{
    MysqlConn* c = nullptr;

    // 有人在排队时必须去队尾，不能直接拿free_conns_里的连接。
    // 否则会饿死：协程释放连接后紧接着又自己抢回去（Release到下次Acquire之间
    // 没有让出协程，Proc()根本没机会把连接派发给队列里的人），
    // 先到的协程会一直霸占连接跑完全部请求，队尾的协程一直等到排队超时。
    if (!free_conns_.empty() && conn_waiters_.empty())
    {
        c = free_conns_.back();
        free_conns_.pop_back();
    }
    else
    {
        uint64_t seq = app::IDGenerator::GetInst().GenerateSeqID();
        MysqlWaitContext wait_ctx;
        conn_waiters_.push_back(seq);

        int32_t ret = context_ctrl_->Pending(seq, conf_.op_timeout_ms, &wait_ctx, {nullptr});

        if (ret != app::RPC_SUCCESS || wait_ctx.ret_code == app::RPC_TIME_OUT || !wait_ctx.conn)
        {
            // 排队超时/失败，把自己从等待队列里摘掉（Awake只清context_cache_，不动这个队列）
            auto iter = std::find(conn_waiters_.begin(), conn_waiters_.end(), seq);
            if (iter != conn_waiters_.end())
                conn_waiters_.erase(iter);
            APP_LOG_WARN(gid, "acquire mysql conn fail, ret=%d, ctx_ret=%d, waiters=%zu", ret,
                         wait_ctx.ret_code, conn_waiters_.size());
            return nullptr;
        }
        c = wait_ctx.conn;
    }

    if (c->broken)
    {
        uint64_t now_ms = app::Clock::GetInst().CurrentMilliSec();
        if (now_ms < next_reconnect_ms_ || !ConnectOne(*c))
        {
            // 重连失败或被节流：连接还回池里，下个请求再试
            next_reconnect_ms_ = now_ms + kReconnectIntervalMs;
            free_conns_.push_back(c);
            return nullptr;
        }
        APP_LOG_WARN(gid, "mysql conn[%d] reconnected", c->index);
    }

    return c;
}

void MysqlWrap::ReleaseConn(MysqlConn* c)
{
    if (!c)
        return;
    // broken的连接也照常还池，由AcquireConn在取出时按节流规则重连
    free_conns_.push_back(c);
}

// ============================================================================
// 协程化的非阻塞调用
// ============================================================================

int MysqlWrap::WaitSocket(MysqlConn& c, int status)
{
    int fd = mysql_get_socket(c.conn);

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    if (status & MYSQL_WAIT_READ)
        ev.events |= EPOLLIN;
    if (status & MYSQL_WAIT_WRITE)
        ev.events |= EPOLLOUT;
    if (status & MYSQL_WAIT_EXCEPT)
        ev.events |= EPOLLPRI;
    ev.data.u32 = static_cast<uint32_t>(c.index);

    // 库自己要求的超时（如握手/读超时）优先，但不超过op_timeout_ms
    uint32_t timeout_ms = conf_.op_timeout_ms;
    bool use_lib_timeout = false;
    if (status & MYSQL_WAIT_TIMEOUT)
    {
        unsigned int lib_ms = mysql_get_timeout_value_ms(c.conn);
        if (lib_ms > 0 && lib_ms <= timeout_ms)
        {
            timeout_ms = lib_ms;
            use_lib_timeout = true;
        }
    }

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        APP_LOG_ERROR(0, "epoll_ctl add fail, conn[%d], fd=%d, errno=%d", c.index, fd, errno);
        c.broken = true;
        return -1;
    }

    // wait_ctx放在协程栈上：每个协程独立mmap栈，挂起期间内容不会被搬动，
    // Proc()回写ready_status是安全的（见app_coroutine.cpp）
    MysqlWaitContext wait_ctx;
    c.wait_seq = app::IDGenerator::GetInst().GenerateSeqID();
    ++in_flight_;

    int32_t ret = context_ctrl_->Pending(c.wait_seq, timeout_ms, &wait_ctx, {nullptr});

    --in_flight_;
    c.wait_seq = 0;
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);

    if (ret != app::RPC_SUCCESS)
    {
        APP_LOG_ERROR(0, "mysql pending fail, conn[%d], ret=%d", c.index, ret);
        c.broken = true;
        return -1;
    }

    if (wait_ctx.ret_code == app::RPC_TIME_OUT)
    {
        if (use_lib_timeout)
            return MYSQL_WAIT_TIMEOUT;  // 库主动要的超时，交回库自己处理

        // 我们的操作超时：查询还在飞，连接协议状态不明，只能弃用重连
        APP_LOG_WARN(0, "mysql op timeout, conn[%d], timeout=%ums", c.index, timeout_ms);
        c.broken = true;
        return -1;
    }

    return wait_ctx.ready_status;
}

template <typename Start, typename Cont>
bool MysqlWrap::RunAsync(MysqlConn& c, const Start& start, const Cont& cont)
{
    int status = start();
    while (status)
    {
        status = WaitSocket(c, status);
        if (status < 0)
            return false;
        status = cont(status);
    }
    return true;
}

bool MysqlWrap::StmtExecute(MysqlConn& c, MYSQL_STMT* stmt, uint64_t gid, const char* what)
{
    int r = 0;
    if (!RunAsync(c, [&] { return mysql_stmt_execute_start(&r, stmt); },
                  [&](int s) { return mysql_stmt_execute_cont(&r, stmt, s); }))
        return false;

    if (r != 0)
    {
        unsigned int err = mysql_stmt_errno(stmt);
        APP_LOG_WARN(gid, "%s execute fail, errno=%u, err=%s", what, err, mysql_stmt_error(stmt));
        if (IsConnLevelError(err))
            c.broken = true;
        return false;
    }
    return true;
}

int MysqlWrap::StmtFetchOne(MysqlConn& c, MYSQL_STMT* stmt, uint64_t gid, const char* what)
{
    int r = 0;
    if (!RunAsync(c, [&] { return mysql_stmt_store_result_start(&r, stmt); },
                  [&](int s) { return mysql_stmt_store_result_cont(&r, stmt, s); }))
        return DB_ERR_MYSQL;
    if (r != 0)
    {
        unsigned int err = mysql_stmt_errno(stmt);
        APP_LOG_WARN(gid, "%s store_result fail, errno=%u, err=%s", what, err, mysql_stmt_error(stmt));
        if (IsConnLevelError(err))
            c.broken = true;
        return DB_ERR_MYSQL;
    }

    // 结果已经缓存到本地，fetch不再产生网络IO，但仍走异步壳保持统一
    if (!RunAsync(c, [&] { return mysql_stmt_fetch_start(&r, stmt); },
                  [&](int s) { return mysql_stmt_fetch_cont(&r, stmt, s); }))
        return DB_ERR_MYSQL;

    if (r == MYSQL_NO_DATA)
        return DB_ERR_NOT_DATA;
    // MYSQL_DATA_TRUNCATED按原实现容忍：字符串缓冲与列宽一致，只可能在超长时截断
    if (r != 0 && r != MYSQL_DATA_TRUNCATED)
    {
        APP_LOG_WARN(gid, "%s fetch fail, r=%d, err=%s", what, r, mysql_stmt_error(stmt));
        return DB_ERR_MYSQL;
    }
    return 0;
}

void MysqlWrap::StmtFreeResult(MysqlConn& c, MYSQL_STMT* stmt)
{
    my_bool r = 0;
    RunAsync(c, [&] { return mysql_stmt_free_result_start(&r, stmt); },
             [&](int s) { return mysql_stmt_free_result_cont(&r, stmt, s); });
}

int MysqlWrap::CheckCasMiss(MysqlConn& c, StmtSlot ver_slot, uint64_t gid)
{
    MYSQL_STMT* stmt = c.stmt[ver_slot];
    uint64_t key = gid;

    MYSQL_BIND param;
    memset(&param, 0, sizeof(param));
    BindU64(param, key);
    if (mysql_stmt_bind_param(stmt, &param) != 0)
        return DB_ERR_MYSQL;

    if (!StmtExecute(c, stmt, gid, "CheckCasMiss"))
        return DB_ERR_MYSQL;

    int32_t cur_version = 0;
    MYSQL_BIND result;
    memset(&result, 0, sizeof(result));
    BindI32(result, cur_version);
    if (mysql_stmt_bind_result(stmt, &result) != 0)
        return DB_ERR_MYSQL;

    int ret = StmtFetchOne(c, stmt, gid, "CheckCasMiss");
    StmtFreeResult(c, stmt);

    if (ret != 0)
        return ret;  // DB_ERR_NOT_DATA表示记录不存在，调用方改走插入

    APP_LOG_WARN(gid, "set version mismatch, cur=%d", cur_version);
    return DB_ERR_INVALID_VERSION;
}

// ============================================================================
// LOGIN
// ============================================================================

int MysqlWrap::GetLogin(uint64_t gid, LOGIN& out, int32_t& data_version)
{
    memset(&out, 0, sizeof(out));
    out.ullGid = gid;
    data_version = 0;

    MysqlConn* c = AcquireConn(gid);
    if (!c)
        return DB_ERR_BUSY;

    MYSQL_STMT* stmt = c->stmt[kStmtGetLogin];
    uint64_t key = gid;

    MYSQL_BIND param;
    memset(&param, 0, sizeof(param));
    BindU64(param, key);

    uint32_t login_flag = 0;
    int32_t dv = 0;
    MYSQL_BIND result[2];
    memset(result, 0, sizeof(result));
    BindU32(result[0], login_flag);
    BindI32(result[1], dv);

    int ret = DB_ERR_MYSQL;
    if (mysql_stmt_bind_param(stmt, &param) == 0 && StmtExecute(*c, stmt, gid, "GetLogin") &&
        mysql_stmt_bind_result(stmt, result) == 0)
    {
        ret = StmtFetchOne(*c, stmt, gid, "GetLogin");
        StmtFreeResult(*c, stmt);
    }

    ReleaseConn(c);

    if (ret != 0)
        return ret;

    out.dwLogin_flag = login_flag;
    data_version = dv;
    return 0;
}

int MysqlWrap::SetLogin(uint64_t gid, const LOGIN& in, int32_t data_version)
{
    MysqlConn* c = AcquireConn(gid);
    if (!c)
        return DB_ERR_BUSY;

    uint64_t key = gid;
    uint32_t login_flag = in.dwLogin_flag;
    int ret = 0;

    if (data_version != 0)
    {
        // 调用方显式带了版本号：条件UPDATE做CAS。
        // data_version必增，所以affected_rows>0就等于命中了目标版本的行。
        MYSQL_STMT* stmt = c->stmt[kStmtCasLogin];
        int32_t dv = data_version;
        MYSQL_BIND param[3];
        memset(param, 0, sizeof(param));
        BindU32(param[0], login_flag);
        BindU64(param[1], key);
        BindI32(param[2], dv);

        if (mysql_stmt_bind_param(stmt, param) != 0 || !StmtExecute(*c, stmt, gid, "SetLogin(cas)"))
        {
            ReleaseConn(c);
            return DB_ERR_MYSQL;
        }
        if (mysql_stmt_affected_rows(stmt) > 0)
        {
            ReleaseConn(c);
            return 0;
        }
        // 0行受影响：区分记录不存在和版本冲突
        ret = CheckCasMiss(*c, kStmtGetLoginVer, gid);
        if (ret != DB_ERR_NOT_DATA)
        {
            ReleaseConn(c);
            return ret;
        }
        // 记录不存在 → 落到下面的upsert插入，
        // 与原实现「exists==false时忽略传入版本号直接INSERT」的行为一致
    }

    // data_version==0：无条件覆盖写，等价tcaplus的REPLACE_REQ语义，一次往返搞定
    MYSQL_STMT* stmt = c->stmt[kStmtUpsertLogin];
    MYSQL_BIND param[3];
    memset(param, 0, sizeof(param));
    BindU64(param[0], key);
    BindU32(param[1], login_flag);
    BindU32(param[2], login_flag);  // ON DUPLICATE KEY UPDATE 的同一个值

    ret = 0;
    if (mysql_stmt_bind_param(stmt, param) != 0 || !StmtExecute(*c, stmt, gid, "SetLogin(upsert)"))
        ret = DB_ERR_MYSQL;

    ReleaseConn(c);
    return ret;
}

// ============================================================================
// USER_INFO
// ============================================================================

int MysqlWrap::GetUserInfo(uint64_t gid, USER_INFO& out, int32_t& data_version)
{
    memset(&out, 0, sizeof(out));
    out.ullGid = gid;
    data_version = 0;

    MysqlConn* c = AcquireConn(gid);
    if (!c)
        return DB_ERR_BUSY;

    MYSQL_STMT* stmt = c->stmt[kStmtGetUserInfo];
    uint64_t key = gid;

    MYSQL_BIND param;
    memset(&param, 0, sizeof(param));
    BindU64(param, key);

    uint32_t is_new = 0;
    uint32_t role_type = 0;
    uint64_t points = 0;
    int32_t dv = 0;
    char user_name[sizeof(out.szUser_name)] = {};
    unsigned long user_name_len = 0;
    my_bool user_name_is_null = 0;

    MYSQL_BIND result[5];
    memset(result, 0, sizeof(result));
    BindU32(result[0], is_new);
    BindU32(result[1], role_type);
    BindStr(result[2], user_name, user_name_len, sizeof(user_name));
    result[2].is_null = &user_name_is_null;
    BindU64(result[3], points);
    BindI32(result[4], dv);

    int ret = DB_ERR_MYSQL;
    if (mysql_stmt_bind_param(stmt, &param) == 0 && StmtExecute(*c, stmt, gid, "GetUserInfo") &&
        mysql_stmt_bind_result(stmt, result) == 0)
    {
        ret = StmtFetchOne(*c, stmt, gid, "GetUserInfo");
        StmtFreeResult(*c, stmt);
    }

    ReleaseConn(c);

    if (ret != 0)
        return ret;

    out.dwIs_new = is_new;
    out.dwRole_type = role_type;
    size_t copy_len = std::min<unsigned long>(user_name_len, sizeof(out.szUser_name) - 1);
    if (!user_name_is_null && copy_len > 0)
        memcpy(out.szUser_name, user_name, copy_len);
    out.ullPoints = points;
    data_version = dv;
    return 0;
}

int MysqlWrap::SetUserInfo(uint64_t gid, const USER_INFO& in, int32_t data_version)
{
    MysqlConn* c = AcquireConn(gid);
    if (!c)
        return DB_ERR_BUSY;

    uint64_t key = gid;
    uint32_t is_new = in.dwIs_new;
    uint32_t role_type = in.dwRole_type;
    uint64_t points = in.ullPoints;
    char* name = const_cast<char*>(in.szUser_name);
    unsigned long name_len = strnlen(in.szUser_name, sizeof(in.szUser_name));
    int ret = 0;

    if (data_version != 0)
    {
        MYSQL_STMT* stmt = c->stmt[kStmtCasUserInfo];
        int32_t dv = data_version;
        MYSQL_BIND param[6];
        memset(param, 0, sizeof(param));
        BindU32(param[0], is_new);
        BindU32(param[1], role_type);
        BindStr(param[2], name, name_len, name_len);
        BindU64(param[3], points);
        BindU64(param[4], key);
        BindI32(param[5], dv);

        if (mysql_stmt_bind_param(stmt, param) != 0 || !StmtExecute(*c, stmt, gid, "SetUserInfo(cas)"))
        {
            ReleaseConn(c);
            return DB_ERR_MYSQL;
        }
        if (mysql_stmt_affected_rows(stmt) > 0)
        {
            ReleaseConn(c);
            return 0;
        }
        ret = CheckCasMiss(*c, kStmtGetUserInfoVer, gid);
        if (ret != DB_ERR_NOT_DATA)
        {
            ReleaseConn(c);
            return ret;
        }
    }

    MYSQL_STMT* stmt = c->stmt[kStmtUpsertUserInfo];
    MYSQL_BIND param[9];
    memset(param, 0, sizeof(param));
    BindU64(param[0], key);
    BindU32(param[1], is_new);
    BindU32(param[2], role_type);
    BindStr(param[3], name, name_len, name_len);
    BindU64(param[4], points);
    // ON DUPLICATE KEY UPDATE 部分，绑同一批缓冲
    BindU32(param[5], is_new);
    BindU32(param[6], role_type);
    BindStr(param[7], name, name_len, name_len);
    BindU64(param[8], points);

    ret = 0;
    if (mysql_stmt_bind_param(stmt, param) != 0 || !StmtExecute(*c, stmt, gid, "SetUserInfo(upsert)"))
        ret = DB_ERR_MYSQL;

    ReleaseConn(c);
    return ret;
}

}  // namespace dbproxy
