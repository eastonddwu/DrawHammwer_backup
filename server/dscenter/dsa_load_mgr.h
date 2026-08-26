/*
 * * file name: dsa_load_mgr.h
 * * description: DSA负载管理器(Singleton)，跟踪所有DSA节点的负载信息，
 * *              提供最简负载均衡分配（选负载最低的DSA）
 */

#ifndef _DSA_LOAD_MGR_H_
#define _DSA_LOAD_MGR_H_

#include <cstdint>
#include <unordered_map>
#include "patterns/singleton.h"

namespace dscenter
{

struct DsaLoadInfo
{
    uint32_t ds_count = 0;       // 当前运行DS数量
    uint32_t max_ds_count = 100; // 最大DS容量
    uint64_t last_report_ts = 0; // 上次上报时间（毫秒）
};

class DsaLoadMgr : public app::Singleton<DsaLoadMgr>
{
public:
    /// 分配负载最低的DSA，返回dsa_svr_id(busid)，无可用DSA返回0
    uint32_t AllocDsa();

    /// 更新DSA负载信息
    void UpdateLoad(uint32_t dsa_svr_id, uint32_t ds_count, uint32_t max_ds_count);

    /// 定时清理过期DSA条目（60秒无上报）
    void OnTick(uint64_t now_ms);

    static constexpr uint64_t kDsaExpireMs = 60000; // DSA超时时间

private:
    friend class app::Singleton<DsaLoadMgr>;
    DsaLoadMgr() = default;

    std::unordered_map<uint32_t, DsaLoadInfo> dsa_load_map_;
};

}  // namespace dscenter

#endif
