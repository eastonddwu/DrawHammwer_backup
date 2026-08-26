/*
 * * file name: context_mgr.h
 * * description: 管理当前线程正在执行的ServerContext（协程私有，用thread_local隔离）
 * */

#ifndef _APP_CONTEXT_MGR_H_
#define _APP_CONTEXT_MGR_H_

#include <cstdint>

namespace app
{
struct ServerContext;

/// 当前线程正在执行的事务ServerContext
class ContextMgr
{
public:
    static ServerContext* GetCurrServerContext() { return curr_context_; }
    static void SetCurrServerContext(ServerContext* ctx);
    static bool IsNull() { return curr_context_ == nullptr; }
    static uint64_t GetContextId();

private:
    static thread_local ServerContext* curr_context_;
};

}  // namespace app

#endif
