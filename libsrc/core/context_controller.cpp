/*
 * * file name: context_controller.cpp
 * * description: ...
 * */

#include "context_controller.h"
#include <cassert>
#include "common/clock.h"
#include "common/id_generator.h"
#include "coro_mgr.h"
#include "log.h"
#include "rpc_error.h"

namespace app
{
bool ContextController::Init()
{
    init_ = true;
    return true;
}

uint32_t ContextController::ProcTimeOut(uint64_t now)
{
    return timeout_queue_.TimeOut(now);
}

ClientContext* ContextController::Awake(uint64_t seq_id, int32_t ret_code)
{
    auto iter = context_cache_.find(seq_id);
    if (iter == context_cache_.end())
    {
        APP_LOG_WARN(0, "cache can not find seq_id(%lu), ret(%d)", seq_id, ret_code);
        return nullptr;
    }
    auto client_ctx = iter->second;
    assert(client_ctx);
    context_cache_.erase(iter);

    if (ret_code != RPC_TIME_OUT)
    {
        // 不是超时触发，需要删除定时器
        timeout_queue_.Cancel(client_ctx->timer_id);
    }

    APP_LOG_TRACE(0, "seq_id(%lu) awake, timer_id(%u), ret(%d)", seq_id, client_ctx->timer_id, ret_code);

    client_ctx->ret_code = ret_code;
    client_ctx->timer_id = 0;
    return client_ctx;
}

int32_t ContextController::Pending(uint64_t seq_id, uint32_t timeout, ClientContext* client_ctx, const AsyncTask& task)
{
    if (!client_ctx)
    {
        APP_LOG_ERROR(0, "params error");
        return RPC_SYS_ERR;
    }

    if (seq_id == 0)
        seq_id = IDGenerator::GetInst().GenerateSeqID();

    uint64_t expire_time = Clock::GetInst().CurrentMilliSec() + timeout;
    uint32_t timer_id = timeout_queue_.Add(
        [this, seq_id](uint32_t timer_id, uint32_t interval_time) {
            auto tmp_context = Awake(seq_id, RPC_TIME_OUT);
            if (tmp_context)
                tmp_context->Run();
        },
        expire_time);
    if (timer_id == 0)
    {
        APP_LOG_ERROR(0, "add context timer error seq_id(%lu)", seq_id);
        return RPC_SYS_ERR;
    }

    client_ctx->timer_id = timer_id;

    auto result = context_cache_.insert({seq_id, client_ctx});
    if (!result.second)
    {
        APP_LOG_ERROR(0, "context_cache insert error, seq_id(%lu)", seq_id);
        return RPC_SYS_ERR;
    }

    APP_LOG_TRACE(0, "seq_id(%lu) pending, timer_id(%u), expire_time(%lu)", seq_id, timer_id, expire_time);

    auto* server_ctx = client_ctx->server_ctx;
    // 有自定义的blocking操作，则用自定义的
    if (task.callback && task.blocking_fun)
    {
        client_ctx->SetCallback([server_ctx, cb = task.callback](int32_t ret_code) { cb(ret_code, server_ctx); },
                                task.recycle_fun);
        ContextMgr::SetCurrServerContext(nullptr);
        task.blocking_fun();
    }
    else
    {
        Coro* coro = CoroMgr::GetInst().ThisCoro();
        assert(coro);
        client_ctx->SetCallback(
            [server_ctx, cb = task.callback](int32_t ret_code) {
                if (cb)
                    cb(ret_code, server_ctx);
            },
            [coro]() { coro->Resume(); });
        ContextMgr::SetCurrServerContext(nullptr);
        coro->Yield();  // 协程Resume后会到这里来
    }
    ContextMgr::SetCurrServerContext(server_ctx);

    return RPC_SUCCESS;
}

size_t ContextController::PendingContextNum() const
{
    return context_cache_.size();
}

size_t ContextController::PendingCoroutineNum() const
{
    if (CoroMgr::HasCoroutine())
        return CoroMgr::GetInst().GetRunningCoro();
    else
        return 0;
}

}  // namespace app
