/*
 * * file name: server_core.h
 * * description: 服务驱动核心（简化版，对齐ua_server的ServerCore，去掉SystemMgr模块管理/
 * *              TimeoutDecorator定时事件/ServiceMesh/调度器等扩展能力，只保留跑通
 * *              tapp驱动+协程+异步RPC最小闭环所需的部分）
 * */

#ifndef _APP_SERVER_CORE_H_
#define _APP_SERVER_CORE_H_

#include <cstdint>
#include "context_controller.h"
#include "rpc_service.h"

namespace app
{
class ICoroutine;

class ServerCore
{
public:
    /// 可选的一些配置参数
    struct SvrOption
    {
        // 协程插件，框架只支持协程模式运行，必须设置（一般用CoroutineMgr::GetInst()）
        ICoroutine* coroutine = nullptr;
        // 单个服务最大协程数
        uint32_t max_coro_num = 10000;
        // 单次proc最多处理的收包个数
        uint32_t max_deal_pkg_num = 128;
    };

    /// 服务初始化
    bool SvrInit(const SvrOption& option);
    /// 服务tick调用
    void SvrTick(uint64_t now_ms, uint64_t tick_count);
    /// 服务主循环，四阶段：处理超时上下文->处理定时事件(简化省略)->业务OnProc->驱动收包
    size_t SvrProc(uint64_t now_ms);
    /// 服务退出前调用
    bool SvrFinish();
    /// 通知服务准备退出了
    void SvrNtfQuit();
    /// 检查服务当前是否可以退出
    bool SvrStopReady() const;
    /// 是否收到停止通知，正在停止
    bool IsStoping() const { return stop_; }

    /// 添加一个transport，并且可以指定为默认的
    bool AddTransportInfo(uint32_t transport_type, const TransportInfo& info, bool is_default = false);
    /// 获取默认transport
    const TransportInfo* DefaultTransportInfo() const;
    /// 按transport_type查找已注册的transport，未注册返回nullptr
    const TransportInfo* FindTransportInfo(uint32_t transport_type) const;

    virtual ~ServerCore() = default;

protected:
    virtual bool OnInit() { return true; }
    virtual void OnTick(uint64_t now_ms, uint64_t tick_count) {}
    virtual size_t OnProc(uint64_t now_ms, bool stop) { return 0; }
    virtual bool OnFinish() { return true; }

protected:
    // 是否收到停止通知，准备停止了
    bool stop_ = false;
    // 默认的transport下标，初始为越界哨兵值(MAX_TRANSPORT_NUM)，
    // 避免在没有任何transport显式设置is_default=true之前，被误认为下标0是default
    uint32_t default_transport_ = RpcService::MAX_TRANSPORT_NUM;
    // 上下文管理控制器
    ContextController context_ctrl_;
    // svr配置参数
    SvrOption option_;
};

}  // namespace app

#endif
