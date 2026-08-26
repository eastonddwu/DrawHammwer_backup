/*
 * * file name: dsa_app.h
 * * description: dsagent的业务server，管理DS进程生命周期。
 * *              双transport：tbus2(对内，default) + TCP(对DS进程，TRANSPORT_DS_TCP)
 */

#ifndef _DSA_APP_H_
#define _DSA_APP_H_

#include <cstdint>
#include <string>
#include "net/pb_codec.h"
#include "net/tcp_channel.h"
#include "patterns/singleton.h"
#include "svr_base/base_server.h"

namespace dsagent
{
class DsaApp : public app::BaseServer, public app::Singleton<DsaApp>
{
public:
    void Setup(const std::string& tbus2_agent_url, uint16_t ds_listen_port,
               uint16_t ds_port_start, uint16_t ds_port_end,
               const std::string& ds_client_ip = "127.0.0.1",
               const std::string& dsa_host = "",
               const std::string& ds_exec_path = "DrawHammer_DS/DrawHammer/Binaries/Linux/DrawHammerServer",
               const std::string& ds_type = "ue_ds",
               const std::string& ds_map = "/Game/DrawHammer/Map/LivingRoom/Lvl_LivingRoom_BattleMap");

    static constexpr uint32_t kDsaGroupBase = 0x08000000;

    app::TcpChannel& GetDsTcpChannel() { return ds_tcp_channel_; }

    uint16_t ds_port_start() const { return ds_port_start_; }
    uint16_t ds_port_end() const { return ds_port_end_; }
    uint16_t ds_listen_port() const { return ds_listen_port_; }
    const std::string& ds_client_ip() const { return ds_client_ip_; }
    const std::string& dsa_host() const { return dsa_host_; }  // DS进程连接dsagent用的IP（注入-DsaHost）
    const std::string& ds_exec_path() const { return ds_exec_path_; }
    const std::string& ds_type() const { return ds_type_; }  // "ue_ds"
    const std::string& ds_map() const { return ds_map_; }     // UE DS map参数

protected:
    virtual bool OnInit() override;
    virtual size_t OnProc(uint64_t now_ms, bool stop) override;
    virtual void OnTick(uint64_t now_ms, uint64_t tick_count) override;

private:
    friend class app::Singleton<DsaApp>;
    DsaApp() = default;

    std::string tbus2_agent_url_;
    uint16_t ds_listen_port_ = 19000;
    uint16_t ds_port_start_ = 20000;
    uint16_t ds_port_end_ = 20099;
    std::string ds_client_ip_ = "127.0.0.1";  // 客户端连接DS时使用的外部IP
    std::string dsa_host_;                     // DS进程连接dsagent的IP，空=自动获取本机公网IP
    std::string ds_exec_path_ = "DrawHammer_DS/DrawHammer/Binaries/Linux/DrawHammerServer";  // UE DS二进制路径（直接execl，不经过shell wrapper），真实值始终来自生成的dsagent_conf.json，此处仅为未传参时的相对路径兜底
    std::string ds_type_ = "ue_ds";                     // DS类型（已移除mock_ds支持）
    std::string ds_map_ = "/Game/DrawHammer/Map/LivingRoom/Lvl_LivingRoom_BattleMap";  // UE DS启动时指定的地图路径
    uint64_t last_report_ms_ = 0;

    app::TcpChannel ds_tcp_channel_;
    app::PbRecvCodec ds_recv_codec_;
    app::PbSendCodec ds_send_codec_;
};

}  // namespace dsagent

#endif
