/*
 * * file name: app_server.h
 * * description: tapp框架的C++桥接层，提供server运行流程，包括启停的一些控制
 * *              静态成员函数作为tapp回调中转层，通过reinterpret_cast还原对象指针再调用虚函数
 * *              Init()最开始会完成LogService（tlog）初始化，日志写到log_dir目录下，
 * *              之后才进入InitTapp和OnAppInit；相比ua_server的app_server.h，
 * *              去掉了tbus总线地址解析、共享内存配置加载、coredump堆栈处理等专属逻辑，
 * *              只保留驱动tapp主循环所必需的最小闭环
 * */

#ifndef _APP_SERVER_H_
#define _APP_SERVER_H_

#include <cstdint>
#include <string>
#include "tapp/tapp.h"

namespace app
{
/// 这个类可以作为业务server的基类，负责和tapp框架对接
class AppServer
{
public:
    /// svr_id是业务自己分配的服务标识（简化版不走tbus地址体系）
    /// log_dir是日志输出目录（相对路径以进程CWD为基准，默认"log"，目录不存在会自动创建）
    int Init(int argc, char* argv[], uint32_t svr_id, const std::string& log_dir = "log");
    int Run();

    uint32_t MySvrID() const { return svr_id_; }

    virtual ~AppServer() = default;

protected:
    /// 初始化
    virtual bool OnAppInit(uint32_t svr_id) { return true; }
    /// 正式开启proc前会调用这个，一旦设置了ready就开始调用proc，不会调用这个了
    virtual size_t OnAppPrepareProc(uint64_t now_ms, bool stop);
    /// 子类处理自己的子逻辑
    virtual size_t OnAppProc(uint64_t now_ms) { return 0; }
    /// 定时回调
    virtual void OnAppTick(uint64_t now_ms, uint64_t tick_count) {}
    /// 重加载回调
    virtual void OnAppReload() {}
    /// 进程开始真正退出前回调
    virtual bool OnAppFinish() { return true; }
    /// 收到退出通知的时候回调，覆盖此函数可以进行一下自定义的标记设置
    virtual void OnAppQuit() {}
    /// 是否可以退出，默认可以马上退出，覆盖此函数可以控制等待一些状态结束后再退出进程
    virtual bool OnAppIsReadyStop() const { return has_stop_ntf_; }

protected:
    inline size_t TickPerSecond() const { return 1000 / tapp_ctx_.iTimer; }
    inline bool IsProcReady() const { return proc_ready_; }
    inline void SetProcReady(bool ready) { proc_ready_ = ready; }

private:
    int InitTapp(int argc, char* argv[]);

    size_t Proc();
    void Tick();
    void Idle(uint32_t sleep_ms);
    bool Reload();
    bool Finish();
    int Quit();

protected:
    TAPPCTX tapp_ctx_;

private:
    uint32_t svr_id_ = 0;
    uint64_t total_tick_count_ = 0;
    // 是否已经收到stop的通知
    bool has_stop_ntf_ = false;
    // proc是否ready，默认开始执行后会设置成true
    bool proc_ready_ = false;

    // 这些作为类静态函数的唯一好处就是可以任意访问类对象的所有函数和成员
    static int AppArgv(tagTAPPCTX* pstCtx, void* pvArg);
    static int AppProc(tagTAPPCTX* pstCtx, void* pvArg);
    static int AppTick(tagTAPPCTX* pstCtx, void* pvArg);
    static int AppReload(tagTAPPCTX* pstCtx, void* pvArg);
    static int AppIdle(tagTAPPCTX* pstCtx, void* pvArg);
    static int AppQuit(tagTAPPCTX* pstCtx, void* pvArg);
    static int AppStop(tagTAPPCTX* pstCtx, void* pvArg);
    static int AppFini(tagTAPPCTX* pstCtx, void* pvArg);
};

}  // namespace app

#endif
