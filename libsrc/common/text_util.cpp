/*
 * * file name: text_util.cpp
 * * description: text_util.h的实现
 * */

#include "common/text_util.h"

#include <cctype>

namespace app
{
namespace text
{

std::string TrimAsciiWhitespace(const std::string& s)
{
    size_t begin = 0;
    size_t end = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])))
        ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;
    return s.substr(begin, end - begin);
}

size_t CountUtf8CodePoints(const std::string& s)
{
    size_t count = 0;
    for (unsigned char c : s)
    {
        if ((c & 0xC0) != 0x80)
            ++count;
    }
    return count;
}

bool ValidateLength(const std::string& s, size_t max_len, std::string* out_trimmed)
{
    std::string trimmed = TrimAsciiWhitespace(s);
    if (out_trimmed)
        *out_trimmed = trimmed;
    if (trimmed.empty())
        return false;
    return CountUtf8CodePoints(trimmed) <= max_len;
}

}  // namespace text
}  // namespace app
