/*
 * * file name: udp_channel.cpp
 * * description: UdpChannel实现，见udp_channel.h
 * */

#include "udp_channel.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include "core/log.h"
#include "pkg_framing.h"
#include "pkg_head.pb.h"

namespace app
{
namespace
{
bool SetNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

sockaddr_in MakeAddr(const std::string& ip, uint16_t port)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ip.empty() ? INADDR_ANY : inet_addr(ip.c_str());
    return addr;
}

}  // namespace

bool UdpChannel::Init(uint32_t my_id, const std::string& listen_ip, uint16_t listen_port)
{
    my_id_ = my_id;

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0)
    {
        APP_LOG_ERROR(0, "epoll_create1 error(%d)", errno);
        return false;
    }

    sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0)
    {
        APP_LOG_ERROR(0, "create udp socket error(%d)", errno);
        return false;
    }

    int reuse = 1;
    setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (listen_port != 0)
    {
        sockaddr_in addr = MakeAddr(listen_ip, listen_port);
        if (bind(sock_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            APP_LOG_ERROR(0, "bind(%s:%u) error(%d)", listen_ip.c_str(), listen_port, errno);
            close(sock_fd_);
            sock_fd_ = -1;
            return false;
        }
        APP_LOG_INFO(0, "udp channel listen on %s:%u, my_id(%u)", listen_ip.c_str(), listen_port, my_id_);
    }

    if (!SetNonBlocking(sock_fd_))
    {
        APP_LOG_ERROR(0, "set udp socket non-blocking error(%d)", errno);
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = sock_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sock_fd_, &ev) != 0)
    {
        APP_LOG_ERROR(0, "epoll_ctl add udp socket error(%d)", errno);
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    return true;
}

UdpChannel::~UdpChannel()
{
    if (sock_fd_ >= 0)
        close(sock_fd_);
    if (epoll_fd_ >= 0)
        close(epoll_fd_);
}

void UdpChannel::AddPeer(uint32_t peer_id, const std::string& ip, uint16_t port)
{
    peer_addrs_[peer_id] = MakeAddr(ip, port);
}

int32_t UdpChannel::Send(uint32_t dest_id, const char* buff, size_t buff_len)
{
    if (sock_fd_ < 0)
        return -1;

    const sockaddr_in* dest_addr = nullptr;
    auto learned_iter = learned_addrs_.find(dest_id);
    if (learned_iter != learned_addrs_.end())
    {
        dest_addr = &learned_iter->second;
    }
    else
    {
        auto peer_iter = peer_addrs_.find(dest_id);
        if (peer_iter != peer_addrs_.end())
            dest_addr = &peer_iter->second;
    }

    if (dest_addr == nullptr)
    {
        APP_LOG_ERROR(0, "dest_id(%u) no known address", dest_id);
        return -1;
    }

    ssize_t n = sendto(sock_fd_, buff, buff_len, 0, reinterpret_cast<const sockaddr*>(dest_addr), sizeof(*dest_addr));
    if (n < 0)
    {
        // UDP是无连接的，发送失败(包括EAGAIN)直接丢弃，不做缓存重试
        APP_LOG_WARN(0, "sendto dest_id(%u) error(%d)", dest_id, errno);
        return -1;
    }
    if (static_cast<size_t>(n) != buff_len)
    {
        APP_LOG_WARN(0, "sendto dest_id(%u) partial send(%zd/%zu)", dest_id, n, buff_len);
        return -1;
    }

    return 0;
}

size_t UdpChannel::HandleReadable(uint32_t max_recv_count)
{
    size_t pkg_count = 0;
    char buf[65536];

    while (pkg_count < max_recv_count)
    {
        sockaddr_in from_addr{};
        socklen_t addr_len = sizeof(from_addr);
        ssize_t n = recvfrom(sock_fd_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from_addr), &addr_len);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EINTR)
                continue;
            APP_LOG_WARN(0, "recvfrom error(%d)", errno);
            break;
        }
        if (n == 0)
            continue;

        // UDP每个datagram即一个完整帧，不需要像TCP那样处理分包/粘包
        int64_t pkg_len = TryGetFrameLen(buf, static_cast<size_t>(n));
        if (pkg_len <= 0 || static_cast<size_t>(pkg_len) != static_cast<size_t>(n))
        {
            APP_LOG_WARN(0, "bad udp pkg from %s:%u, len(%zd)", inet_ntoa(from_addr.sin_addr),
                         ntohs(from_addr.sin_port), n);
            continue;
        }

        const FramePrefix* prefix = reinterpret_cast<const FramePrefix*>(buf);
        uint32_t recv_id = 0;

        app::protocol::PkgHead head;
        if (head.ParseFromArray(buf + sizeof(FramePrefix), prefix->head_len) && head.src() != 0)
        {
            recv_id = head.src();
            learned_addrs_[recv_id] = from_addr;  // 学习/更新对端实际地址，用于回包寻址
        }

        if (recv_callback_)
            recv_callback_(buf, static_cast<size_t>(pkg_len), recv_id, 0);

        ++pkg_count;
    }

    return pkg_count;
}

size_t UdpChannel::Loop(uint32_t max_recv_count)
{
    constexpr int MAX_EVENTS = 4;
    epoll_event events[MAX_EVENTS];

    int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, 0);
    if (n < 0)
    {
        if (errno != EINTR)
            APP_LOG_WARN(0, "epoll_wait error(%d)", errno);
        return 0;
    }

    size_t total_count = 0;
    for (int i = 0; i < n; ++i)
    {
        if (events[i].events & EPOLLIN)
            total_count += HandleReadable(max_recv_count);
    }

    return total_count;
}

}  // namespace app
