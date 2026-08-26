/*
 * * file name: pb_codec.cpp
 * * description: ...
 * */

#include "pb_codec.h"
#include <cstring>

namespace app
{
void PbRecvCodec::Reset()
{
    head_.Clear();
    body_.clear();
}

int32_t PbRecvCodec::Decode(const char* data, uint32_t data_len)
{
    int64_t frame_len = TryGetFrameLen(data, data_len);
    if (frame_len <= 0)
        return static_cast<int32_t>(frame_len);

    const FramePrefix* prefix = reinterpret_cast<const FramePrefix*>(data);
    const char* head_data = data + sizeof(FramePrefix);
    const char* body_data = head_data + prefix->head_len;

    if (!head_.ParseFromArray(head_data, static_cast<int>(prefix->head_len)))
        return -1;

    body_.assign(body_data, prefix->body_len);
    return static_cast<int32_t>(frame_len);
}

void PbSendCodec::Reset()
{
    head_.Clear();
    body_.clear();
    encode_buf_.clear();
}

bool PbSendCodec::SetBody(const char* data, uint32_t len)
{
    if (len > PKG_MAX_BODY_LEN)
        return false;

    body_.assign(data, len);
    return true;
}

const char* PbSendCodec::Encode(uint32_t& data_len)
{
    std::string head_bytes;
    head_.SerializeToString(&head_bytes);

    encode_buf_ = BuildFrame(head_bytes, body_);

    data_len = static_cast<uint32_t>(encode_buf_.size());
    return encode_buf_.data();
}

}  // namespace app
