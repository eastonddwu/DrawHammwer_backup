/*
 * * file name: context_controller.h
 * * description: 提供协程挂起/唤醒的上下文管理功能
 * */

#ifndef _APP_CONTEXT_CONTROLLER_H_
#define _APP_CONTEXT_CONTROLLER_H_

#include <unordered_map>
#include "context.h"
#include "timeout_queue.h"

namespace app
{
class ContextController
{
public:
    bool Init();
    /// 处理定时器，传入当前时间
    uint32_t ProcTimeOut(uint64_t now);
    /// 挂起当前协程
    int32_t Pending(uint64_t seq_id, uint32_t timeout, ClientContext* client_ctx, const AsyncTask& task);
    /// 唤醒当前协程
    ClientContext* Awake(uint64_t seq_id, int32_t ret_code);
    /// 当前挂起的上下文数量
    size_t PendingContextNum() const;
    /// 当前存在的协程数
    size_t PendingCoroutineNum() const;

private:
    /// 超时队列
    TimeoutQueue timeout_queue_;
    /// 挂起的上下文cache
    std::unordered_map<uint64_t, ClientContext*> context_cache_;
    /// 是否初始化的标记
    bool init_ = false;
};

}  // namespace app

#endif
