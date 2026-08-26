/*
 * * file name: coro_mgr.h
 * * description: 提供全局可获取的协程插件指针
 * */

#ifndef _APP_CORO_MGR_H_
#define _APP_CORO_MGR_H_

#include <cassert>
#include "interface/coroutine_interface.h"

namespace app
{
/// 协程管理器（静态类，全局只持有一个ICoroutine*指针，由ServerCore在SvrInit时注入）
class CoroMgr
{
public:
    static void SetCoroutine(ICoroutine* coroutine) { coroutine_ = coroutine; }
    static ICoroutine& GetInst()
    {
        assert(coroutine_);
        return *coroutine_;
    }
    static bool HasCoroutine() { return coroutine_ != nullptr; }

private:
    inline static ICoroutine* coroutine_ = nullptr;
};

}  // namespace app

#endif
