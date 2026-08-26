/*
 * * file name: json_config.cpp
 * * description: JsonGetStr()实现，见json_config.h说明
 * */

#include "json_config.h"

namespace app
{
namespace config
{
std::string JsonGetStr(const std::string& json, const std::string& key)
{
    // 匹配 "key":"value" 或 "key": "value"
    std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos)
        return "";
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos)
        return "";
    // 跳过空白
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        ++pos;
    if (pos >= json.size())
        return "";
    if (json[pos] == '"')
    {
        // 字符串值
        ++pos;
        auto end = json.find('"', pos);
        if (end == std::string::npos)
            return "";
        return json.substr(pos, end - pos);
    }
    else
    {
        // 数字值，读到下一个分隔符
        auto end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != '\n')
            ++end;
        std::string val = json.substr(pos, end - pos);
        // 去掉首尾空白
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
            val.pop_back();
        return val;
    }
}

}  // namespace config
}  // namespace app
