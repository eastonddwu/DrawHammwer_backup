/*
 * * file name: user_info_codec.h
 * * description: user_info业务数据的二进制序列化格式（rolesvr与dbproxy之间约定的wire format）。
 * *
 * *              格式: is_new(4B) + role_type(4B) + user_name_len(4B) + user_name(NB) + points(8B)
 * *              全部按主机字节序（与原有逐字段memcpy实现保持一致）。
 * *
 * *              原先rolesvr(role_service.cpp)与dbproxy(db_service.cpp)各自手写offset/memcpy，
 * *              易错且重复，现统一到此Encode/Decode。
 * */

#ifndef _APP_USER_INFO_CODEC_H_
#define _APP_USER_INFO_CODEC_H_

#include <cstdint>
#include <string>

namespace app
{
namespace codec
{
struct UserInfo
{
    uint32_t is_new = 0;
    uint32_t role_type = 0;
    std::string user_name;
    uint64_t points = 0;
};

/// 将UserInfo编码为二进制字节流。
std::string EncodeUserInfo(const UserInfo& info);

/// 从二进制字节流解码UserInfo。与原实现一致：字段缺失时保持默认值、不报错（尽力解析）。
/// 返回是否至少解析出首字段（data非空且长度足够）；仅供调用方参考，通常可忽略。
bool DecodeUserInfo(const std::string& data, UserInfo& info);

}  // namespace codec
}  // namespace app

#endif
