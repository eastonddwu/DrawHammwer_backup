/*
 * * file name: server_core.cpp
 * * description: ...
 * */

#include "server_core.h"
#include "common/id_generator.h"
#include "common/metrics.h"
#include "common/utils.h"
#include "coro_mgr.h"
#include "interface/channel_interface.h"
#include "log.h"

namespace app
{
bool ServerCore::SvrInit(const SvrOption& option)
{
    option_ = option;

    if (!IDGenerator::GetInst().Init())
    {
        APP_LOG_ERROR(0, "id generator init fail");
        return false;
    }

    if (!option_.coroutine)
    {
        APP_LOG_ERROR(0, "coroutine plugin is required");
        return false;
    }
    option_.coroutine->SetMaxCoroNum(option_.max_coro_num);
    CoroMgr::SetCoroutine(option_.coroutine);

    if (!context_ctrl_.Init())
    {
        APP_LOG_ERROR(0, "context controller init fail");
        return false;
    }

    RpcService::GetInst().SetContextCtrl(&context_ctrl_);

    if (!OnInit())
        return false;

    APP_LOG_INFO(0, "ServerCore SvrInit ok");
    return true;
}

void ServerCore::SvrTick(uint64_t now_ms, uint64_t tick_count)
{
    OnTick(now_ms, tick_count);
}

size_t ServerCore::SvrProc(uint64_t now_ms)
{
    // 阶段1：处理超时上下文
    uint32_t ctx_count = context_ctrl_.ProcTimeOut(now_ms);

    // 阶段2：处理定时事件（简化版MVP暂不实现TimeoutDecorator，留待后续扩展）

    // 阶段3：业务自定义逻辑
    uint32_t proc_count = static_cast<uint32_t>(OnProc(now_ms, stop_));

    // 阶段4：驱动收包（只驱动default transport，其余channel由业务在OnProc()里手动驱动）
    uint32_t deal_pkg_count = 0;
    if (!stop_)
    {
        auto&& transport_info = DefaultTransportInfo();
        if (transport_info && transport_info->channel)
            deal_pkg_count += static_cast<uint32_t>(transport_info->channel->Loop(option_.max_deal_pkg_num));
    }

    if (Metrics::GetInst().Enabled())
    {
        size_t running_coro = 0;
        size_t total_coro = 0;
        size_t max_coro = 0;
        if (CoroMgr::HasCoroutine())
        {
            auto&& coro_mgr = CoroMgr::GetInst();
            running_coro = coro_mgr.GetRunningCoro();
            total_coro = coro_mgr.GetTotalCoro();
            max_coro = coro_mgr.GetMaxCoroNum();
        }
        Metrics::GetInst().OnFrame(deal_pkg_count, option_.max_deal_pkg_num, running_coro, total_coro, max_coro);
    }

    return ctx_count + proc_count + deal_pkg_count;
}

bool ServerCore::SvrFinish()
{
    OnFinish();
    APP_LOG_INFO(0, "finish(%d)", stop_);
    return true;
}

void ServerCore::SvrNtfQuit()
{
    if (!stop_)
    {
        stop_ = true;
        APP_LOG_INFO(0, "ntf quit, pending ctx num(%lu) coro(%lu)", context_ctrl_.PendingContextNum(),
                     context_ctrl_.PendingCoroutineNum());
    }
}

bool ServerCore::SvrStopReady() const
{
    if (stop_)
    {
        if (context_ctrl_.PendingContextNum() == 0)
            return true;

        static uint64_t last_log_time = 0;
        if (last_log_time + 200 < utils::CurrentRealMilliSec())
        {
            APP_LOG_WARN(0, "pending context(%lu) coroutine(%lu)", context_ctrl_.PendingContextNum(),
                         context_ctrl_.PendingCoroutineNum());
            last_log_time = utils::CurrentRealMilliSec();
        }
    }

    return false;
}

bool ServerCore::AddTransportInfo(uint32_t transport_type, const TransportInfo& info, bool is_default)
{
    if (!RpcService::GetInst().AddTransport(transport_type, info))
        return false;
    if (is_default)
        default_transport_ = transport_type;
    return true;
}

const TransportInfo* ServerCore::DefaultTransportInfo() const
{
    return RpcService::GetInst().FindTransport(default_transport_);
}

const TransportInfo* ServerCore::FindTransportInfo(uint32_t transport_type) const
{
    return RpcService::GetInst().FindTransport(transport_type);
}

}  // namespace app
