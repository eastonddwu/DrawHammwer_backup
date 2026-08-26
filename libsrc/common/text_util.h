/*
 * * file name: text_util.h
 * * description: 文本校验工具：ASCII空白trim + Unicode码点计数。
 * *              昵称/房间名长度闸门共用（connsvr、roomsvr），计数口径为trim后的Unicode码点数，
 * *              与客户端 UE FString::Len() 一致，不按UTF-8字节数（"小明"=2而非6）。
 * */

#ifndef _APP_TEXT_UTIL_H_
#define _APP_TEXT_UTIL_H_

#include <cstddef>
#include <string>

namespace app
{
namespace text
{
/// 昵称长度上限（trim后的Unicode码点数）
constexpr size_t kMaxUserNameLen = 8;
/// 房间名长度上限（trim后的Unicode码点数）；= kMaxUserNameLen + len("的房间")
constexpr size_t kMaxRoomNameLen = 11;
/// 默认房间名后缀，模板为 "{昵称}的房间"
constexpr const char* kDefaultRoomNameSuffix = "的房间";

/// 去掉首尾ASCII空白（空格/\t/\r/\n等）
std::string TrimAsciiWhitespace(const std::string& s);

/// 统计Unicode码点数（非UTF-8续字节的字节数）
size_t CountUtf8CodePoints(const std::string& s);

/// trim后长度是否落在[1, max_len]。out_trimmed非空时回填trim结果。
bool ValidateLength(const std::string& s, size_t max_len, std::string* out_trimmed);

}  // namespace text
}  // namespace app

#endif
