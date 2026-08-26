/*
 * * file name: transport_type.h
 * * description: 全局Transport类型枚举，定义所有支持的通信通道类型。
 * *              对齐ua_server的transport_type.h设计，app_server当前使用PB_TBUSPP/TCONND/TCP_PB，
 * *              其余类型(DS_TCP/HTTP/PB_TBUS/UDP)预留扩展位。
 * */

#ifndef _APP_TRANSPORT_TYPE_H_
#define _APP_TRANSPORT_TYPE_H_

#include <cstdint>

namespace app
{
enum TransportType
{
    TRANSPORT_PB_TBUSPP = 0,  // TBuspp/TBus2 + PB codec（后端通用，默认transport）
    TRANSPORT_DS_TCP    = 1,  // TCP + DS codec（dsagent <-> DS进程）
    TRANSPORT_HTTP      = 2,  // HTTP
    TRANSPORT_TCONND    = 3,  // TConnd + PB codec（connsvr <-> 客户端）
    TRANSPORT_PB_TBUS   = 4,  // TBus + PB codec（router专用）
    TRANSPORT_UDP       = 5,  // UDP + DS codec（dsagent）
    TRANSPORT_TCP_PB    = 6,  // TCP + PB codec（直连TCP，echo_demo使用）
};

static constexpr uint32_t RPC_DEFAULT_TIMEOUT = 5000;

}  // namespace app

#endif
