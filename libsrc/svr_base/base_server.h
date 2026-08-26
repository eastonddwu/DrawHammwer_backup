/*
 * * file name: base_server.h
 * * description: AppServer(tapp桥接层) + ServerCore(服务驱动核心) 的多继承桥接层，
 * *              业务server通常直接继承这个类。相比ua_server的base_server.h，
 * *              去掉了FrameworkConfMap配置文件加载和共享内存日志初始化，
 * *              SvrInitImpl改为直接接受参数构造SvrOption
 * */

#ifndef _APP_BASE_SERVER_H_
#define _APP_BASE_SERVER_H_

#include "core/server_core.h"
#include "svr_base/app_server.h"

namespace app
{
class BaseServer : public AppServer, public ServerCore
{
protected:
    /// 初始化，转发给SvrInitImpl
    virtual bool OnAppInit(uint32_t svr_id) override { return SvrInitImpl(); }
    /// 子类处理自己的子逻辑，转发给ServerCore::SvrProc
    virtual size_t OnAppProc(uint64_t now_ms) override { return ServerCore::SvrProc(now_ms); }
    /// 定时回调，转发给ServerCore::SvrTick
    virtual void OnAppTick(uint64_t now_ms, uint64_t tick_count) override { ServerCore::SvrTick(now_ms, tick_count); }
    /// 退出前回调，转发给ServerCore::SvrFinish
    virtual bool OnAppFinish() override { return ServerCore::SvrFinish(); }
    /// 收到退出通知的时候回调，转发给ServerCore::SvrNtfQuit
    virtual void OnAppQuit() override { ServerCore::SvrNtfQuit(); }
    /// 是否可以退出了，合并AppServer和ServerCore各自的判断
    virtual bool OnAppIsReadyStop() const override
    {
        return AppServer::OnAppIsReadyStop() && ServerCore::SvrStopReady();
    }

protected:
    /// 简化版初始化：不依赖配置文件，直接用参数构造SvrOption，框架只支持协程模式
    /// max_coro_num/max_deal_pkg_num为0时表示使用SvrOption的默认值
    bool SvrInitImpl(uint32_t max_coro_num = 0, uint32_t max_deal_pkg_num = 0);
};

}  // namespace app

#endif
