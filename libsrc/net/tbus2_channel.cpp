/*
 * * file name: tbus2_channel.cpp
 * * description: TBus2Channel实现，见tbus2_channel.h
 * */

#include "tbus2_channel.h"
#include <cstring>
#include "core/interface/routing_interface.h"
#include "core/log.h"
#include "core/svr_type.h"
#include "tbuspp2_defs.h"

namespace app
{
bool TBus2Channel::Init(uint32_t my_id, const std::string& agent_url)
{
    my_id_ = my_id;

    std::memset(&ep_conf_, 0, sizeof(ep_conf_));
    if (!agent_url.empty())
        strncpy(ep_conf_.agent_url, agent_url.c_str(), sizeof(ep_conf_.agent_url) - 1);
    ep_conf_.busid = my_id_;  // 纯数字busid，不使用busid_str
    ep_conf_.start_standby = false;
    ep_conf_.prefer_runas_last_status = true;
    ep_conf_.close_by_unexpect_exit = true;
    ep_conf_.keepalive_with_ping = true;
    ep_conf_.cb = TBus2EventCallback;
    ep_conf_.cb_udata = this;

    int err = 0;
    ep_ = tbuspp_open(&ep_conf_, 3000, nullptr, &err);
    if (!ep_)
    {
        APP_LOG_ERROR(0, "tbuspp_open fail, my_id(%u), agent_url(%s), err(%d): %s", my_id_, agent_url.c_str(), err,
                      tbuspp_error_string(err));
        return false;
    }

    tbuspp_id_t got_busid = tbuspp_get_busid(ep_);
    if (got_busid != my_id_)
    {
        APP_LOG_ERROR(0, "tbuspp_get_busid mismatch, my_id(%u), got(%llu)", my_id_,
                      static_cast<unsigned long long>(got_busid));
        tbuspp_close(ep_);
        ep_ = nullptr;
        return false;
    }

    in_queue_ = tbuspp_get_input_queue(ep_);
    out_queue_ = tbuspp_get_output_queue(ep_);
    if (!in_queue_ || !out_queue_)
    {
        APP_LOG_ERROR(0, "get in/out queue fail, my_id(%u)", my_id_);
        tbuspp_close(ep_);
        ep_ = nullptr;
        return false;
    }

    APP_LOG_INFO(0, "tbus2 channel init ok, my_id(%u), agent_url(%s)", my_id_, agent_url.c_str());
    return true;
}

TBus2Channel::~TBus2Channel()
{
    if (ep_)
        tbuspp_close(ep_);
}

int TBus2Channel::TBus2EventCallback(tbuspp_endpoint_t* ep, const tbuspp_event_t* evt, void* udata)
{
    if (!ep || !evt || !udata)
        return -1;
    return static_cast<TBus2Channel*>(udata)->Notify(ep, evt);
}

int TBus2Channel::Notify(tbuspp_endpoint_t* /*ep*/, const tbuspp_event_t* evt)
{
    if (!evt)
        return -1;

    // 处理节点上下线事件，驱动路由表更新
    if (evt->event_id == TBUSPP_EVT_ENDPOINT_CHANGE_EVT)
    {
        const auto& change = evt->endpoint_change_evt;
        uint32_t busid = static_cast<uint32_t>(change.busid);
        uint32_t svr_type = SvrTypeFromBusid(busid);

        if (change.status == TBUSPP_ENDPOINT_STATUS_READY)
        {
            APP_LOG_INFO(0, "tbus2 endpoint online, busid(%u), svr_type(%u), old_status(%d)",
                         busid, svr_type, change.old_status);
            if (routing_)
                routing_->AddRoute(svr_type, busid);
        }
        else if (change.status == TBUSPP_ENDPOINT_STATUS_STOP)
        {
            APP_LOG_INFO(0, "tbus2 endpoint offline, busid(%u), svr_type(%u), old_status(%d)",
                         busid, svr_type, change.old_status);
            if (routing_)
                routing_->DelRoute(svr_type, busid);
        }
        else
        {
            APP_LOG_INFO(0, "tbus2 endpoint status change, busid(%u), status(%d), old_status(%d)",
                         busid, change.status, change.old_status);
        }
    }
    else
    {
        APP_LOG_INFO(0, "tbus2 event, my_id(%u), event_id(%u)", my_id_, evt->event_id);
    }

    return 0;
}

int32_t TBus2Channel::Send(uint32_t dest_id, const char* buff, size_t buff_len)
{
    if (!ep_ || !out_queue_)
    {
        APP_LOG_ERROR(0, "channel not ready, dest_id(%u)", dest_id);
        return -1;
    }

    tbuspp_msg_param_t param;
    tbuspp_init_msg_param(&param);
    int ret = tbuspp_queue_write(out_queue_, dest_id, buff, buff_len, &param);
    if (ret != 0)
    {
        APP_LOG_ERROR(0, "tbuspp_queue_write fail, dest_id(%u), ret(%d): %s", dest_id, ret,
                      tbuspp_error_string(ret));
        return -1;
    }
    return 0;
}

size_t TBus2Channel::OnRecv(uint32_t max_recv_count)
{
    if (!ep_ || !in_queue_)
        return 0;

    size_t pkg_count = 0;
    while (pkg_count < max_recv_count)
    {
        uint32_t msg_len = 0;
        tbuspp_msg_desc_t ctx{};
        int ret = tbuspp_queue_read(in_queue_, buffer_, static_cast<uint32_t>(MAX_PKG_LEN), &msg_len, &ctx);
        if (ret == TBUSPP_ERR_QUEUE_EMPTY)
        {
            break;
        }
        else if (ret == TBUSPP_ERR_LESS_MSG_BUF)
        {
            APP_LOG_WARN(0, "pkg too large, len(%u), src(%llu)", msg_len,
                         static_cast<unsigned long long>(ctx.src));
            tbuspp_queue_pop(in_queue_);
            continue;
        }
        else if (ret != TBUSPP_ERR_OK)
        {
            APP_LOG_WARN(0, "tbuspp_queue_read fail, ret(%d): %s", ret, tbuspp_error_string(ret));
            break;
        }

        if (recv_callback_)
            recv_callback_(buffer_, msg_len, static_cast<uint32_t>(ctx.src), ctx.ctime / 1000);

        ++pkg_count;
    }

    return pkg_count;
}

size_t TBus2Channel::Loop(uint32_t max_recv_count)
{
    if (!ep_)
        return 0;

    tbuspp_update(ep_, 0);  // 驱动事件回调/心跳，app_server是单线程驱动，无并发问题
    return OnRecv(max_recv_count);
}

}  // namespace app
