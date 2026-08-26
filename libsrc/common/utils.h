/*
 * * file name: utils.h
 * * description: 通用工具函数（简化版，仅保留框架必需的时间函数）
 * */

#ifndef _APP_UTILS_H_
#define _APP_UTILS_H_

#include <cstdint>

namespace app
{
namespace utils
{
/// 获取当前实时的时间（毫秒）
uint64_t CurrentRealMilliSec();
/// 获取当前实时的时间（微秒）
uint64_t CurrentRealMicroSec();

}  // namespace utils
}  // namespace app

#endif
