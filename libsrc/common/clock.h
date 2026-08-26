/*
 * * file name: clock.h
 * * description: 全局时钟缓存，避免频繁调用gettimeofday
 * */

#ifndef _APP_CLOCK_H_
#define _APP_CLOCK_H_

#include <cstdint>
#include "patterns/singleton.h"

namespace app
{
class Clock : public Singleton<Clock>
{
public:
    /// 获取当前时间（秒）
    uint64_t CurrentSec() const { return micro_sec_ / 1000000; }
    /// 获取当前时间（毫秒）
    uint64_t CurrentMilliSec() const { return micro_sec_ / 1000; }
    /// 获取当前时间（微秒）
    uint64_t CurrentMicroSec() const { return micro_sec_; }
    /// 刷新当前时间
    void Update(uint64_t micro_sec) { micro_sec_ = micro_sec; }

private:
    friend class Singleton<Clock>;
    uint64_t micro_sec_ = 0;
};

}  // namespace app

#endif
