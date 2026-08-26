/*
 * * file name: udp_channel.h
 * * description: 基于epoll的非阻塞UDP Channel，实现IChannel接口
 * *              提供id->ip:port的静态地址表，Send(dest_id)时优先用学习到的地址，否则用静态表地址
 * *              UDP是无连接、面向数据报的，一个datagram即一个完整帧，不需要TCP那样的分包/粘包处理
 * *              收包时从包头学习到的src id反向绑定对端sockaddr，方便后续回包寻址
 * */

#ifndef _APP_UDP_CHANNEL_H_
#define _APP_UDP_CHANNEL_H_

#include <netinet/in.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include "core/interface/channel_interface.h"

namespace app
{
class UdpChannel : public IChannel
{
public:
    /// 初始化，my_id是自己的id，listen_ip/listen_port为空/0表示不绑定端口（纯客户端角色，仅能主动发送）
    bool Init(uint32_t my_id, const std::string& listen_ip, uint16_t listen_port);
    /// 注册一个对端静态地址，Send(dest_id)时如果还没学习到对端地址会用这个地址
    void AddPeer(uint32_t peer_id, const std::string& ip, uint16_t port);

    virtual uint32_t MyID() const override { return my_id_; }
    virtual int32_t Send(uint32_t dest_id, const char* buff, size_t buff_len) override;
    virtual size_t Loop(uint32_t max_recv_count) override;

    virtual ~UdpChannel() override;

private:
    /// 处理socket上的可读事件，逐个datagram解析出完整帧后回调，最多处理max_recv_count个，返回实际处理的个数
    size_t HandleReadable(uint32_t max_recv_count);

private:
    uint32_t my_id_ = 0;
    int sock_fd_ = -1;
    int epoll_fd_ = -1;

    std::unordered_map<uint32_t, sockaddr_in> peer_addrs_;   // 静态配置的对端地址
    std::unordered_map<uint32_t, sockaddr_in> learned_addrs_;  // 收包时学习到的对端实际地址，优先使用
};

}  // namespace app

#endif
