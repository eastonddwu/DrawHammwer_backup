/*
 * * file name: rpc_service.h
 * * description: RPC分发服务（对齐ua_server的PBService设计）
 * *              管理cmd->RpcMethod注册表，负责收发包的编解码调度、协程拉起、Pending/Awake挂起唤醒
 * */

#ifndef _APP_RPC_SERVICE_H_
#define _APP_RPC_SERVICE_H_

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <google/protobuf/message.h>
#include "context.h"
#include "patterns/singleton.h"
#include "rpc_context.h"
#include "transport.h"

namespace app
{
class ContextController;

/// rpc方法处理函数，业务层直接实现这个签名，通过context读取请求、写入回包
using RpcHandler = std::function<void(RpcContext& context)>;

struct RpcMethod
{
    RpcHandler handler;
    /// 请求/回包消息原型（default_instance），用于New()出该cmd对应的具体消息类型
    const google::protobuf::Message* req_prototype = nullptr;
    const google::protobuf::Message* rsp_prototype = nullptr;
    bool is_private = false;
};

class RpcService : public Singleton<RpcService>
{
public:
    /// 支持最多10个transport
    static constexpr uint32_t MAX_TRANSPORT_NUM = 10;

    /// 设置上下文调度器句柄
    void SetContextCtrl(ContextController* context_ctrl) { context_ctrl_ = context_ctrl; }
    /// 注册提供的服务
    bool RegisterMethod(uint32_t cmd, const RpcMethod& method_info);

    /// 发起rpc，dest是指定目标地址，timeout是超时时间间隔（单位ms），rsp为nullptr时则task.callback无效
    /// 指定了rsp并且task.callback为nullptr则会挂起当前协程，其它情况下都不会挂起
    int32_t Rpc(uint32_t transport_type, uint64_t gid, uint32_t cmd, const google::protobuf::Message& req,
                google::protobuf::Message* rsp, const AsyncTask& task, uint32_t dest, uint32_t timeout);

    /// 添加多一个channel信息，允许多次调用添加多个
    bool AddTransport(uint32_t transport_type, const TransportInfo& info);
    /// 获取一个channel信息，如果没有添加则返回nullptr
    const TransportInfo* FindTransport(uint32_t transport_type) const;

private:
    /// 收到数据包回调函数
    int32_t OnRecv(uint32_t transport_type, const char* data, size_t data_len, uint32_t recv_id,
                    uint64_t arrived_time);
    /// 处理请求包
    bool DealRequest(uint32_t transport_type, const ReadCodec& codec);
    /// 处理回包
    void DealResponse(const ReadCodec& codec);
    /// 请求处理事务逻辑启动
    void DealMethod(RpcContext* context, const RpcHandler& handler);
    /// 处理完method请求，回包和回收资源
    void MethodFinish(RpcContext* context);
    /// 发送一个消息，相关的目标信息通过send_codec来设置
    int32_t SendMessage(const TransportInfo& info, const std::string& body);

private:
    friend Singleton<RpcService>;
    RpcService() = default;
    ~RpcService() = default;

private:
    /// 注册的服务
    std::unordered_map<uint32_t, RpcMethod> methods_;
    /// 上下文切换管理器
    ContextController* context_ctrl_ = nullptr;
    /// 通道数组
    std::array<TransportInfo, MAX_TRANSPORT_NUM> transport_infos_;
};

}  // namespace app

#endif
