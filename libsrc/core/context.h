/*
 * * file name: context.h
 * * description: 上下文对象定义，是Pending/Awake协程挂起唤醒机制的基石
 * */

#ifndef _APP_CONTEXT_H_
#define _APP_CONTEXT_H_

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include "context_mgr.h"

namespace app
{
/// 上下文对象，异步模式下可以实现Context的子类来带上更多自己的信息
struct Context
{
    /// 自定义回调处理函数
    using Callback = std::function<void(int32_t)>;
    /// 自定义回调回收函数（也可以不需要）
    using RecycleFun = std::function<void()>;

    Context() : id(++auto_counter_) {}

    void SetCallback(const Callback& cb, const RecycleFun& fun = nullptr)
    {
        callback = cb;
        recycle = fun;
    }
    void SetCallback(Callback&& cb, RecycleFun&& fun = nullptr)
    {
        callback = std::move(cb);
        recycle = std::move(fun);
    }

    void Run()
    {
        assert(callback);
        callback(ret_code);
        if (recycle)
            recycle();
    }

    /// 自增唯一ID，在一些场景下可能需要通过这个区分是哪个context
    const uint32_t id = 0;
    int32_t ret_code = 0;

protected:
    Callback callback = nullptr;
    RecycleFun recycle = nullptr;

private:
    inline static uint32_t auto_counter_ = 0;
};

/// 服务端上下文对象，被调处理的时候创建
struct ServerContext : public Context
{
    /// 处理耗时（毫秒）。start_time/end_time存的是微秒实时值（见rpc_service.cpp的赋值处），
    /// 不用Clock缓存值是因为Clock每帧只更新一次且精度为毫秒，同帧内完成的请求会算出0耗时。
    uint32_t Duration() const { return static_cast<uint32_t>((end_time - start_time) / 1000); }
    /// 处理耗时（微秒）
    uint64_t DurationMicro() const { return end_time > start_time ? end_time - start_time : 0; }
    virtual ~ServerContext() = default;

    /// 请求开始处理的时间（微秒，实时值）
    uint64_t start_time = 0;
    /// 请求处理完成的时间（微秒，实时值）
    uint64_t end_time = 0;
    uint64_t gid = 0;
    uint16_t pkg_flag = 0;
};

/// 客户端上下文对象，主调发起的时候创建
struct ClientContext : public Context
{
    ClientContext() { server_ctx = ContextMgr::GetCurrServerContext(); }
    virtual ~ClientContext() = default;

    uint32_t timer_id = 0;
    ServerContext* server_ctx = nullptr;
};

/// 异步调用过程中执行的任务对象，包含了回调，回收和是否堵塞三个功能
struct AsyncTask
{
    /// rpc回调函数的类型
    using RpcCallback = std::function<void(int32_t, ServerContext*)>;
    using BlockingCallBack = std::function<void()>;
    using RecycleCallBack = Context::RecycleFun;

    AsyncTask(std::nullptr_t) {}
    template <class Callable, class Recycle = RecycleCallBack, class Blocking = BlockingCallBack>
    AsyncTask(const Callable& cb, const Recycle& recycle = nullptr, const Blocking& blocking = nullptr)
        : callback(cb), recycle_fun(recycle), blocking_fun(blocking)
    {
    }

    RpcCallback callback = nullptr;
    RecycleCallBack recycle_fun = nullptr;
    BlockingCallBack blocking_fun = nullptr;
};

}  // namespace app

#endif
