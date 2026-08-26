/*
 * * file name: coroutine_interface.h
 * * description: 非对称协程接口，参考ua_server的ICoroutine设计
 * */

#ifndef _APP_COROUTINE_INTERFACE_H_
#define _APP_COROUTINE_INTERFACE_H_

#include <cstdint>
#include <functional>

namespace app
{
/// 协程对象，只提供两个接口
class Coro
{
public:
    /// 唤醒当前协程
    virtual void Resume() = 0;
    /// 切回主协程
    virtual void Yield() = 0;

    virtual ~Coro() = default;
};

/// 协程创建和管理接口
class ICoroutine
{
public:
    /// 设置允许的协程最大数量
    virtual void SetMaxCoroNum(size_t max_num) = 0;
    /// 获取允许的协程最大数量
    virtual size_t GetMaxCoroNum() const = 0;
    /// 获取当前在跑的协程对象数量
    virtual size_t GetRunningCoro() const = 0;
    /// 获取当前所有的协程对象数量
    virtual size_t GetTotalCoro() const = 0;
    /// 启动一个协程，只能在主协程调用，返回true表示有正确启动了协程
    using CoroFunc = void(void*);
    virtual bool Spawn(CoroFunc f, void* args) = 0;
    using CoroTask = std::function<void()>;
    virtual bool Spawn(CoroTask task) = 0;
    /// 如果当前是在协程中，则返回当前协程的Coro指针，不然则返回nullptr
    virtual Coro* ThisCoro() const = 0;

    virtual ~ICoroutine() = default;
};

}  // namespace app

#endif
