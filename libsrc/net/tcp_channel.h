/*
 * * file name: tcp_channel.h
 * * description: 基于epoll的非阻塞TCP Channel，实现IChannel接口
 * *              提供id->ip:port的静态地址表，Send(dest_id)时按需建立连接
 * *              收到的包按目标ID寻址回包时，通过收包时从包头学习到的src id反向绑定fd
 * */

#ifndef _APP_TCP_CHANNEL_H_
#define _APP_TCP_CHANNEL_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include "core/interface/channel_interface.h"

namespace app
{
class TcpChannel : public IChannel
{
public:
    /// 初始化，my_id是自己的id，listen_ip/listen_port为空/0表示不监听（纯客户端角色）
    bool Init(uint32_t my_id, const std::string& listen_ip, uint16_t listen_port);
    /// 注册一个对端地址，Send(dest_id)时如果还没连接会按这个地址主动连接
    void AddPeer(uint32_t peer_id, const std::string& ip, uint16_t port);

    virtual uint32_t MyID() const override { return my_id_; }
    virtual int32_t Send(uint32_t dest_id, const char* buff, size_t buff_len) override;
    virtual size_t Loop(uint32_t max_recv_count) override;

    virtual ~TcpChannel() override;

private:
    struct PeerAddr
    {
        std::string ip;
        uint16_t port = 0;
    };

    struct Connection
    {
        int fd = -1;
        uint32_t peer_id = 0;  // 0表示还未从收到的包中学习到对端id
        std::string recv_buf;
        std::string send_buf;
        bool connecting = false;  // 非阻塞connect还未完成
    };

    /// 主动连接一个地址，成功发起（不代表已连上）返回true
    int ConnectTo(const PeerAddr& addr);
    /// 把fd加入epoll关注读事件（以及可选写事件）
    bool AddEpollFd(int fd, uint32_t events);
    bool ModEpollFd(int fd, uint32_t events);
    void CloseConnection(int fd);
    /// 处理监听socket上的新连接
    void HandleAccept();
    /// 处理某个fd上的可读事件，解析出完整包后回调，返回本次处理的包数
    size_t HandleReadable(int fd, uint32_t max_recv_count);
    /// 处理某个fd上的可写事件，把send_buf里剩余数据发出去
    void HandleWritable(int fd);
    /// 尝试立即发送，发不完的数据缓存到send_buf，并关注EPOLLOUT
    int32_t TrySend(int fd, const char* data, size_t len);

private:
    uint32_t my_id_ = 0;
    int listen_fd_ = -1;
    int epoll_fd_ = -1;

    std::unordered_map<uint32_t, PeerAddr> peer_addrs_;
    std::unordered_map<uint32_t, int> id_to_fd_;
    std::unordered_map<int, Connection> connections_;
};

}  // namespace app

#endif
