/*
 * * file name: metrics.cpp
 * * description: 性能埋点实现，见metrics.h说明
 * */

#include "metrics.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "runtime_config.h"
#include "utils.h"

namespace app
{
uint64_t LatencyHistogram::Percentile(double ratio) const
{
    if (count_ == 0)
        return 0;

    // 目标是"第ceil(count*ratio)个样本"（1-based）。必须向上取整：
    // 若用截断，990个1us+10个100000us的场景下P99会取到第990个样本(=1us)，
    // 完全漏掉长尾——而定位性能瓶颈恰恰最关心长尾。向上取整后取第991个，正确落入长尾桶。
    uint64_t target = static_cast<uint64_t>(std::ceil(static_cast<double>(count_) * ratio));
    if (target == 0)
        target = 1;
    if (target > count_)
        target = count_;

    uint64_t accum = 0;
    for (size_t i = 0; i < kBucketNum; ++i)
    {
        accum += buckets_[i];
        if (accum >= target)
        {
            // 桶i覆盖[2^(i-1), 2^i)，返回上界2^i；桶0代表0微秒
            if (i == 0)
                return 0;
            uint64_t upper = 1ULL << i;
            // 分位数不应超过实际观测到的最大值
            return std::min(upper, max_micro_);
        }
    }
    return max_micro_;
}

void LatencyHistogram::Reset()
{
    count_ = 0;
    total_micro_ = 0;
    max_micro_ = 0;
    for (size_t i = 0; i < kBucketNum; ++i)
        buckets_[i] = 0;
}

void Metrics::Init()
{
    std::string value = runtime_config::Get("APP_METRICS");
    enabled_ = (!value.empty() && (value[0] == '1' || value[0] == 'y' || value[0] == 'Y'));
    start_micro_ = utils::CurrentRealMicroSec();
}

void Metrics::OnServerRpc(uint32_t cmd, uint64_t micro_sec, bool timeout)
{
    if (!enabled_)
        return;
    auto&& stat = server_stats_[cmd];
    stat.latency.Add(micro_sec);
    if (timeout)
        ++stat.timeout_count;
}

void Metrics::OnClientRpc(uint32_t cmd, uint64_t micro_sec, bool timeout)
{
    if (!enabled_)
        return;
    auto&& stat = client_stats_[cmd];
    stat.latency.Add(micro_sec);
    if (timeout)
        ++stat.timeout_count;
}

void Metrics::OnRejected(uint32_t cmd)
{
    if (!enabled_)
        return;
    ++server_stats_[cmd].rejected_count;
}

void Metrics::OnFrame(uint64_t deal_pkg_num, uint64_t pkg_limit, size_t running_coro, size_t total_coro,
                      size_t max_coro)
{
    if (!enabled_)
        return;

    ++frame_count_;
    total_pkg_num_ += deal_pkg_num;
    if (pkg_limit > 0 && deal_pkg_num >= pkg_limit)
        ++busy_frame_count_;
    if (deal_pkg_num > max_pkg_in_frame_)
        max_pkg_in_frame_ = deal_pkg_num;
    if (running_coro > max_running_coro_)
        max_running_coro_ = running_coro;
    if (total_coro > max_total_coro_)
        max_total_coro_ = total_coro;
    max_coro_limit_ = max_coro;
}

void Metrics::DumpCmdSection(std::string& out, const char* title,
                             const std::unordered_map<uint32_t, CmdStat>& stats)
{
    if (stats.empty())
        return;

    char line[512];
    snprintf(line, sizeof(line),
             "  %s:\n    %-12s %10s %10s %10s %10s %10s %10s %10s %8s %8s\n", title, "cmd", "count",
             "avg_us", "p50_us", "p95_us", "p99_us", "p999_us", "max_us", "timeout", "reject");
    out += line;

    // 按调用次数降序，让最热的cmd排在最前面
    std::vector<const std::pair<const uint32_t, CmdStat>*> sorted;
    sorted.reserve(stats.size());
    for (const auto& item : stats)
        sorted.push_back(&item);
    std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b) {
        return a->second.latency.Count() > b->second.latency.Count();
    });

    for (const auto* item : sorted)
    {
        const auto& s = item->second;
        snprintf(line, sizeof(line),
                 "    0x%08X %10llu %10llu %10llu %10llu %10llu %10llu %10llu %8llu %8llu\n", item->first,
                 static_cast<unsigned long long>(s.latency.Count()),
                 static_cast<unsigned long long>(s.latency.AvgMicro()),
                 static_cast<unsigned long long>(s.latency.Percentile(0.50)),
                 static_cast<unsigned long long>(s.latency.Percentile(0.95)),
                 static_cast<unsigned long long>(s.latency.Percentile(0.99)),
                 static_cast<unsigned long long>(s.latency.Percentile(0.999)),
                 static_cast<unsigned long long>(s.latency.MaxMicro()),
                 static_cast<unsigned long long>(s.timeout_count),
                 static_cast<unsigned long long>(s.rejected_count));
        out += line;
    }
}

std::string Metrics::Dump() const
{
    if (!enabled_)
        return "metrics disabled (set APP_METRICS=1 to enable)";

    uint64_t now = utils::CurrentRealMicroSec();
    uint64_t elapsed_micro = now > start_micro_ ? now - start_micro_ : 1;
    double elapsed_sec = static_cast<double>(elapsed_micro) / 1000000.0;

    uint64_t total_req = 0;
    for (const auto& item : server_stats_)
        total_req += item.second.latency.Count();

    std::string out = "\n===== metrics =====\n";
    char line[512];

    snprintf(line, sizeof(line), "  window: %.2fs, server_req: %llu, qps: %.1f\n", elapsed_sec,
             static_cast<unsigned long long>(total_req),
             elapsed_sec > 0 ? static_cast<double>(total_req) / elapsed_sec : 0.0);
    out += line;

    snprintf(line, sizeof(line),
             "  loop: frames(%llu) pkgs(%llu) avg_pkg_per_frame(%.2f) max_pkg_in_frame(%llu) "
             "busy_frames(%llu, %.2f%%)\n",
             static_cast<unsigned long long>(frame_count_), static_cast<unsigned long long>(total_pkg_num_),
             frame_count_ ? static_cast<double>(total_pkg_num_) / static_cast<double>(frame_count_) : 0.0,
             static_cast<unsigned long long>(max_pkg_in_frame_),
             static_cast<unsigned long long>(busy_frame_count_),
             frame_count_ ? 100.0 * static_cast<double>(busy_frame_count_) / static_cast<double>(frame_count_)
                          : 0.0);
    out += line;

    snprintf(line, sizeof(line), "  coro: max_running(%zu) max_total(%zu) limit(%zu) usage(%.1f%%)\n",
             max_running_coro_, max_total_coro_, max_coro_limit_,
             max_coro_limit_ ? 100.0 * static_cast<double>(max_running_coro_) /
                                   static_cast<double>(max_coro_limit_)
                             : 0.0);
    out += line;

    DumpCmdSection(out, "server", server_stats_);
    DumpCmdSection(out, "client", client_stats_);
    out += "===================";
    return out;
}

void Metrics::Reset()
{
    server_stats_.clear();
    client_stats_.clear();
    frame_count_ = 0;
    total_pkg_num_ = 0;
    busy_frame_count_ = 0;
    max_pkg_in_frame_ = 0;
    max_running_coro_ = 0;
    max_total_coro_ = 0;
    start_micro_ = utils::CurrentRealMicroSec();
}

}  // namespace app
