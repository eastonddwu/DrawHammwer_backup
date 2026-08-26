/*
 * * file name: user_info_codec.cpp
 * * description: EncodeUserInfo()/DecodeUserInfo()实现，见user_info_codec.h说明
 * */

#include "user_info_codec.h"
#include <cstring>

namespace app
{
namespace codec
{
std::string EncodeUserInfo(const UserInfo& info)
{
    uint32_t name_len = static_cast<uint32_t>(info.user_name.size());

    std::string buf;
    buf.append(reinterpret_cast<const char*>(&info.is_new), sizeof(info.is_new));
    buf.append(reinterpret_cast<const char*>(&info.role_type), sizeof(info.role_type));
    buf.append(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
    buf.append(info.user_name);
    buf.append(reinterpret_cast<const char*>(&info.points), sizeof(info.points));
    return buf;
}

bool DecodeUserInfo(const std::string& data, UserInfo& info)
{
    size_t offset = 0;
    bool parsed_any = false;

    if (data.size() >= offset + sizeof(uint32_t))
    {
        std::memcpy(&info.is_new, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        parsed_any = true;
    }
    if (data.size() >= offset + sizeof(uint32_t))
    {
        std::memcpy(&info.role_type, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
    uint32_t name_len = 0;
    if (data.size() >= offset + sizeof(uint32_t))
    {
        std::memcpy(&name_len, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
    if (name_len > 0 && data.size() >= offset + name_len)
    {
        info.user_name.assign(data.data() + offset, name_len);
        offset += name_len;
    }
    if (data.size() >= offset + sizeof(uint64_t))
    {
        std::memcpy(&info.points, data.data() + offset, sizeof(uint64_t));
    }
    return parsed_any;
}

}  // namespace codec
}  // namespace app
