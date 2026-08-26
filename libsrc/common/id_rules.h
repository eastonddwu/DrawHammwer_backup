/*
 * * file name: id_rules.h
 * * description: 全局唯一ID编码规则（对齐ua_server的id_rules设计）。
 * *              把「归属实例」编码进ID高位，任何服务持有该ID即可用GetBusidFromGid
 * *              反解出归属实例的busid，实现无中心目录的确定性路由。
 * *              当前用于roomsvr的room_id：同一房间的所有操作据此路由到持有该房间的实例。
 * */

#ifndef _APP_ID_RULES_H_
#define _APP_ID_RULES_H_

#include <cstdint>
#include "core/svr_type.h"

namespace app
{

// 64-bit 全局唯一ID布局：
//   bits [63..44] reserved (20b, 恒0)
//   bits [43..36] world_id (8b)   —— 对应busid字节 (busid>>16)&0xFF
//   bits [35..24] inst_id  (12b)  —— 对应busid低12位 (busid & 0xFFF)
//   bits [23..0]  local_seq(24b)  —— 每进程自增序号（>=1，约1677万/进程生命周期）
constexpr uint32_t kIdSeqBits    = 24;
constexpr uint32_t kIdInstBits   = 12;
constexpr uint32_t kIdWorldBits  = 8;
constexpr uint32_t kIdInstShift  = kIdSeqBits;                 // 24
constexpr uint32_t kIdWorldShift = kIdSeqBits + kIdInstBits;   // 36
constexpr uint64_t kIdSeqMask    = (1ULL << kIdSeqBits) - 1;

/// 用本进程的 world/inst + 本地自增序号生成全局唯一ID
inline uint64_t GenerateGlobalUniqueId(uint32_t world_id, uint32_t inst_id, uint64_t local_seq)
{
    return (static_cast<uint64_t>(world_id & 0xFF) << kIdWorldShift) |
           (static_cast<uint64_t>(inst_id & 0xFFF) << kIdInstShift) |
           (local_seq & kIdSeqMask);
}

/// 从编码ID中反解出归属实例的busid（格式 svr_type.world.0.inst，如 6.1.0.N = 0x0601000N）。
/// id==0 或未编码实例(inst==0)时返回0，调用方应回退到哈希路由。
inline uint32_t GetBusidFromGid(uint64_t id, uint32_t svr_type)
{
    if (id == 0)
        return 0;
    uint32_t world = static_cast<uint32_t>((id >> kIdWorldShift) & 0xFF);
    uint32_t inst = static_cast<uint32_t>((id >> kIdInstShift) & 0xFFF);
    if (inst == 0)
        return 0;
    return (svr_type << 24) | (world << 16) | inst;
}

/// roomsvr 便捷封装：从 room_id 反解归属 roomsvr 实例 busid
inline uint32_t GetBusidFromRoomId(uint64_t room_id)
{
    return GetBusidFromGid(room_id, kSvrTypeRoomSvr);
}

}  // namespace app

#endif
