/*
 * * file name: codec_interface.h
 * * description: 统一协议编解码接口，简化版（不依赖protobuf，body为原始字节流）
 * */

#ifndef _APP_CODEC_INTERFACE_H_
#define _APP_CODEC_INTERFACE_H_

#include <cstddef>
#include <cstdint>

namespace app
{
class ReadCodec
{
private:
    ReadCodec(const ReadCodec& other) = delete;
    ReadCodec(ReadCodec&& other) = delete;
    ReadCodec& operator=(const ReadCodec& other) = delete;

public:
    ReadCodec() = default;
    /// 获取消息包头的cmd
    virtual uint32_t GetCmd() const = 0;
    /// 获取消息包头的请求数据唯一ID
    virtual uint64_t GetGid() const = 0;
    /// 获取消息包头的seq id
    virtual uint64_t GetSeqID() const = 0;
    /// 获取源地址
    virtual uint32_t GetSrc() const = 0;
    /// 获取目标地址
    virtual uint32_t GetDst() const = 0;
    /// 获取超时时间戳
    virtual uint64_t GetTimeout() const { return 0; }
    /// 获取请求返回码
    virtual int32_t GetRetCode() const { return 0; }
    /// 获取flag信息
    virtual uint32_t GetFlag() const { return 0; }
    /// 协议包体长度
    virtual uint32_t GetBodyLen() const = 0;
    /// 协议包体
    virtual const char* GetBody() const = 0;
    /// 重置所有的数据
    virtual void Reset() = 0;
    virtual ~ReadCodec() = default;
};

class WriteCodec : public ReadCodec
{
public:
    /// 设置消息包头的cmd
    virtual void SetCmd(uint32_t cmd) = 0;
    /// 设置消息包头的请求数据唯一ID
    virtual void SetGid(uint64_t gid) = 0;
    /// 设置消息包头的seq id
    virtual void SetSeqID(uint64_t seq_id) = 0;
    /// 设置源地址
    virtual void SetSrc(uint32_t id) = 0;
    /// 设置目标地址
    virtual void SetDst(uint32_t id) = 0;
    /// 设置超时时间戳
    virtual void SetTimeout(uint64_t ms_time) {}
    /// 设置请求返回码
    virtual void SetRetCode(int32_t ret_code) {}
    /// 设置flag信息
    virtual void SetFlag(uint32_t flag) {}
    /// 设置包体数据
    virtual bool SetBody(const char* data, uint32_t len) = 0;
    virtual ~WriteCodec() = default;
};

class RecvCodec : public ReadCodec
{
public:
    /// 从二进制流解码，返回消耗的字节数，0表示数据不够，<0表示出错
    virtual int32_t Decode(const char* data, uint32_t data_len) = 0;
};

class SendCodec : public WriteCodec
{
public:
    /// 编码成二进制，data_len输出编码后的长度
    virtual const char* Encode(uint32_t& data_len) = 0;
};

}  // namespace app

#endif
