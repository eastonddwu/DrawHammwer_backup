/*
 * * file name: default_init.h
 * * description: 默认transport初始化（对齐ua_server的UseDefaultInit设计），
 * *              提供UseDefaultInit函数：创建TBus2Channel + PB编解码器，
 * *              注册TRANSPORT_PB_TBUSPP为default transport。
 * *              业务server在OnInit()中调用此函数即可完成默认链路注册，
 * *              无需手动创建channel/codecs。
 * */

#ifndef _APP_DEFAULT_INIT_H_
#define _APP_DEFAULT_INIT_H_

#include <cstdint>
#include <string>

namespace app
{
class ServerCore;

/// 初始化默认transport（TRANSPORT_PB_TBUSPP）：
/// 创建TBus2Channel + PbRecvCodec/PbSendCodec，Init channel，注册为default transport。
/// 额外的transport（如TCONND/TCP）再手动AddTransportInfo注册。
///
/// @param svr        ServerCore实例（通常是BaseServer子类的*this）
/// @param busid      tbus2 busid（需落在namesrv配置的非零group下，如0x01000000 | svr_id）
/// @param agent_url  tbus2 agent的URL
/// @return           初始化成功返回true
bool UseDefaultInit(ServerCore& svr, uint32_t busid, const std::string& agent_url);

/// 便捷封装：UseDefaultInit + 预留系统插件扩展点（对齐ua_server的UseDefaultPlugin命名）。
/// 当前等价于UseDefaultInit，后续可在此处追加MonitorBk/TracerPlugin等系统级插件初始化。
inline bool UseDefaultPlugin(ServerCore& svr, uint32_t busid, const std::string& agent_url)
{
    return UseDefaultInit(svr, busid, agent_url);
}

}  // namespace app

#endif
