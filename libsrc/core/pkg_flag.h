/*
 * * file name: pkg_flag.h
 * * description: 协议包头flag标志位定义（简化版，仅保留MVP闭环需要用到的几个）
 * */

#ifndef _APP_PKG_FLAG_H_
#define _APP_PKG_FLAG_H_

#include <cstdint>

namespace app
{
constexpr uint32_t FLAG_RSP_PKG = 0x0001;      // 没有标记代表是请求包
constexpr uint32_t FLAG_DONT_RSP = 0x0002;     // 是否需要回包
constexpr uint32_t FLAG_FROM_TCONND = 0x0004;  // 来自tconnd的客户端请求

}  // namespace app

#endif
