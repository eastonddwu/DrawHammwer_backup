/*
 * * file name: tbus2_channel.h
 * * description: 基于tbuspp2(tbus2)的Channel，实现IChannel接口
 * *              只对接tbuspp2的收发消息能力(tbuspp_open/queue_read/queue_write)，并处理
 * *              ENDPOINT_CHANGE事件驱动RoutingMgr增删路由节点(节点上下线通知)。
 * *              busid直接使用普通数字(与本项目svr_id一致)，不使用tbus v1兼容的字符串寻址体系，
 * *              也不做主机序/网络序转换
 * */

#ifndef _APP_TBUS2_CHANNEL_H_
#define _APP_TBUS2_CHANNEL_H_

#include <cstdint>
#include <string>
#include "core/interface/channel_interface.h"
#include "tbuspp2.h"

namespace app
{
class IRouting;

class TBus2Channel : public IChannel
{
public:
    /// 初始化，my_id是自己的busid(纯数字)，agent_url是本地tbus2 agent地址，如"tcp://127.0.0.1:10708"
    /// (agent_url为空则使用tbuspp2默认地址)
    bool Init(uint32_t my_id, const std::string& agent_url);

    virtual uint32_t MyID() const override { return my_id_; }
    virtual int32_t Send(uint32_t dest_id, const char* buff, size_t buff_len) override;
    virtual size_t Loop(uint32_t max_recv_count) override;

    virtual ~TBus2Channel() override;

    /// 设置路由插件（UseDefaultInit中调用，mesh事件驱动路由表更新）
    void SetRouting(IRouting* routing) { routing_ = routing; }

private:
    /// tbuspp2事件回调分发入口(C回调签名要求为静态函数)
    static int TBus2EventCallback(tbuspp_endpoint_t* ep, const tbuspp_event_t* evt, void* udata);
    /// 事件回调实际处理：处理ENDPOINT_CHANGE事件驱动路由表更新
    int Notify(tbuspp_endpoint_t* ep, const tbuspp_event_t* evt);

    /// 从in_queue_读取消息并回调recv_callback_，最多处理max_recv_count个，返回实际处理个数
    size_t OnRecv(uint32_t max_recv_count);

private:
    uint32_t my_id_ = 0;
    tbuspp_endpoint_t* ep_ = nullptr;
    tbuspp_endpoint_conf_t ep_conf_ = {};
    tbuspp_queue_t* in_queue_ = nullptr;
    tbuspp_queue_t* out_queue_ = nullptr;
    IRouting* routing_ = nullptr;

    static constexpr size_t MAX_PKG_LEN = 64 * 1024;
    char buffer_[MAX_PKG_LEN] = {};
};

}  // namespace app

#endif
