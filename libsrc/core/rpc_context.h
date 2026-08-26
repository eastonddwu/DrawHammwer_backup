/*
 * * file name: rpc_context.h
 * * description: RPC上下文，请求/回包统一用google::protobuf::Message存储
 * *              是RpcContext（服务端）/RpcClientContext（客户端）的定义
 * */

#ifndef _APP_RPC_CONTEXT_H_
#define _APP_RPC_CONTEXT_H_

#include <cstdint>
#include <google/protobuf/message.h>
#include "context.h"
#include "interface/codec_interface.h"
#include "log.h"

namespace app
{
/// 请求处理时创建的服务端上下文，携带包头缓存和请求/回包消息
struct RpcContext : public ServerContext
{
    struct PkgHeadCache
    {
        uint32_t src = 0;
        uint32_t dst = 0;
        uint64_t gid = 0;
        uint64_t seq_id = 0;
        uint64_t timeout = 0;
        uint32_t cmd = 0;
        uint32_t pkg_flag = 0;
    };

    /// req_proto/rsp_proto分别是该cmd注册时的请求/回包消息原型（default_instance），用于New()出具体类型的对象
    RpcContext(uint32_t transport_type, const ReadCodec& codec, const google::protobuf::Message* req_proto,
               const google::protobuf::Message* rsp_proto)
        : index(transport_type)
    {
        head.src = codec.GetSrc();
        head.dst = codec.GetDst();
        head.gid = codec.GetGid();
        head.seq_id = codec.GetSeqID();
        head.timeout = codec.GetTimeout();
        head.cmd = codec.GetCmd();
        head.pkg_flag = codec.GetFlag();

        req_msg_ = req_proto->New();
        rsp_msg_ = rsp_proto->New();
        if (!req_msg_->ParseFromArray(codec.GetBody(), static_cast<int>(codec.GetBodyLen())))
        {
            APP_LOG_WARN(head.gid, "parse req failed, cmd(0x%08X), seq_id(%lu)", head.cmd, head.seq_id);
        }

        ServerContext::gid = head.gid;
        ServerContext::pkg_flag = static_cast<uint16_t>(head.pkg_flag);
    }

    ~RpcContext() override
    {
        delete req_msg_;
        delete rsp_msg_;
    }

    const google::protobuf::Message& GetReq() const { return *req_msg_; }
    google::protobuf::Message& GetRsp() { return *rsp_msg_; }

    /// 这个请求是否需要被忽略（不回包）
    bool ignore = false;
    /// 收到这个请求所在的transport下标，回包时要用
    uint32_t index = 0;
    PkgHeadCache head;

protected:
    /// 请求包消息
    google::protobuf::Message* req_msg_ = nullptr;
    /// 回包消息
    google::protobuf::Message* rsp_msg_ = nullptr;
};

/// 主调发起时创建的客户端上下文，携带回包消息指针
struct RpcClientContext : public ClientContext
{
    uint64_t gid = 0;
    uint32_t cmd = 0;
    /// 回包消息（外部拥有，不管理生命周期）
    google::protobuf::Message* rsp = nullptr;
};

}  // namespace app

#endif
