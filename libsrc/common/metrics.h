/*
 * * file name: metrics.h
 * * description: 轻量性能埋点，用于压测时定位瓶颈。设计约束：
 * *              1) 热路径零分配、零字符串操作，单次记录只做「读时钟 + 数组下标自增」；
 * *              2) 时间一律取utils::CurrentRealMicroSec()而非Clock缓存值——Clock每帧只更新一次
 * *                 且精度是毫秒，同一帧内完成的RPC会被算成0耗时，无法反映真实分布；
 * *              3) 默认关闭，通过环境变量APP_METRICS=1开启，不开启时Enabled()为false，
 * *                 调用点被编译期内联成一次bool判断，对生产路径几乎无影响。
 * *
 * *              耗时分布用对数分桶（底为2的微秒桶），而不是保存全部样本：
 * *              内存恒定（每个cmd约40个uint64），且P50/P95/P99对定位瓶颈足够。
 * *              桶i covers [2^(i-1), 2^i) 微秒，桶0为0微秒。
 * */

#ifndef _APP_METRICS_H_
#define _APP_METRICS_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "patterns/singleton.h"

namespace app
{
/// 单个指标项的耗时分布统计
class LatencyHistogram
{
public:
    /// 桶数量：2^39微秒约等于6.4天，足够覆盖任何RPC耗时
    static constexpr size_t kBucketNum = 40;

    void Add(uint64_t micro_sec)
    {
        ++count_;
        total_micro_ += micro_sec;
        if (micro_sec > max_micro_)
            max_micro_ = micro_sec;
        ++buckets_[BucketIndex(micro_sec)];
    }

    uint64_t Count() const { return count_; }
    uint64_t TotalMicro() const { return total_micro_; }
    uint64_t MaxMicro() const { return max_micro_; }
    uint64_t AvgMicro() const { return count_ ? total_micro_ / count_ : 0; }

    /// 取分位数对应的桶上界（微秒）。ratio取值0~1，如0.99表示P99。
    /// 返回的是桶上界而非精确值，量级正确即可满足定位瓶颈的需要。
    uint64_t Percentile(double ratio) const;

    void Reset();

private:
    static size_t BucketIndex(uint64_t micro_sec)
    {
        if (micro_sec == 0)
            return 0;
        // 64 - clz 得到最高有效位序号，即 floor(log2(x)) + 1
        size_t idx = 64 - static_cast<size_t>(__builtin_clzll(micro_sec));
        return idx < kBucketNum ? idx : kBucketNum - 1;
    }

    uint64_t count_ = 0;
    uint64_t total_micro_ = 0;
    uint64_t max_micro_ = 0;
    uint64_t buckets_[kBucketNum] = {0};
};

/// 性能指标汇总，进程内单例。非线程安全（框架是单线程reactor，无需加锁）。
class Metrics : public Singleton<Metrics>
{
public:
    /// 读取环境变量APP_METRICS决定是否开启，并记录统计起始时间。
    /// 未调用Init时Enabled()恒为false，所有记录接口直接返回。
    void Init();

    bool Enabled() const { return enabled_; }

    /// 记录一次服务端请求处理完成（cmd为协议命令字，micro_sec为处理总耗时）
    void OnServerRpc(uint32_t cmd, uint64_t micro_sec, bool timeout);
    /// 记录一次客户端发起的RPC完成
    void OnClientRpc(uint32_t cmd, uint64_t micro_sec, bool timeout);
    /// 记录一次请求因协程池耗尽等原因被拒绝
    void OnRejected(uint32_t cmd);
    /// 记录一帧主循环处理的包数与协程池水位。
    /// pkg_limit为单帧收包上限（ServerCore的max_deal_pkg_num），用于统计有多少帧被打满——
    /// busy帧占比高说明收包能力已成为瓶颈，队列在堆积。
    void OnFrame(uint64_t deal_pkg_num, uint64_t pkg_limit, size_t running_coro, size_t total_coro,
                 size_t max_coro);

    /// 把当前统计结果格式化成多行文本（人可读），用于定期打日志
    std::string Dump() const;
    /// 清空所有统计并把起始时间重置为当前
    void Reset();

private:
    friend Singleton<Metrics>;
    Metrics() = default;
    ~Metrics() = default;

    struct CmdStat
    {
        LatencyHistogram latency;
        uint64_t timeout_count = 0;
        uint64_t rejected_count = 0;
    };

    static void DumpCmdSection(std::string& out, const char* title,
                               const std::unordered_map<uint32_t, CmdStat>& stats);

    bool enabled_ = false;
    uint64_t start_micro_ = 0;

    std::unordered_map<uint32_t, CmdStat> server_stats_;
    std::unordered_map<uint32_t, CmdStat> client_stats_;

    // 主循环水位
    uint64_t frame_count_ = 0;
    uint64_t total_pkg_num_ = 0;
    uint64_t busy_frame_count_ = 0;  // 处理包数达到上限的帧数，说明单帧被打满
    uint64_t max_pkg_in_frame_ = 0;
    size_t max_running_coro_ = 0;
    size_t max_total_coro_ = 0;
    size_t max_coro_limit_ = 0;
};

}  // namespace app

#endif
