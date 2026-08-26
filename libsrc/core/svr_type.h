/*
 * * file name: svr_type.h
 * * description: 服务类型常量定义，从busid高位(>>24)派生。
 * *              busid布局为 {group_base | instance_id}，group_base的高字节即为svr_type。
 * *              对齐ua_server的server_family设计，app_server当前使用5种服务类型。
 * */

#ifndef _APP_SVR_TYPE_H_
#define _APP_SVR_TYPE_H_

#include <cstdint>

namespace app
{
/// 服务类型常量，等于busid >> 24
constexpr uint32_t kSvrTypeEchoDemo = 1;   // 0x01000000
constexpr uint32_t kSvrTypeConnSvr  = 3;   // 0x03000000
constexpr uint32_t kSvrTypeRoleSvr  = 4;   // 0x04000000
constexpr uint32_t kSvrTypeDBProxy  = 5;   // 0x05000000
constexpr uint32_t kSvrTypeRoomSvr  = 6;   // 0x06000000
constexpr uint32_t kSvrTypeDsCenter = 7;   // 0x07000000
constexpr uint32_t kSvrTypeDsAgent  = 8;   // 0x08000000

/// TCM 部署时的群组地址（含 world=1），用于 RPC 路由寻址。
/// TCM 部署的 busid 格式为 X.1.0.N，对应的 group 为 X.1.0.0；
/// 而 kSvrTypeX << 24 = X.0.0.0（world=0），namesrv 无法路由到 world=1 的 endpoint。
/// sandbox 环境下 busid 为 world=0（如 0x06000001），使用 kSvrTypeX << 24 即可；
/// TCM 部署时使用 kGroupAddrX 寻址。
constexpr uint32_t kGroupAddrConnSvr  = 0x03010000;  // 3.1.0.0
constexpr uint32_t kGroupAddrRoleSvr  = 0x04010000;  // 4.1.0.0
constexpr uint32_t kGroupAddrDBProxy  = 0x05010000;  // 5.1.0.0
constexpr uint32_t kGroupAddrRoomSvr  = 0x06010000;  // 6.1.0.0
constexpr uint32_t kGroupAddrDsCenter = 0x07010000;  // 7.1.0.0
constexpr uint32_t kGroupAddrDsAgent  = 0x08010000;  // 8.1.0.0

/// 从busid提取服务类型（busid的高字节）
inline uint32_t SvrTypeFromBusid(uint32_t busid)
{
    return busid >> 24;
}

/// 从busid提取实例编号（低12位）
inline uint32_t InstFromBusid(uint32_t busid)
{
    return busid & 0xFFF;
}

}  // namespace app

#endif
