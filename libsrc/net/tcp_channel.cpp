/*
 * * file name: tcp_channel.cpp
 * * description: ...
 * */

#include "tcp_channel.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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

}  // namespace

bool TcpChannel::Init(uint32_t my_id, const std::string& listen_ip, uint16_t listen_port)
{
    my_id_ = my_id;

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0)
    {
        APP_LOG_ERROR(0, "epoll_create1 error(%d)", errno);
        return false;
    }

    if (listen_port != 0)
    {
        listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (listen_fd_ < 0)
        {
            APP_LOG_ERROR(0, "create listen socket error(%d)", errno);
            return false;
        }

        int reuse = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(listen_port);
        addr.sin_addr.s_addr = listen_ip.empty() ? INADDR_ANY : inet_addr(listen_ip.c_str());

        if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            APP_LOG_ERROR(0, "bind(%s:%u) error(%d)", listen_ip.c_str(), listen_port, errno);
            return false;
        }
        if (listen(listen_fd_, 128) != 0)
        {
            APP_LOG_ERROR(0, "listen error(%d)", errno);
            return false;
        }
        if (!SetNonBlocking(listen_fd_))
        {
            APP_LOG_ERROR(0, "set listen_fd non-blocking error(%d)", errno);
            return false;
        }
        if (!AddEpollFd(listen_fd_, EPOLLIN))
            return false;

        APP_LOG_INFO(0, "tcp channel listen on %s:%u, my_id(%u)", listen_ip.c_str(), listen_port, my_id_);
    }

    return true;
}

TcpChannel::~TcpChannel()
{
    for (auto&& item : connections_)
        close(item.first);
    if (listen_fd_ >= 0)
        close(listen_fd_);
    if (epoll_fd_ >= 0)
        close(epoll_fd_);
}

void TcpChannel::AddPeer(uint32_t peer_id, const std::string& ip, uint16_t port)
{
    peer_addrs_[peer_id] = PeerAddr{ip, port};
}

bool TcpChannel::AddEpollFd(int fd, uint32_t events)
{
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        APP_LOG_ERROR(0, "epoll_ctl add fd(%d) error(%d)", fd, errno);
        return false;
    }
    return true;
}

bool TcpChannel::ModEpollFd(int fd, uint32_t events)
{
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) != 0)
    {
        APP_LOG_ERROR(0, "epoll_ctl mod fd(%d) error(%d)", fd, errno);
        return false;
    }
    return true;
}

void TcpChannel::CloseConnection(int fd)
{
    auto iter = connections_.find(fd);
    if (iter != connections_.end())
    {
        if (iter->second.peer_id != 0)
            id_to_fd_.erase(iter->second.peer_id);
        connections_.erase(iter);
    }
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

int TcpChannel::ConnectTo(const PeerAddr& addr)
{
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        APP_LOG_ERROR(0, "create socket error(%d)", errno);
        return -1;
    }
    if (!SetNonBlocking(fd))
    {
        close(fd);
        return -1;
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(addr.port);
    dest.sin_addr.s_addr = inet_addr(addr.ip.c_str());

    int ret = connect(fd, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    bool connecting = false;
    if (ret != 0)
    {
        if (errno == EINPROGRESS)
        {
            connecting = true;
        }
        else
        {
            APP_LOG_ERROR(0, "connect(%s:%u) error(%d)", addr.ip.c_str(), addr.port, errno);
            close(fd);
            return -1;
        }
    }

    if (!AddEpollFd(fd, EPOLLIN | EPOLLOUT))
    {
        close(fd);
        return -1;
    }

    Connection conn;
    conn.fd = fd;
    conn.connecting = connecting;
    connections_[fd] = std::move(conn);

    return fd;
}

int32_t TcpChannel::TrySend(int fd, const char* data, size_t len)
{
    auto iter = connections_.find(fd);
    if (iter == connections_.end())
        return -1;

    Connection& conn = iter->second;
    if (conn.connecting || !conn.send_buf.empty())
    {
        // 还没连上，或者有排队数据没发完，先缓存，保持顺序
        conn.send_buf.append(data, len);
        return 0;
    }

    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n > 0)
        {
            sent += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        if (n < 0 && errno == EINTR)
            continue;

        APP_LOG_ERROR(0, "send fd(%d) error(%d)", fd, errno);
        return -1;
    }

    if (sent < len)
    {
        conn.send_buf.append(data + sent, len - sent);
        ModEpollFd(fd, EPOLLIN | EPOLLOUT);
    }

    return 0;
}

int32_t TcpChannel::Send(uint32_t dest_id, const char* buff, size_t buff_len)
{
    auto id_iter = id_to_fd_.find(dest_id);
    if (id_iter != id_to_fd_.end())
        return TrySend(id_iter->second, buff, buff_len);

    auto addr_iter = peer_addrs_.find(dest_id);
    if (addr_iter == peer_addrs_.end())
    {
        APP_LOG_ERROR(0, "dest_id(%u) no connection and no known address", dest_id);
        return -1;
    }

    int fd = ConnectTo(addr_iter->second);
    if (fd < 0)
        return -1;

    connections_[fd].peer_id = dest_id;
    id_to_fd_[dest_id] = fd;

    return TrySend(fd, buff, buff_len);
}

void TcpChannel::HandleAccept()
{
    while (true)
    {
        sockaddr_in peer_addr{};
        socklen_t addr_len = sizeof(peer_addr);
        int fd = accept4(listen_fd_, reinterpret_cast<sockaddr*>(&peer_addr), &addr_len, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (fd < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                APP_LOG_WARN(0, "accept error(%d)", errno);
            break;
        }

        if (!SetNonBlocking(fd))
        {
            close(fd);
            continue;
        }
        if (!AddEpollFd(fd, EPOLLIN))
        {
            close(fd);
            continue;
        }

        Connection conn;
        conn.fd = fd;
        connections_[fd] = std::move(conn);
        APP_LOG_TRACE(0, "accept new connection fd(%d) from %s:%u", fd, inet_ntoa(peer_addr.sin_addr),
                      ntohs(peer_addr.sin_port));
    }
}

size_t TcpChannel::HandleReadable(int fd, uint32_t max_recv_count)
{
    auto iter = connections_.find(fd);
    if (iter == connections_.end())
        return 0;
    Connection& conn = iter->second;

    char buf[8192];
    while (true)
    {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0)
        {
            conn.recv_buf.append(buf, static_cast<size_t>(n));
            continue;
        }
        if (n == 0)
        {
            APP_LOG_TRACE(0, "peer close connection fd(%d)", fd);
            CloseConnection(fd);
            return 0;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
        if (errno == EINTR)
            continue;

        APP_LOG_WARN(0, "recv fd(%d) error(%d)", fd, errno);
        CloseConnection(fd);
        return 0;
    }

    size_t pkg_count = 0;
    size_t offset = 0;
    while (pkg_count < max_recv_count)
    {
        int64_t pkg_len = TryGetFrameLen(conn.recv_buf.data() + offset, conn.recv_buf.size() - offset);
        if (pkg_len < 0)
        {
            APP_LOG_ERROR(0, "bad pkg on fd(%d), close connection", fd);
            CloseConnection(fd);
            return pkg_count;
        }
        if (pkg_len == 0)
            break;

        const char* pkg_data = conn.recv_buf.data() + offset;
        const FramePrefix* prefix = reinterpret_cast<const FramePrefix*>(pkg_data);

        // 从收到的第一个包中学习对端id，方便后续回包寻址
        if (conn.peer_id == 0)
        {
            app::protocol::PkgHead head;
            if (head.ParseFromArray(pkg_data + sizeof(FramePrefix), prefix->head_len) && head.src() != 0)
            {
                conn.peer_id = head.src();
                id_to_fd_[head.src()] = fd;
            }
        }

        uint32_t recv_id = conn.peer_id != 0 ? conn.peer_id : static_cast<uint32_t>(fd);
        if (recv_callback_)
            recv_callback_(pkg_data, static_cast<size_t>(pkg_len), recv_id, 0);

        offset += static_cast<size_t>(pkg_len);
        ++pkg_count;
    }

    if (offset > 0)
        conn.recv_buf.erase(0, offset);

    return pkg_count;
}

void TcpChannel::HandleWritable(int fd)
{
    auto iter = connections_.find(fd);
    if (iter == connections_.end())
        return;
    Connection& conn = iter->second;

    if (conn.connecting)
    {
        int err = 0;
        socklen_t err_len = sizeof(err);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len) != 0 || err != 0)
        {
            APP_LOG_ERROR(0, "connect fd(%d) failed, err(%d)", fd, err);
            CloseConnection(fd);
            return;
        }
        conn.connecting = false;
        APP_LOG_TRACE(0, "connect fd(%d) established", fd);
    }

    if (!conn.send_buf.empty())
    {
        size_t sent = 0;
        size_t len = conn.send_buf.size();
        while (sent < len)
        {
            ssize_t n = send(fd, conn.send_buf.data() + sent, len - sent, MSG_NOSIGNAL);
            if (n > 0)
            {
                sent += static_cast<size_t>(n);
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                break;
            if (n < 0 && errno == EINTR)
                continue;

            APP_LOG_ERROR(0, "send fd(%d) error(%d)", fd, errno);
            CloseConnection(fd);
            return;
        }
        conn.send_buf.erase(0, sent);
    }

    if (conn.send_buf.empty())
        ModEpollFd(fd, EPOLLIN);
}

size_t TcpChannel::Loop(uint32_t max_recv_count)
{
    constexpr int MAX_EVENTS = 64;
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
        int fd = events[i].data.fd;
        if (fd == listen_fd_)
        {
            HandleAccept();
            continue;
        }

        if (events[i].events & (EPOLLERR | EPOLLHUP))
        {
            CloseConnection(fd);
            continue;
        }

        if (events[i].events & EPOLLOUT)
            HandleWritable(fd);

        if (events[i].events & EPOLLIN)
        {
            if (total_count < max_recv_count)
                total_count += HandleReadable(fd, max_recv_count - static_cast<uint32_t>(total_count));
        }
    }

    return total_count;
}

}  // namespace app
