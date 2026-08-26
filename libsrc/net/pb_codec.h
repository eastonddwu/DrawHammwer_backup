/*
 * * file name: pb_codec.h
 * * description: protobuf编解码，实现RecvCodec/SendCodec
 * *              包头用PkgHead(protobuf message)序列化，body为业务protobuf消息序列化后的字节流
 * */

#ifndef _APP_PB_CODEC_H_
#define _APP_PB_CODEC_H_

#include <string>
#include "core/interface/codec_interface.h"
#include "pkg_framing.h"
#include "pkg_head.pb.h"

namespace app
{
/// 抽取PbRecvCodec/PbSendCodec共用的PkgHead存储与只读getter实现。
/// 以Base为模板参数（RecvCodec或SendCodec），沿单一继承链混入，避免ReadCodec菱形继承。
template <typename Base>
class PbCodecBase : public Base
{
public:
    virtual uint32_t GetCmd() const override { return head_.cmd(); }
    virtual uint64_t GetGid() const override { return head_.gid(); }
    virtual uint64_t GetSeqID() const override { return head_.seq_id(); }
    virtual uint32_t GetSrc() const override { return head_.src(); }
    virtual uint32_t GetDst() const override { return head_.dst(); }
    virtual uint64_t GetTimeout() const override { return head_.timeout(); }
    virtual int32_t GetRetCode() const override { return head_.ret_code(); }
    virtual uint32_t GetFlag() const override { return head_.flag(); }
    virtual uint32_t GetBodyLen() const override { return static_cast<uint32_t>(body_.size()); }
    virtual const char* GetBody() const override { return body_.data(); }

protected:
    app::protocol::PkgHead head_;
    std::string body_;
};

class PbRecvCodec : public PbCodecBase<RecvCodec>
{
public:
    virtual void Reset() override;

    /// 从二进制流解码，返回消耗的字节数，0表示数据不够，<0表示出错
    virtual int32_t Decode(const char* data, uint32_t data_len) override;
};

class PbSendCodec : public PbCodecBase<SendCodec>
{
public:
    virtual void Reset() override;

    virtual void SetCmd(uint32_t cmd) override { head_.set_cmd(cmd); }
    virtual void SetGid(uint64_t gid) override { head_.set_gid(gid); }
    virtual void SetSeqID(uint64_t seq_id) override { head_.set_seq_id(seq_id); }
    virtual void SetSrc(uint32_t id) override { head_.set_src(id); }
    virtual void SetDst(uint32_t id) override { head_.set_dst(id); }
    virtual void SetTimeout(uint64_t ms_time) override { head_.set_timeout(ms_time); }
    virtual void SetRetCode(int32_t ret_code) override { head_.set_ret_code(ret_code); }
    virtual void SetFlag(uint32_t flag) override { head_.set_flag(flag); }
    virtual bool SetBody(const char* data, uint32_t len) override;

    /// 编码成二进制，data_len输出编码后的长度
    virtual const char* Encode(uint32_t& data_len) override;

private:
    std::string encode_buf_;
};

}  // namespace app

#endif
