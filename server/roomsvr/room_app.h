/*
 * * file name: room_app.h
 * * description: roomsvr的业务server，继承BaseServer获得tapp驱动+ServerCore服务循环，
 * *              只接入tbuspp2一个transport(通过UseDefaultInit注册为default transport，
 * *              由框架SvrProc()自动驱动)，负责注册RPC方法，管理房间生命周期
 */

#ifndef _ROOM_APP_H_
#define _ROOM_APP_H_

#include <cstdint>
#include <string>
#include "patterns/singleton.h"
#include "svr_base/base_server.h"

namespace roomsvr
{
class RoomApp : public app::BaseServer, public app::Singleton<RoomApp>
{
public:
    /// 设置tbus2 agent地址，必须在Init之前调用
    void Setup(const std::string& tbus2_agent_url);

    /// GroupBase（用于命令行模式下 main.cpp 组合完整busid）
    static constexpr uint32_t kRoomGroupBase = 0x06000000;

protected:
    virtual bool OnInit() override;
    virtual size_t OnProc(uint64_t now_ms, bool stop) override;
    virtual void OnTick(uint64_t now_ms, uint64_t tick_count) override;

private:
    friend class app::Singleton<RoomApp>;
    RoomApp() = default;

    std::string tbus2_agent_url_;
};

}  // namespace roomsvr

#endif
