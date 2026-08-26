/*
 * * file name: json_config.h
 * * description: 轻量级JSON字段读取辅助函数。各业务服务main.cpp在--conf-file启动模式下，
 * *              需要从简单的扁平JSON配置里取出字符串/数字字段。此处提供统一实现，
 * *              替代原先散落在各server/main.cpp中逐字重复的JsonGetStr()。
 * *
 * *              注意：这是一个只支持扁平结构、不做完整语法校验的极简解析器，
 * *              仅用于读取TCM下发的简单conf-file（如{"svr_id":1,"tbus2_agent_url":"..."}）。
 * *              不适用于嵌套对象/数组等复杂JSON。
 * */

#ifndef _APP_JSON_CONFIG_H_
#define _APP_JSON_CONFIG_H_

#include <string>

namespace app
{
namespace config
{
/// 从扁平JSON字符串里按key取出值（支持 "key":"value" 字符串 或 "key":number 数字）。
/// 找不到key、或格式不完整时返回空字符串。数字值返回其原始文本，由调用方自行转换。
std::string JsonGetStr(const std::string& json, const std::string& key);

}  // namespace config
}  // namespace app

#endif
