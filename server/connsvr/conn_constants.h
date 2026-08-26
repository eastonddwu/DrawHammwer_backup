/*
 * * file name: conn_constants.h
 * * description: connsvr内部使用的常量：转发RPC的超时、以及回给客户端的错误码。
 * *              原先散落在conn_service.cpp各handler中的魔数(2000/3000/1999)统一到此处。
 * */

#ifndef _CONN_CONSTANTS_H_
#define _CONN_CONSTANTS_H_

#include <cstdint>

namespace connsvr
{
/// 转发到后端(rolesvr/roomsvr)的RPC默认超时(ms)
constexpr uint32_t kForwardTimeoutMs = 2000;
/// StartBattle涉及DS分配，链路更长，用更宽松的超时(ms)
constexpr uint32_t kStartBattleTimeoutMs = 3000;

/// 游客gid编码：高32位标记 + 低32位序列
constexpr uint64_t kGuestGidMarker = (1ULL << 32);
constexpr uint64_t kGuestLow32Mask = 0xFFFFFFFFULL;

/// 游客并发与限频阈值
constexpr uint32_t kGuestMaxOnline = 10000;
constexpr uint32_t kGuestIpRateLimitPerMin = 60;

/// 游客登录/资料相关错误码
constexpr int32_t kCodeGuestInvalidName = 1101;        // 空白/超长昵称
constexpr int32_t kCodeGuestSensitiveName = 1102;      // 昵称违规（保留位）
constexpr int32_t kCodeGuestLimitReached = 1103;       // 游客并发上限
constexpr int32_t kCodeGuestRateLimited = 1104;        // 单IP限频
constexpr int32_t kCodeDuplicateLogin = 1105;          // 本连接重复登录
constexpr int32_t kCodeGuestSetUserInfoForbidden = 1106; // 游客不可改资料

/// 账号资料错误码
constexpr int32_t kCodeInvalidUserName = 1301;         // 昵称trim后为空或超过8字

/// 后端不可达时回给客户端的错误码（业务无关的通用"服务暂不可用"）
constexpr int32_t kCodeBackendUnreachable = 1999;

}  // namespace connsvr

#endif
