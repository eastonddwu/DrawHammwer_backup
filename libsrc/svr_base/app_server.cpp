/*
 * * file name: app_server.cpp
 * * description: ...
 * */

#include "app_server.h"
#include <cstdlib>
#include <cstring>
#include <thread>
#include "common/clock.h"
#include "common/metrics.h"
#include "common/runtime_config.h"
#include "common/utils.h"
#include "core/log.h"
#include "core/log_service.h"
#include "tapp/tapp.h"

namespace app
{
// 单次tick最少间隔（毫秒）
static constexpr uint32_t TICK_INTERVAL_MS = 1000;
// 每多少次tick输出一次metrics汇总（tick为1秒一次，故为10秒）
static constexpr uint64_t kMetricsDumpTicks = 10;

int AppServer::Init(int argc, char* argv[], uint32_t svr_id, const std::string& log_dir)
{
    svr_id_ = svr_id;

    // module_name取自argv[0]的basename，作为tlog的category名和日志文件名前缀；
    // 必须在第一条APP_LOG_*调用之前完成，否则日志会被静默丢弃
    std::string module_name = (argc > 0 && argv[0]) ? argv[0] : "app_server";
    size_t slash_pos = module_name.find_last_of('/');
    if (slash_pos != std::string::npos)
        module_name = module_name.substr(slash_pos + 1);

    if (!LogService::GetInst().Init(log_dir, module_name))
    {
        fprintf(stderr, "LogService init fail, log_dir(%s), module_name(%s)\n", log_dir.c_str(),
                module_name.c_str());
        return -1;
    }

    // 性能埋点默认关闭，压测时设置环境变量APP_METRICS=1开启
    Metrics::GetInst().Init();

    int ret = InitTapp(argc, argv);
    if (ret < 0)
        return ret;

    if (!OnAppInit(svr_id_))
    {
        APP_LOG_ERROR(0, "OnAppInit error");
        return -1;
    }

    APP_LOG_INFO(0, "app server init succ, svr_id(%u), log_dir(%s), module_name(%s)", svr_id_, log_dir.c_str(),
                 module_name.c_str());
    return 0;
}

int AppServer::InitTapp(int argc, char* argv[])
{
    memset(&tapp_ctx_, 0, sizeof(tapp_ctx_));

    tapp_ctx_.argc = argc;
    tapp_ctx_.argv = argv;

    tapp_ctx_.pfnArgv = (PFNTAPPFUNC)AppArgv;
    tapp_ctx_.pfnFini = (PFNTAPPFUNC)AppFini;
    tapp_ctx_.pfnProc = (PFNTAPPFUNC)AppProc;
    tapp_ctx_.pfnIdle = (PFNTAPPFUNC)AppIdle;
    tapp_ctx_.pfnTick = (PFNTAPPFUNC)AppTick;
    tapp_ctx_.pfnReload = (PFNTAPPFUNC)AppReload;
    tapp_ctx_.pfnStop = (PFNTAPPFUNC)AppStop;
    tapp_ctx_.pfnQuit = (PFNTAPPFUNC)AppQuit;

    // 每秒一次tick调用
    tapp_ctx_.iTimer = TICK_INTERVAL_MS;
    // idle休眠时长（毫秒）。空闲时每帧sleep这么久，直接决定RPC延迟下界：
    // 一次跨进程RPC要经历「主调发出→被调下一帧收到→被调回包→主调下一帧收到」，
    // 每一跳都可能多等一个idle周期。可通过运行期配置APP_IDLE_SLEEP_MS调整，
    // 压测对比延迟时很有用（设为0则空转烧CPU，仅用于测量延迟下界，不可用于生产）。
    tapp_ctx_.iIdleSleep = static_cast<int>(
        ::strtol(runtime_config::Get("APP_IDLE_SLEEP_MS", "2").c_str(), nullptr, 10));
    // 简化版不使用tbus
    tapp_ctx_.iUseBus = 0;
    tapp_ctx_.iNoLoadConf = 1;

    return tapp_def_init(&tapp_ctx_, this);
}

bool AppServer::Reload()
{
    APP_LOG_INFO(0, "app reload");
    OnAppReload();
    return true;
}

int AppServer::Run()
{
    int ret = tapp_def_mainloop(&tapp_ctx_, this);
    if (ret != 0)
    {
        APP_LOG_ERROR(0, "tapp_def_mainloop error(%d)", ret);
        return ret;
    }

    bool result = Finish();
    if (!result)
    {
        APP_LOG_ERROR(0, "app Finish error");
    }

    return 0;
}

void AppServer::Tick()
{
    uint64_t now = utils::CurrentRealMilliSec();
    ++total_tick_count_;
    OnAppTick(now, total_tick_count_);

    // 开启埋点时定期输出汇总。用WARN级别：压测时日志级别通常调到warn，
    // 这样既能看到汇总，又不会混进ERROR里干扰真正的错误排查。
    if (Metrics::GetInst().Enabled() && total_tick_count_ % kMetricsDumpTicks == 0)
    {
        APP_LOG_WARN(0, "%s", Metrics::GetInst().Dump().c_str());
        Metrics::GetInst().Reset();
    }
}

size_t AppServer::Proc()
{
    Clock::GetInst().Update(utils::CurrentRealMicroSec());
    uint64_t now_ms = Clock::GetInst().CurrentMilliSec();
    size_t count = 0;
    if (IsProcReady())
        count = OnAppProc(now_ms);
    else
        count = OnAppPrepareProc(now_ms, has_stop_ntf_);

    if (OnAppIsReadyStop())
    {
        APP_LOG_INFO(0, "gracefully stop");
        tapp_exit_mainloop();
        return count;
    }

    return count;
}

size_t AppServer::OnAppPrepareProc(uint64_t now_ms, bool stop)
{
    SetProcReady(true);
    return 1;
}

void AppServer::Idle(uint32_t sleep_ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds{sleep_ms});
}

bool AppServer::Finish()
{
    APP_LOG_INFO(0, "app Finish");
    // 退出前补一次汇总，保证短时压测也能拿到完整数据（否则不足一个dump周期就什么都看不到）
    if (Metrics::GetInst().Enabled())
        APP_LOG_WARN(0, "final %s", Metrics::GetInst().Dump().c_str());
    bool result = OnAppFinish();
    LogService::GetInst().Fini();
    return result;
}

int AppServer::Quit()
{
    has_stop_ntf_ = true;
    OnAppQuit();
    // 返回0，表示并不马上退出，等到进程自己调用tapp_exit_mainloop
    return 0;
}

int AppServer::AppArgv(tagTAPPCTX* pstCtx, void* pvArg)
{
    return 0;
}

int AppServer::AppProc(tagTAPPCTX* pstCtx, void* pvArg)
{
    AppServer* app_svr = reinterpret_cast<AppServer*>(pvArg);
    // 接口要求返回小于0表示空闲
    return app_svr->Proc() > 0 ? 0 : -1;
}

int AppServer::AppTick(tagTAPPCTX* pstCtx, void* pvArg)
{
    AppServer* app_svr = reinterpret_cast<AppServer*>(pvArg);
    app_svr->Tick();
    return 0;
}

int AppServer::AppReload(tagTAPPCTX* pstCtx, void* pvArg)
{
    AppServer* app_svr = reinterpret_cast<AppServer*>(pvArg);
    return app_svr->Reload() ? 0 : -1;
}

int AppServer::AppIdle(tagTAPPCTX* pstCtx, void* pvArg)
{
    AppServer* app_svr = reinterpret_cast<AppServer*>(pvArg);
    app_svr->Idle(pstCtx->iIdleSleep);
    return 0;
}

int AppServer::AppQuit(tagTAPPCTX* pstCtx, void* pvArg)
{
    AppServer* app_svr = reinterpret_cast<AppServer*>(pvArg);
    return app_svr->Quit();
}

int AppServer::AppStop(tagTAPPCTX* pstCtx, void* pvArg)
{
    AppServer* app_svr = reinterpret_cast<AppServer*>(pvArg);
    return app_svr->Quit();
}

int AppServer::AppFini(tagTAPPCTX* pstCtx, void* pvArg)
{
    AppServer* app_svr = reinterpret_cast<AppServer*>(pvArg);
    return app_svr->Finish() ? 0 : -1;
}

}  // namespace app
