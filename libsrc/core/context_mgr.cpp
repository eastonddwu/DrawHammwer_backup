/*
 * * file name: context_mgr.cpp
 * * description: ...
 * */

#include "context_mgr.h"
#include "context.h"

namespace app
{
thread_local ServerContext* ContextMgr::curr_context_ = nullptr;

void ContextMgr::SetCurrServerContext(ServerContext* ctx)
{
    curr_context_ = ctx;
}

uint64_t ContextMgr::GetContextId()
{
    auto* ctx = GetCurrServerContext();
    if (ctx)
        return ctx->gid;
    return 0;
}

}  // namespace app
