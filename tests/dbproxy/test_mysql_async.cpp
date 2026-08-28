/*
 * * file name: test_mysql_async.cpp
 * * description: MysqlWrap协程异步实现的独立验证程序，不依赖tbus2/tcaplus/TCM，
 * *              自己拉起协程框架 + ContextController 后直接驱动 MysqlWrap。
 * *
 * *              四个场景:
 * *                1. correctness — Set后Get回读比对全字段（含中文名、边界长度、覆盖写）
 * *                2. cas         — data_version乐观锁: 正确版本成功/错误版本冲突/记录不存在时插入
 * *                3. concurrent  — N个协程各M轮 Set+Get，测异步实现的吞吐
 * *                4. contention  — N个协程并发写同一gid，验证去掉SELECT FOR UPDATE后无丢失更新
 * *                5. sync_base   — 同进程内用改造前的同步写法(显式事务+每次prepare)
 * *                                 串行跑同样的量，作为对照基线
 * *
 * *              用法: test_mysql_async [conf_file] [coro_num] [round_per_coro]
 * *              默认 conf/mysql.conf 32 20
 * */

#include <mysql.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "common/clock.h"
#include "common/id_generator.h"
#include "common/utils.h"
#include "core/context_controller.h"
#include "core/coro_mgr.h"
#include "coroutine/app_coroutine.h"
#include "core/log_service.h"
#include "mysql_wrap.h"
#include "table/tb_app_tcaplus.h"
#include "utils/db_conf.h"
#include "utils/db_error.h"

using namespace dbproxy;

// 测试专用gid段，避开真实玩家数据（真实gid来自tconnd openid，量级在百万内）
static const uint64_t kTestGidBase = 9000000000ULL;

static app::ContextController g_ctrl;
static int g_fail = 0;

#define CHECK(cond, fmt, ...)                                                     \
    do                                                                            \
    {                                                                             \
        if (!(cond))                                                              \
        {                                                                         \
            printf("  [FAIL] " fmt "\n", ##__VA_ARGS__);                          \
            g_fail++;                                                             \
        }                                                                         \
    } while (0)

// ============================================================================
// 主循环：协程挂起后必须由主协程驱动，否则永远醒不过来
// ============================================================================

// 驱动到所有协程跑完（done_num达到expect）或超时
static void RunUntilDone(const int& done_num, int expect, const char* what)
{
    uint64_t deadline = app::utils::CurrentRealMilliSec() + 60000;
    while (done_num < expect)
    {
        app::Clock::GetInst().Update(app::utils::CurrentRealMicroSec());
        g_ctrl.ProcTimeOut(app::Clock::GetInst().CurrentMilliSec());
        MysqlWrap::GetInst().Proc();

        if (app::utils::CurrentRealMilliSec() > deadline)
        {
            printf("  [FAIL] %s timeout, done=%d/%d, pending_ctx=%zu\n", what, done_num, expect,
                   g_ctrl.PendingContextNum());
            g_fail++;
            return;
        }
    }
}

// 在协程里跑一段逻辑并等它结束
static void RunInCoro(const std::function<void()>& task, const char* what)
{
    int done = 0;
    app::CoroMgr::GetInst().Spawn([&] {
        task();
        done++;
    });
    RunUntilDone(done, 1, what);
}

// ============================================================================
// 场景1: 正确性
// ============================================================================

static void CaseCorrectness()
{
    printf("── case: correctness ──\n");
    auto&& db = MysqlWrap::GetInst();

    RunInCoro(
        [&] {
            const uint64_t gid = kTestGidBase + 1;

            // 1) 首次写入（记录不存在，走upsert的INSERT分支）
            USER_INFO in;
            memset(&in, 0, sizeof(in));
            in.ullGid = gid;
            in.dwIs_new = 1;
            in.dwRole_type = 3;
            snprintf(in.szUser_name, sizeof(in.szUser_name), "画个锤子");
            in.ullPoints = 12345;
            CHECK(db.SetUserInfo(gid, in, 0) == 0, "SetUserInfo insert");

            USER_INFO out;
            int32_t dv = 0;
            int ret = db.GetUserInfo(gid, out, dv);
            CHECK(ret == 0, "GetUserInfo after insert, ret=%d", ret);
            CHECK(out.dwIs_new == 1, "is_new=%u, want 1", out.dwIs_new);
            CHECK(out.dwRole_type == 3, "role_type=%u, want 3", out.dwRole_type);
            CHECK(strcmp(out.szUser_name, "画个锤子") == 0, "user_name=\"%s\", want \"画个锤子\"",
                  out.szUser_name);
            CHECK(out.ullPoints == 12345, "points=%lu, want 12345", out.ullPoints);
            CHECK(dv >= 1, "data_version=%d, want >=1", dv);
            int32_t dv_after_insert = dv;

            // 2) 覆盖写（记录已存在，走upsert的UPDATE分支），data_version应+1
            in.dwIs_new = 0;
            in.dwRole_type = 7;
            snprintf(in.szUser_name, sizeof(in.szUser_name), "覆盖后的名字");
            in.ullPoints = 999;
            CHECK(db.SetUserInfo(gid, in, 0) == 0, "SetUserInfo overwrite");

            ret = db.GetUserInfo(gid, out, dv);
            CHECK(ret == 0, "GetUserInfo after overwrite, ret=%d", ret);
            CHECK(out.dwIs_new == 0, "is_new=%u, want 0", out.dwIs_new);
            CHECK(out.dwRole_type == 7, "role_type=%u, want 7", out.dwRole_type);
            CHECK(strcmp(out.szUser_name, "覆盖后的名字") == 0, "user_name=\"%s\"", out.szUser_name);
            CHECK(out.ullPoints == 999, "points=%lu, want 999", out.ullPoints);
            CHECK(dv == dv_after_insert + 1, "data_version=%d, want %d", dv, dv_after_insert + 1);

            // 3) 边界长度用户名（127字节，szUser_name容量128含结尾\0）
            memset(in.szUser_name, 0, sizeof(in.szUser_name));
            memset(in.szUser_name, 'x', 127);
            CHECK(db.SetUserInfo(gid, in, 0) == 0, "SetUserInfo max-len name");
            ret = db.GetUserInfo(gid, out, dv);
            CHECK(ret == 0, "GetUserInfo max-len name, ret=%d", ret);
            CHECK(strnlen(out.szUser_name, sizeof(out.szUser_name)) == 127, "name_len=%zu, want 127",
                  strnlen(out.szUser_name, sizeof(out.szUser_name)));

            // 4) 不存在的记录返回DB_ERR_NOT_DATA
            ret = db.GetUserInfo(kTestGidBase + 99999, out, dv);
            CHECK(ret == DB_ERR_NOT_DATA, "GetUserInfo missing row, ret=%d, want %d", ret, DB_ERR_NOT_DATA);

            // 5) login表同样走一遍
            LOGIN lin;
            memset(&lin, 0, sizeof(lin));
            lin.ullGid = gid;
            lin.dwLogin_flag = 1;
            CHECK(db.SetLogin(gid, lin, 0) == 0, "SetLogin insert");
            LOGIN lout;
            ret = db.GetLogin(gid, lout, dv);
            CHECK(ret == 0, "GetLogin, ret=%d", ret);
            CHECK(lout.dwLogin_flag == 1, "login_flag=%u, want 1", lout.dwLogin_flag);

            lin.dwLogin_flag = 42;
            CHECK(db.SetLogin(gid, lin, 0) == 0, "SetLogin overwrite");
            ret = db.GetLogin(gid, lout, dv);
            CHECK(ret == 0, "GetLogin after overwrite, ret=%d", ret);
            CHECK(lout.dwLogin_flag == 42, "login_flag=%u, want 42", lout.dwLogin_flag);
        },
        "correctness");

    printf("  done\n");
}

// ============================================================================
// 场景2: data_version 乐观锁
// ============================================================================

static void CaseCas()
{
    printf("── case: cas (data_version) ──\n");
    auto&& db = MysqlWrap::GetInst();

    RunInCoro(
        [&] {
            const uint64_t gid = kTestGidBase + 2;

            USER_INFO in;
            memset(&in, 0, sizeof(in));
            in.ullGid = gid;
            in.dwRole_type = 1;
            snprintf(in.szUser_name, sizeof(in.szUser_name), "cas_user");
            CHECK(db.SetUserInfo(gid, in, 0) == 0, "prepare row");

            USER_INFO out;
            int32_t dv = 0;
            CHECK(db.GetUserInfo(gid, out, dv) == 0, "read current version");

            // 1) 带正确版本号 → 成功，版本+1
            in.dwRole_type = 2;
            int ret = db.SetUserInfo(gid, in, dv);
            CHECK(ret == 0, "cas with correct version, ret=%d", ret);
            int32_t new_dv = 0;
            CHECK(db.GetUserInfo(gid, out, new_dv) == 0, "read after cas");
            CHECK(new_dv == dv + 1, "data_version=%d, want %d", new_dv, dv + 1);
            CHECK(out.dwRole_type == 2, "role_type=%u, want 2", out.dwRole_type);

            // 2) 带过期版本号 → 冲突，且数据不变
            in.dwRole_type = 3;
            ret = db.SetUserInfo(gid, in, dv);
            CHECK(ret == DB_ERR_INVALID_VERSION, "cas with stale version, ret=%d, want %d", ret,
                  DB_ERR_INVALID_VERSION);
            CHECK(db.GetUserInfo(gid, out, dv) == 0, "read after stale cas");
            CHECK(out.dwRole_type == 2, "role_type=%u after rejected cas, want 2", out.dwRole_type);

            // 3) 记录不存在时带版本号 → 按原实现语义忽略版本号直接插入
            const uint64_t fresh_gid = kTestGidBase + 3;
            in.dwRole_type = 9;
            ret = db.SetUserInfo(fresh_gid, in, 100);
            CHECK(ret == 0, "cas on missing row should insert, ret=%d", ret);
            CHECK(db.GetUserInfo(fresh_gid, out, dv) == 0, "read inserted row");
            CHECK(out.dwRole_type == 9, "role_type=%u, want 9", out.dwRole_type);

            // 4) login表的CAS
            LOGIN lin;
            memset(&lin, 0, sizeof(lin));
            lin.dwLogin_flag = 1;
            CHECK(db.SetLogin(gid, lin, 0) == 0, "prepare login row");
            LOGIN lout;
            CHECK(db.GetLogin(gid, lout, dv) == 0, "read login version");
            lin.dwLogin_flag = 2;
            CHECK(db.SetLogin(gid, lin, dv) == 0, "login cas correct version");
            ret = db.SetLogin(gid, lin, dv);
            CHECK(ret == DB_ERR_INVALID_VERSION, "login cas stale version, ret=%d", ret);
        },
        "cas");

    printf("  done\n");
}

// ============================================================================
// 场景3: 并发吞吐（异步实现）
// ============================================================================

// 每轮的操作数：1次Set + 1次Get
static const int kOpsPerRound = 2;

static void CaseConcurrent(int coro_num, int rounds)
{
    printf("── case: concurrent (async) — %d coro × %d round ──\n", coro_num, rounds);
    auto&& db = MysqlWrap::GetInst();

    int done = 0;
    int err = 0;
    uint64_t start_us = app::utils::CurrentRealMicroSec();

    for (int i = 0; i < coro_num; i++)
    {
        bool spawned = app::CoroMgr::GetInst().Spawn([&, i] {
            for (int r = 0; r < rounds; r++)
            {
                uint64_t gid = kTestGidBase + 1000 + i;

                USER_INFO in;
                memset(&in, 0, sizeof(in));
                in.ullGid = gid;
                in.dwIs_new = 0;
                in.dwRole_type = static_cast<uint32_t>(r);
                snprintf(in.szUser_name, sizeof(in.szUser_name), "bench_%d_%d", i, r);
                in.ullPoints = static_cast<uint64_t>(r);
                if (db.SetUserInfo(gid, in, 0) != 0)
                    err++;

                USER_INFO out;
                int32_t dv = 0;
                if (db.GetUserInfo(gid, out, dv) != 0)
                    err++;
                else if (out.dwRole_type != static_cast<uint32_t>(r))
                    err++;  // 自己写的自己读，必须读到刚写的值
            }
            done++;
        });
        if (!spawned)
        {
            printf("  [FAIL] spawn coro %d fail\n", i);
            g_fail++;
            break;
        }
    }

    RunUntilDone(done, coro_num, "concurrent");
    uint64_t cost_us = app::utils::CurrentRealMicroSec() - start_us;

    int total_ops = coro_num * rounds * kOpsPerRound;
    double qps = cost_us > 0 ? total_ops * 1000000.0 / cost_us : 0;
    printf("  ops=%d cost=%.1fms qps=%.0f err=%d\n", total_ops, cost_us / 1000.0, qps, err);
    CHECK(err == 0, "concurrent err_count=%d", err);
}

// ============================================================================
// 场景4: 同一gid并发写 — 验证去掉SELECT FOR UPDATE后仍然没有丢失更新
// ============================================================================

// 改造前靠「SELECT ... FOR UPDATE」持行锁串行化同gid的写入，现在靠单条语句的
// 原子性。data_version=data_version+1 由MySQL在同一条语句里完成，所以N次成功写入
// 之后版本号必须正好涨N，一次都不能丢。
static void CaseContention(int coro_num, int rounds)
{
    printf("── case: contention (同一gid并发写) — %d coro × %d round ──\n", coro_num, rounds);
    auto&& db = MysqlWrap::GetInst();
    const uint64_t gid = kTestGidBase + 4;

    int done = 0;
    int ok_write = 0;
    int err = 0;

    // 先建好这一行并取到初始版本号
    RunInCoro(
        [&] {
            USER_INFO in;
            memset(&in, 0, sizeof(in));
            in.ullGid = gid;
            snprintf(in.szUser_name, sizeof(in.szUser_name), "contention");
            CHECK(db.SetUserInfo(gid, in, 0) == 0, "prepare contention row");
        },
        "contention-prepare");

    int32_t base_dv = 0;
    RunInCoro(
        [&] {
            USER_INFO out;
            CHECK(db.GetUserInfo(gid, out, base_dv) == 0, "read base version");
        },
        "contention-base");

    for (int i = 0; i < coro_num; i++)
    {
        app::CoroMgr::GetInst().Spawn([&, i] {
            for (int r = 0; r < rounds; r++)
            {
                USER_INFO in;
                memset(&in, 0, sizeof(in));
                in.ullGid = gid;
                in.dwRole_type = static_cast<uint32_t>(i);
                in.ullPoints = static_cast<uint64_t>(r);
                snprintf(in.szUser_name, sizeof(in.szUser_name), "c_%d_%d", i, r);
                if (db.SetUserInfo(gid, in, 0) == 0)
                    ok_write++;
                else
                    err++;
            }
            done++;
        });
    }
    RunUntilDone(done, coro_num, "contention");

    int32_t final_dv = 0;
    RunInCoro(
        [&] {
            USER_INFO out;
            CHECK(db.GetUserInfo(gid, out, final_dv) == 0, "read final version");
        },
        "contention-final");

    printf("  ok_write=%d err=%d data_version %d → %d (delta=%d)\n", ok_write, err, base_dv, final_dv,
           final_dv - base_dv);
    CHECK(err == 0, "contention err_count=%d", err);
    // 每次成功写入必须让版本号涨且只涨1，delta与成功次数一致才说明没有丢失更新
    CHECK(final_dv - base_dv == ok_write, "data_version delta=%d, want %d (lost update!)",
          final_dv - base_dv, ok_write);
}

// ============================================================================
// 场景5: 同步基线 — 复刻改造前的写法作为对照
// ============================================================================

// 改造前的SetUserInfo: START TRANSACTION + SELECT FOR UPDATE + INSERT/UPDATE + COMMIT，
// 且每次操作都新建/销毁prepared statement
static bool SyncSetUserInfo(MYSQL* conn, uint64_t gid, uint32_t role_type)
{
    if (mysql_query(conn, "START TRANSACTION") != 0)
        return false;

    bool exists = false;
    {
        const char* sql = "SELECT data_version FROM user_info WHERE gid=? FOR UPDATE";
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0)
        {
            mysql_stmt_close(stmt);
            mysql_query(conn, "ROLLBACK");
            return false;
        }
        MYSQL_BIND param;
        memset(&param, 0, sizeof(param));
        param.buffer_type = MYSQL_TYPE_LONGLONG;
        param.buffer = &gid;
        param.is_unsigned = 1;
        mysql_stmt_bind_param(stmt, &param);
        if (mysql_stmt_execute(stmt) != 0)
        {
            mysql_stmt_close(stmt);
            mysql_query(conn, "ROLLBACK");
            return false;
        }
        int32_t dv = 0;
        MYSQL_BIND result;
        memset(&result, 0, sizeof(result));
        result.buffer_type = MYSQL_TYPE_LONG;
        result.buffer = &dv;
        mysql_stmt_bind_result(stmt, &result);
        mysql_stmt_store_result(stmt);
        exists = (mysql_stmt_fetch(stmt) == 0);
        mysql_stmt_close(stmt);
    }

    {
        char name[64];
        snprintf(name, sizeof(name), "sync_%u", role_type);
        unsigned long name_len = strlen(name);
        uint32_t is_new = 0;
        uint64_t points = role_type;

        const char* sql = exists ? "UPDATE user_info SET is_new=?, role_type=?, user_name=?, points=?, "
                                   "data_version=data_version+1 WHERE gid=?"
                                 : "INSERT INTO user_info (gid, is_new, role_type, user_name, points, "
                                   "data_version) VALUES (?,?,?,?,?,1)";
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0)
        {
            mysql_stmt_close(stmt);
            mysql_query(conn, "ROLLBACK");
            return false;
        }
        MYSQL_BIND param[5];
        memset(param, 0, sizeof(param));
        int i = 0;
        if (!exists)
        {
            param[i].buffer_type = MYSQL_TYPE_LONGLONG;
            param[i].buffer = &gid;
            param[i].is_unsigned = 1;
            i++;
        }
        param[i].buffer_type = MYSQL_TYPE_LONG;
        param[i].buffer = &is_new;
        param[i].is_unsigned = 1;
        i++;
        param[i].buffer_type = MYSQL_TYPE_LONG;
        param[i].buffer = &role_type;
        param[i].is_unsigned = 1;
        i++;
        param[i].buffer_type = MYSQL_TYPE_STRING;
        param[i].buffer = name;
        param[i].buffer_length = name_len;
        param[i].length = &name_len;
        i++;
        param[i].buffer_type = MYSQL_TYPE_LONGLONG;
        param[i].buffer = &points;
        param[i].is_unsigned = 1;
        i++;
        if (exists)
        {
            param[i].buffer_type = MYSQL_TYPE_LONGLONG;
            param[i].buffer = &gid;
            param[i].is_unsigned = 1;
            i++;
        }
        mysql_stmt_bind_param(stmt, param);
        bool ok = (mysql_stmt_execute(stmt) == 0);
        mysql_stmt_close(stmt);
        if (!ok)
        {
            mysql_query(conn, "ROLLBACK");
            return false;
        }
    }

    return mysql_query(conn, "COMMIT") == 0;
}

// 改造前的GetUserInfo: 每次新建/销毁prepared statement
static bool SyncGetUserInfo(MYSQL* conn, uint64_t gid, uint32_t& role_type)
{
    const char* sql = "SELECT is_new, role_type, user_name, points, data_version FROM user_info WHERE gid=?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }
    MYSQL_BIND param;
    memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_LONGLONG;
    param.buffer = &gid;
    param.is_unsigned = 1;
    mysql_stmt_bind_param(stmt, &param);
    if (mysql_stmt_execute(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    uint32_t is_new = 0;
    uint64_t points = 0;
    int32_t dv = 0;
    char name[128] = {};
    unsigned long name_len = 0;
    MYSQL_BIND result[5];
    memset(result, 0, sizeof(result));
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &is_new;
    result[0].is_unsigned = 1;
    result[1].buffer_type = MYSQL_TYPE_LONG;
    result[1].buffer = &role_type;
    result[1].is_unsigned = 1;
    result[2].buffer_type = MYSQL_TYPE_STRING;
    result[2].buffer = name;
    result[2].buffer_length = sizeof(name);
    result[2].length = &name_len;
    result[3].buffer_type = MYSQL_TYPE_LONGLONG;
    result[3].buffer = &points;
    result[3].is_unsigned = 1;
    result[4].buffer_type = MYSQL_TYPE_LONG;
    result[4].buffer = &dv;
    mysql_stmt_bind_result(stmt, result);
    mysql_stmt_store_result(stmt);
    int r = mysql_stmt_fetch(stmt);
    mysql_stmt_close(stmt);
    return r == 0 || r == MYSQL_DATA_TRUNCATED;
}

static void CaseSyncBaseline(const MysqlConf& conf, int total_rounds)
{
    printf("── case: sync baseline (改造前写法，单连接串行) — %d round ──\n", total_rounds);

    MYSQL* conn = mysql_init(nullptr);
    if (!mysql_real_connect(conn, conf.host.c_str(), conf.user.c_str(), conf.password.c_str(),
                            conf.database.c_str(), conf.port, nullptr, 0))
    {
        printf("  [SKIP] connect fail: %s\n", mysql_error(conn));
        mysql_close(conn);
        return;
    }
    mysql_autocommit(conn, 0);

    int err = 0;
    uint64_t start_us = app::utils::CurrentRealMicroSec();
    for (int r = 0; r < total_rounds; r++)
    {
        uint64_t gid = kTestGidBase + 2000 + (r % 32);
        if (!SyncSetUserInfo(conn, gid, static_cast<uint32_t>(r)))
            err++;
        uint32_t role_type = 0;
        if (!SyncGetUserInfo(conn, gid, role_type))
            err++;
    }
    uint64_t cost_us = app::utils::CurrentRealMicroSec() - start_us;
    mysql_close(conn);

    int total_ops = total_rounds * kOpsPerRound;
    double qps = cost_us > 0 ? total_ops * 1000000.0 / cost_us : 0;
    printf("  ops=%d cost=%.1fms qps=%.0f err=%d\n", total_ops, cost_us / 1000.0, qps, err);
}

// ============================================================================
// 清理
// ============================================================================

static void Cleanup(const MysqlConf& conf)
{
    MYSQL* conn = mysql_init(nullptr);
    if (!mysql_real_connect(conn, conf.host.c_str(), conf.user.c_str(), conf.password.c_str(),
                            conf.database.c_str(), conf.port, nullptr, 0))
    {
        mysql_close(conn);
        return;
    }
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM user_info WHERE gid >= %llu",
             static_cast<unsigned long long>(kTestGidBase));
    mysql_query(conn, sql);
    snprintf(sql, sizeof(sql), "DELETE FROM login WHERE gid >= %llu",
             static_cast<unsigned long long>(kTestGidBase));
    mysql_query(conn, sql);
    mysql_close(conn);
}

// ============================================================================

int main(int argc, char* argv[])
{
    std::string conf_file = (argc > 1) ? argv[1] : "conf/mysql.conf";
    int coro_num = (argc > 2) ? atoi(argv[2]) : 32;
    int rounds = (argc > 3) ? atoi(argv[3]) : 20;

    if (!app::LogService::GetInst().Init("log", "test_mysql_async"))
    {
        fprintf(stderr, "LogService init fail\n");
        return 1;
    }
    if (!app::IDGenerator::GetInst().Init())
    {
        fprintf(stderr, "IDGenerator init fail\n");
        return 1;
    }
    app::Clock::GetInst().Update(app::utils::CurrentRealMicroSec());

    app::CoroutineMgr::GetInst().SetMaxCoroNum(10000);
    app::CoroMgr::SetCoroutine(&app::CoroutineMgr::GetInst());

    if (!g_ctrl.Init())
    {
        fprintf(stderr, "ContextController init fail\n");
        return 1;
    }

    DbConf db_conf;
    if (!db_conf.ParseMysqlFromFile(conf_file))
    {
        fprintf(stderr, "parse mysql conf fail: %s\n", conf_file.c_str());
        return 1;
    }
    const MysqlConf& conf = db_conf.mysql_conf;
    printf("mysql=%s:%u db=%s conn_num=%u op_timeout=%ums\n\n", conf.host.c_str(), conf.port,
           conf.database.c_str(), conf.conn_num, conf.op_timeout_ms);

    Cleanup(conf);

    int ret = MysqlWrap::GetInst().Init(conf, &g_ctrl);
    if (ret != 0)
    {
        fprintf(stderr, "MysqlWrap init fail, ret=%d\n", ret);
        return 1;
    }

    CaseCorrectness();
    CaseCas();
    CaseConcurrent(coro_num, rounds);
    CaseContention(coro_num, rounds);
    CaseSyncBaseline(conf, coro_num * rounds);

    MysqlWrap::GetInst().Finish();
    Cleanup(conf);
    app::LogService::GetInst().Fini();

    printf("\n%s (fail=%d)\n", g_fail == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", g_fail);
    return g_fail == 0 ? 0 : 1;
}
