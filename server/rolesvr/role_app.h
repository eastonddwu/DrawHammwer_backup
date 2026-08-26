/*
 * * file name: role_app.h
 * * description: rolesvr的业务server，继承BaseServer获得tapp驱动+ServerCore服务循环，
 * *              只接入tbuspp2一个transport(通过UseDefaultInit注册为default transport，
 * *              由框架SvrProc()自动驱动)，负责注册RPC方法
 * */

#ifndef _ROLE_APP_H_
#define _ROLE_APP_H_

#include <cstdint>
#include <string>
#include "patterns/singleton.h"
#include "svr_base/base_server.h"

namespace rolesvr
{
class RoleApp : public app::BaseServer, public app::Singleton<RoleApp>
{
public:
    /// 设置tbus2 agent地址，必须在Init之前调用
    /// 后端服务(dbproxy)的busid通过tbus2 mesh事件自动发现，无需手动指定
    void Setup(const std::string& tbus2_agent_url);

    /// GroupBase（用于命令行模式下 main.cpp 组合完整busid）
    static constexpr uint32_t kRoleGroupBase = 0x04000000;

protected:
    virtual bool OnInit() override;

private:
    friend class app::Singleton<RoleApp>;
    RoleApp() = default;

    /// tbus2 namesrv保留gid=0(0.0.0.0)为无效组，busid必须落在domain.yaml配置的非零group下。
    /// rolesvr的tbus2 busid = kRoleGroupBase | svr_id（命令行模式），或由--conf-file直接指定。

private:
    std::string tbus2_agent_url_;
};

}  // namespace rolesvr

#endif
