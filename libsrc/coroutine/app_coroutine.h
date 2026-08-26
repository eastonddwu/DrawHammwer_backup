/*
 * * file name: app_coroutine.h
 * * description: 基于libco的协程实现，对齐ua_server的LibcoCoroMgr设计
 * */

#ifndef _APP_COROUTINE_H_
#define _APP_COROUTINE_H_

#include <cstdint>
#include <functional>
#include "coctx.h"
#include "core/interface/coroutine_interface.h"
#include "patterns/singleton.h"

namespace app
{
class CoroImpl;
class CoroutineMgr : public ICoroutine, public Singleton<CoroutineMgr>
{
public:
    /// 设置允许的协程最大数量
    virtual void SetMaxCoroNum(size_t max_num) override final;
    /// 获取允许的协程最大数量
    virtual size_t GetMaxCoroNum() const override final;
    /// 获取当前在跑的协程对象数量
    virtual size_t GetRunningCoro() const override final;
    /// 获取当前所有的协程对象数量
    virtual size_t GetTotalCoro() const override final;
    /// 启动一个协程，只能在主协程调用，返回true表示有正确启动了协程
    using CoroFunc = void(void*);
    virtual bool Spawn(CoroFunc f, void* args) override final;
    using CoroTask = std::function<void()>;
    virtual bool Spawn(CoroTask task) override final;
    /// 如果当前是在协程中，则返回当前协程的Coro指针，不然则返回nullptr
    virtual Coro* ThisCoro() const override final;

    /// 设置协程栈大小
    void SetStackSize(size_t size);
    /// 获取协程栈大小
    size_t GetStackSize() const;
    /// 设置是否进行内存段保护，会增加额外内存
    void SetMemProtect(bool protect);

private:
    friend class Singleton<CoroutineMgr>;
    CoroutineMgr();
    ~CoroutineMgr();
    /// 申请一个协程对象
    CoroImpl* Allocate();
    /// 回收一个协程对象
    void Free(CoroImpl* coro);
    /// 协程逻辑函数
    static void* RunLoop(void* arg1, void* arg2);

private:
    size_t stack_size_ = 64 * 1024;
    size_t max_coro_num_ = 10000;
    bool need_protect_ = true;
};

}  // namespace app

#endif
