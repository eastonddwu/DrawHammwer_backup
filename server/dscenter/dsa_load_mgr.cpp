/*
 * * file name: dsa_load_mgr.cpp
 * * description: DsaLoadMgr实现，见dsa_load_mgr.h说明
 */

#include "dsa_load_mgr.h"
#include "core/log.h"
#include <algorithm>

namespace dscenter
{

uint32_t DsaLoadMgr::AllocDsa()
{
    if (dsa_load_map_.empty())
        return 0;

    // 选择负载率最低的DSA（ds_count / max_ds_count 最小）
    uint32_t best_dsa = 0;
    double best_ratio = 1.0; // 最大负载率为1.0（100%）

    for (const auto& [dsa_id, load] : dsa_load_map_)
    {
        if (load.ds_count >= load.max_ds_count)
            continue; // 已满，跳过

        double ratio = static_cast<double>(load.ds_count) / std::max(load.max_ds_count, 1u);
        if (ratio < best_ratio)
        {
            best_ratio = ratio;
            best_dsa = dsa_id;
        }
    }

    return best_dsa;
}

void DsaLoadMgr::UpdateLoad(uint32_t dsa_svr_id, uint32_t ds_count, uint32_t max_ds_count)
{
    auto& info = dsa_load_map_[dsa_svr_id];
    info.ds_count = ds_count;
    info.max_ds_count = max_ds_count;
    info.last_report_ts = 0; // 由OnTick更新
}

void DsaLoadMgr::OnTick(uint64_t now_ms)
{
    // 更新所有活跃条目的last_report_ts
    // 注意：UpdateLoad时已更新数据，这里只清理过期条目
    // 由于我们不在UpdateLoad中设置时间戳（避免依赖时钟），
    // 暂时不清理，依赖DSA的持续上报来维持活跃状态
    // TODO: 如果需要过期清理，在UpdateLoad中记录时间戳
}

}  // namespace dscenter
