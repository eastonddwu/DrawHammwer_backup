/*
 * * file name: pkg_framing.h
 * * description: TCP分包/粘包处理用的最小定长前缀定义，被pb_codec和tcp_channel共用
 * *              帧格式: [FramePrefix(12B)][head二进制(head_len字节)][body二进制(body_len字节)]
 * *              head是PkgHead(protobuf message)序列化后的字节，body是业务protobuf消息序列化后的字节
 * *              tcp_channel需要在不完全解码的情况下探测一个完整帧的长度用于TCP分包/粘包处理
 * */

#ifndef _APP_PKG_FRAMING_H_
#define _APP_PKG_FRAMING_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace app
{
#pragma pack(push, 1)
struct FramePrefix
{
    uint32_t magic = 0;
    uint32_t head_len = 0;
    uint32_t body_len = 0;
};
#pragma pack(pop)

constexpr uint32_t PKG_MAGIC = 0xA5A5A5A5;
constexpr uint32_t PKG_MAX_HEAD_LEN = 4 * 1024;
constexpr uint32_t PKG_MAX_BODY_LEN = 1 * 1024 * 1024;

/// 探测buffer中是否有一个完整帧，返回值：>0表示完整帧总长度(前缀+head+body)；0表示数据不够；<0表示数据错误(魔数不对/head或body过长)
inline int64_t TryGetFrameLen(const char* data, size_t len)
{
    if (len < sizeof(FramePrefix))
        return 0;

    const FramePrefix* prefix = reinterpret_cast<const FramePrefix*>(data);
    if (prefix->magic != PKG_MAGIC)
        return -1;
    if (prefix->head_len > PKG_MAX_HEAD_LEN)
        return -1;
    if (prefix->body_len > PKG_MAX_BODY_LEN)
        return -1;

    return static_cast<int64_t>(sizeof(FramePrefix) + prefix->head_len + prefix->body_len);
}

/// 按帧格式 [FramePrefix][head][body] 拼装一个完整帧。head/body为已序列化的二进制字节。
/// 与PbSendCodec::Encode()的拼装逻辑一致，供直接构造帧的场景（如connsvr主动推送）复用。
inline std::string BuildFrame(const std::string& head_bytes, const std::string& body_bytes)
{
    FramePrefix prefix;
    prefix.magic = PKG_MAGIC;
    prefix.head_len = static_cast<uint32_t>(head_bytes.size());
    prefix.body_len = static_cast<uint32_t>(body_bytes.size());

    std::string frame;
    frame.resize(sizeof(FramePrefix) + head_bytes.size() + body_bytes.size());
    char* buf = &frame[0];
    std::memcpy(buf, &prefix, sizeof(FramePrefix));
    std::memcpy(buf + sizeof(FramePrefix), head_bytes.data(), head_bytes.size());
    if (!body_bytes.empty())
        std::memcpy(buf + sizeof(FramePrefix) + head_bytes.size(), body_bytes.data(), body_bytes.size());
    return frame;
}

}  // namespace app

#endif
