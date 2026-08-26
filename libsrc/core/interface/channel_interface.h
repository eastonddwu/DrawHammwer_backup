/*
 * * file name: channel_interface.h
 * * description: 收发包通道抽象接口，参考ua_server的IChannel设计
 * */

#ifndef _APP_CHANNEL_INTERFACE_H_
#define _APP_CHANNEL_INTERFACE_H_

#include <cstdint>
#include <functional>

namespace app
{
class IChannel
{
public:
    /// 参数分别是 data, data_len, recv_id, arrived_time
    using RecvCallBack = std::function<int32_t(const char*, size_t, uint32_t, uint64_t)>;
    void SetCallback(RecvCallBack callback) { recv_callback_ = std::move(callback); }
    /// 当前end point
    virtual uint32_t MyID() const = 0;
    /// 发送数据接口
    virtual int32_t Send(uint32_t dest_id, const char* buff, size_t buff_len) = 0;
    /// 收包驱动，返回本次处理了多少个包
    virtual size_t Loop(uint32_t max_recv_count) = 0;
    virtual ~IChannel() = default;

protected:
    RecvCallBack recv_callback_ = nullptr;
};

}  // namespace app
#endif
