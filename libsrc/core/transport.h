/*
 * * file name: transport.h
 * * description: 收发通道打包，把channel和codec绑在一起
 * */

#ifndef _APP_TRANSPORT_H_
#define _APP_TRANSPORT_H_

#include <cstdint>

namespace app
{
class IChannel;
class RecvCodec;
class SendCodec;
class IRouting;

struct TransportInfo
{
    /// 把send_codec中已经SetXxx好的内容编码后发送给dst
    int32_t Send(uint32_t dst) const;

    /// 收发包通道
    IChannel* channel = nullptr;
    /// 收包反序列化插件
    RecvCodec* recv_codec = nullptr;
    /// 发包序列化插件
    SendCodec* send_codec = nullptr;
    /// 选路插件（非空时Send()会查路由覆盖dst）
    IRouting* routing = nullptr;
};

}  // namespace app

#endif
