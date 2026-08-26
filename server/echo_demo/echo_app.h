/*
 * * file name: echo_app.h
 * * description: echo_demo的业务server，继承BaseServer获得tapp驱动+ServerCore服务循环，
 * *              负责初始化TCP Channel、通过UseDefaultInit注册TBus2Channel(default transport)、
 * *              注册RPC方法
 * */

#ifndef _ECHO_APP_H_
#define _ECHO_APP_H_

#include <cstdint>
#include <string>
#include "net/pb_codec.h"
#include "net/tcp_channel.h"
#include "patterns/singleton.h"
#include "svr_base/base_server.h"

namespace echo_demo
{
class EchoApp : public app::BaseServer, public app::Singleton<EchoApp>
{
public:
    /// 设置监听端口和对端地址，必须在Init之前调用（本机id用AppServer::Init传入的svr_id）
    /// tbus2_agent_url为空则不启用tbus2 transport(仍走已有TCP流程)，否则尝试对接本地tbus2 agent
    void Setup(uint16_t listen_port, uint32_t peer_id, const std::string& peer_ip, uint16_t peer_port,
               const std::string& tbus2_agent_url = "");

    /// 供rpc handler查询要主叫的对端id（TCP transport用，即对端svr_id）
    uint32_t PeerID() const { return peer_id_; }

    /// 供rpc handler查询要主叫的对端tbus2 busid（TBUS2 transport用，见tbus2_busid_注释）
    uint32_t PeerTBus2BusID() const { return kTBus2GroupBase | peer_id_; }

protected:
    virtual bool OnInit() override;
    /// 手动驱动TCP channel（TCP不是default transport，参照ConnSvr模式：
    /// tbus2是内部主链路走框架自动驱动，TCP是对外业务连接自己在这里收）
    virtual size_t OnProc(uint64_t now_ms, bool stop) override;

private:
    friend class app::Singleton<EchoApp>;
    EchoApp() = default;

    /// tbus2 namesrv保留gid=0(0.0.0.0)为无效组(handshake返回NS_ERR_WRONG_GROUP_ID)，
    /// 普通busid必须落在domain.yaml配置的非零group下，这里复用已有的"1.0.0.0"(stateless group)，
    /// echo_demo A/B的tbus2 busid = kTBus2GroupBase | svr_id，即1.0.0.1/1.0.0.2
    static constexpr uint32_t kTBus2GroupBase = 0x01000000;

private:
    uint16_t listen_port_ = 0;
    uint32_t peer_id_ = 0;      ///< 对端svr_id，用作channel_.AddPeer的路由key，也是EchoCallPeer里Rpc()调用的dst id（PeerID()对外暴露）
    std::string peer_ip_;       ///< 对端监听IP，OnInit时传给channel_.AddPeer，用于建立到对端的TCP连接
    uint16_t peer_port_ = 0;
    std::string tbus2_agent_url_;

    app::TcpChannel channel_;
    app::PbRecvCodec recv_codec_;
    app::PbSendCodec send_codec_;
};

}  // namespace echo_demo

#endif
