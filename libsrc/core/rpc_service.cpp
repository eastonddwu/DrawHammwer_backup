/*
 * * file name: rpc_service.cpp
 * * description: ...
 * */

#include "rpc_service.h"
#include <cassert>
#include <cstring>
#include "common/clock.h"
#include "common/id_generator.h"
#include "common/metrics.h"
#include "common/utils.h"
#include "context_controller.h"
#include "context_mgr.h"
#include "coro_mgr.h"
#include "interface/channel_interface.h"
#include "interface/codec_interface.h"
#include "log.h"
#include "pkg_flag.h"
#include "rpc_error.h"

namespace app
{
bool RpcService::RegisterMethod(uint32_t cmd, const RpcMethod& method_info)
{
    return methods_.insert({cmd, method_info}).second;
}

int32_t RpcService::SendMessage(const TransportInfo& info, const std::string& body)
{
    SendCodec* send_codec = info.send_codec;

    if (!send_codec->SetBody(body.data(), static_cast<uint32_t>(body.size())))
    {
        APP_LOG_ERROR(send_codec->GetGid(), "set body error, cmd(0x%08X), len(%zu)", send_codec->GetCmd(),
                      body.size());
        return RPC_MSG_ENCODE_ERR;
    }

    return info.Send(send_codec->GetDst());
}

void RpcService::MethodFinish(RpcContext* context)
{
    assert(context->index < MAX_TRANSPORT_NUM);

    if (!context->ignore)
    {
        if (!(context->head.pkg_flag & FLAG_DONT_RSP))
        {
            auto&& info = transport_infos_[context->index];
            auto&& send_codec = info.send_codec;
            send_codec->Reset();
            send_codec->SetSrc(info.channel->MyID());
            send_codec->SetDst(context->head.src);
            send_codec->SetTimeout(0);
            send_codec->SetGid(context->head.gid);
            send_codec->SetSeqID(context->head.seq_id);
            send_codec->SetCmd(context->head.cmd);
            send_codec->SetRetCode(context->ret_code);
            send_codec->SetFlag(context->head.pkg_flag | FLAG_DONT_RSP | FLAG_RSP_PKG);

            std::string rsp_bytes;
            context->GetRsp().SerializeToString(&rsp_bytes);

            APP_LOG_TRACE(context->head.gid, "response cmd(0x%08X), src(%u), dst(%u), ret(%d), body_len(%zu)",
                          context->head.cmd, send_codec->GetSrc(), send_codec->GetDst(), context->ret_code,
                          rsp_bytes.size());

            auto ret = SendMessage(info, rsp_bytes);
            if (ret != RPC_SUCCESS)
            {
                APP_LOG_WARN(context->head.gid, "send rsp error(%d), cmd(0x%08X)", ret, context->head.cmd);
            }
        }
    }

    context->end_time = utils::CurrentRealMicroSec();
    Metrics::GetInst().OnServerRpc(context->head.cmd, context->DurationMicro(),
                                   context->ret_code == RPC_TIME_OUT);
    ContextMgr::SetCurrServerContext(nullptr);
}

int32_t RpcService::OnRecv(uint32_t transport_type, const char* data, size_t data_len, uint32_t recv_id,
                            uint64_t arrived_time)
{
    // transport_type来自channel回调（在AddTransport里绑定），正常情况下必然合法；
    // 这里在系统入口(网络收包)处做一次防御性校验，避免越界/空codec导致UB。
    if (transport_type >= MAX_TRANSPORT_NUM || !transport_infos_[transport_type].recv_codec)
    {
        APP_LOG_ERROR(0, "invalid transport_type(%u) on recv, recv_id(%u)", transport_type, recv_id);
        return RPC_SYS_ERR;
    }

    auto&& recv_codec = transport_infos_[transport_type].recv_codec;
    int32_t decode_len = recv_codec->Decode(data, static_cast<uint32_t>(data_len));
    if (decode_len < 0)
    {
        APP_LOG_ERROR(0, "decode pkg failed, recv_id(%u)", recv_id);
        return RPC_MSG_DECODE_ERR;
    }
    if (decode_len == 0)
    {
        // 数据不够，等待下次继续收
        return RPC_SUCCESS;
    }

    uint64_t gid = recv_codec->GetGid();
    uint32_t cmd = recv_codec->GetCmd();
    bool is_rsp = recv_codec->GetFlag() & FLAG_RSP_PKG;

    APP_LOG_TRACE(gid, "on recv, cmd(0x%08X), is_rsp(%d), seq_id(%lu), body_len(%u), recv_id(%u)", cmd, is_rsp,
                  recv_codec->GetSeqID(), recv_codec->GetBodyLen(), recv_id);

    if (!is_rsp)
        DealRequest(transport_type, *recv_codec);
    else
        DealResponse(*recv_codec);

    return RPC_SUCCESS;
}

bool RpcService::DealRequest(uint32_t transport_type, const ReadCodec& codec)
{
    uint64_t gid = codec.GetGid();
    uint32_t cmd = codec.GetCmd();
    if (codec.GetTimeout() > 0 && codec.GetTimeout() < Clock::GetInst().CurrentMilliSec())
    {
        APP_LOG_WARN(gid, "drop pkg, cmd(0x%08X), seq_id(%lu), expired(%lu)", cmd, codec.GetSeqID(),
                     codec.GetTimeout());
        return false;
    }

    auto iter = methods_.find(cmd);
    if (iter == methods_.end())
    {
        APP_LOG_ERROR(gid, "recv req, cmd(0x%08X), seq_id(%lu) can not find method", cmd, codec.GetSeqID());
        return false;
    }
    auto&& rpc_method = iter->second;

    RpcContext* context = new RpcContext(transport_type, codec, rpc_method.req_prototype, rpc_method.rsp_prototype);
    context->SetCallback([this, context](int32_t) { MethodFinish(context); }, [context]() { delete context; });
    // 用实时微秒而非Clock缓存值：Clock每帧只更新一次且精度毫秒，同帧内完成的请求耗时会算成0
    context->start_time = utils::CurrentRealMicroSec();

    auto&& handler = rpc_method.handler;
    if (!CoroMgr::GetInst().Spawn([context, handler, this]() { DealMethod(context, handler); }))
    {
        APP_LOG_ERROR(gid, "spawn error, cmd(0x%08X)", cmd);
        Metrics::GetInst().OnRejected(cmd);
        delete context;
        return false;
    }

    return true;
}

void RpcService::DealMethod(RpcContext* context, const RpcHandler& handler)
{
    ContextMgr::SetCurrServerContext(context);

    handler(*context);

    context->Run();
}

int32_t RpcService::Rpc(uint32_t transport_type, uint64_t gid, uint32_t cmd, const google::protobuf::Message& req,
                         google::protobuf::Message* rsp, const AsyncTask& task, uint32_t dest, uint32_t timeout)
{
    assert(transport_type < MAX_TRANSPORT_NUM);
    auto&& info = transport_infos_[transport_type];
    assert(info.channel);
    assert(info.send_codec);

    auto&& send_codec = info.send_codec;
    send_codec->Reset();
    send_codec->SetSrc(info.channel->MyID());
    send_codec->SetDst(dest);
    send_codec->SetTimeout(timeout > 0 ? Clock::GetInst().CurrentMilliSec() + timeout : 0);
    send_codec->SetGid(gid);
    send_codec->SetCmd(cmd);
    send_codec->SetRetCode(RPC_SUCCESS);

    uint64_t seq_id = 0;
    uint32_t pkg_flag = 0;

    if (!rsp)
        pkg_flag |= FLAG_DONT_RSP;
    else
        seq_id = IDGenerator::GetInst().GenerateSeqID();

    send_codec->SetFlag(pkg_flag);
    send_codec->SetSeqID(seq_id);

    std::string req_bytes;
    req.SerializeToString(&req_bytes);

    int32_t ret = SendMessage(info, req_bytes);
    if (ret != RPC_SUCCESS)
    {
        APP_LOG_WARN(gid, "send req error(%d), cmd(0x%08X)", ret, cmd);
        return ret;
    }

    APP_LOG_TRACE(gid, "Rpc|gid(%lu) cmd(0x%08X) seq_id(%lu) req_len(%zu)", gid, cmd, seq_id, req_bytes.size());

    if (!rsp)
        return RPC_SUCCESS;

    if (task.callback)
    {
        RpcClientContext* client_ctx = new RpcClientContext;
        client_ctx->gid = gid;
        client_ctx->cmd = cmd;
        client_ctx->rsp = rsp;

        AsyncTask task_wrapper = {[cmd, seq_id, cb = task.callback](int32_t ret_code, ServerContext* ctx) {
                                       if (ret_code != RPC_SUCCESS)
                                       {
                                           APP_LOG_WARN(0, "rpc fail: cmd(0x%08X) seq_id(%lu) ret(%d)", cmd, seq_id,
                                                        ret_code);
                                       }
                                       if (cb)
                                           cb(ret_code, ctx);
                                   },
                                   [client_ctx, recycle = task.recycle_fun]() {
                                       if (recycle)
                                           recycle();
                                       delete client_ctx;
                                   },
                                   task.blocking_fun};

        ret = context_ctrl_->Pending(seq_id, timeout, client_ctx, task_wrapper);
        if (ret != RPC_SUCCESS)
            delete client_ctx;
        return ret;
    }
    else
    {
        RpcClientContext client_ctx;
        client_ctx.gid = gid;
        client_ctx.cmd = cmd;
        client_ctx.rsp = rsp;

        AsyncTask task_wrapper = {[cmd, seq_id](int32_t ret_code, ServerContext*) {
            if (ret_code != RPC_SUCCESS)
            {
                APP_LOG_WARN(0, "rpc fail: cmd(0x%08X) seq_id(%lu) ret(%d)", cmd, seq_id, ret_code);
            }
        }};
        // 协程模式下Pending()内部Yield，返回时说明响应已到达（或超时），可直接量出往返耗时
        uint64_t rpc_begin = utils::CurrentRealMicroSec();
        ret = context_ctrl_->Pending(seq_id, timeout, &client_ctx, task_wrapper);
        Metrics::GetInst().OnClientRpc(cmd, utils::CurrentRealMicroSec() - rpc_begin,
                                       client_ctx.ret_code == RPC_TIME_OUT);
        if (ret != RPC_SUCCESS)
            return ret;
        return client_ctx.ret_code;
    }
}

bool RpcService::AddTransport(uint32_t transport_type, const TransportInfo& info)
{
    if (transport_type >= MAX_TRANSPORT_NUM)
        return false;

    if (!info.channel || !info.recv_codec || !info.send_codec)
    {
        APP_LOG_ERROR(0, "channel(%p), recv_codec(%p), send_codec(%p)", (void*)info.channel, (void*)info.recv_codec,
                      (void*)info.send_codec);
        return false;
    }

    if (transport_infos_[transport_type].channel)
    {
        APP_LOG_ERROR(0, "transport(%u) already has value", transport_type);
        return false;
    }

    transport_infos_[transport_type] = info;

    info.channel->SetCallback([this, transport_type](const char* data, size_t len, uint32_t recv_id,
                                                       uint64_t arrived_time) {
        return OnRecv(transport_type, data, len, recv_id, arrived_time);
    });

    return true;
}

const TransportInfo* RpcService::FindTransport(uint32_t transport_type) const
{
    if (transport_type >= MAX_TRANSPORT_NUM)
        return nullptr;

    if (!transport_infos_[transport_type].channel)
        return nullptr;

    return &(transport_infos_[transport_type]);
}

void RpcService::DealResponse(const ReadCodec& codec)
{
    uint64_t seq_id = codec.GetSeqID();
    uint64_t gid = codec.GetGid();
    uint32_t cmd = codec.GetCmd();
    auto client_ctx = static_cast<RpcClientContext*>(context_ctrl_->Awake(seq_id, codec.GetRetCode()));
    if (!client_ctx)
    {
        APP_LOG_WARN(gid, "cache can not find seq_id(%lu), cmd(0x%08X)", seq_id, cmd);
        return;
    }

    auto rsp = client_ctx->rsp;
    assert(rsp);
    uint64_t req_gid = client_ctx->gid;
    uint32_t req_cmd = client_ctx->cmd;
    if (gid != req_gid || cmd != req_cmd)
    {
        APP_LOG_ERROR(gid, "seq_id(%lu) response error gid(%lu) cmd(0x%08X) != cache gid(%lu) cmd(0x%08X)", seq_id,
                      gid, cmd, req_gid, req_cmd);
        client_ctx->ret_code = RPC_SYS_ERR;
    }
    else
    {
        if (!rsp->ParseFromArray(codec.GetBody(), static_cast<int>(codec.GetBodyLen())))
        {
            APP_LOG_ERROR(gid, "parse rsp failed, cmd(0x%08X), seq_id(%lu)", cmd, seq_id);
            client_ctx->ret_code = RPC_MSG_DECODE_ERR;
        }
    }

    APP_LOG_TRACE(gid, "deal rsp, seq_id(%lu) cmd(0x%08X), ret(%d), body_len(%u)", seq_id, cmd, client_ctx->ret_code,
                  codec.GetBodyLen());

    client_ctx->Run();
}

}  // namespace app
