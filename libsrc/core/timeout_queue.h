/*
 * * file name: timeout_queue.h
 * * description: 基于有序集合的定时器队列，主要用于RPC超时管理（频繁取消，故不用优先队列）
 * */

#ifndef _APP_TIMEOUT_QUEUE_H_
#define _APP_TIMEOUT_QUEUE_H_

#include <cstdint>
#include <functional>
#include <set>
#include <unordered_map>

namespace app
{
class TimeoutQueue
{
public:
    using Task = std::function<void(uint32_t timer_id, uint32_t interval_time)>;
    /// 添加定时器，返回一个唯一的定时器ID，interval_time如果是0则表示只执行一次
    uint32_t Add(Task task, uint64_t expire_time, uint32_t interval_time = 0);
    /// 取消定时器
    bool Cancel(uint32_t timer_id);
    /// 处理所有比now超时的timer，返回处理了多少个
    uint32_t TimeOut(uint64_t now);
    /// 判断定时器是否存在
    bool Exist(uint32_t timer_id) const;
    /// 清空所有定时器
    void Clear();

private:
    uint32_t GenerateID();

private:
    struct Timer
    {
        /// 递增的唯一序号
        uint32_t seq_id = 0;
        /// 循环重复定时器间隔时间ms
        uint32_t interval_time = 0;
        /// 到期时间ms
        uint64_t expire_time = 0;
        /// 要执行的任务
        Task task;

        friend bool operator<(const Timer& left, const Timer& right)
        {
            if (left.expire_time != right.expire_time)
                return left.expire_time < right.expire_time;
            else
                return left.seq_id < right.seq_id;
        }
    };

    std::set<Timer> timer_queue_;
    /// seq id到expire_time的映射
    std::unordered_map<uint32_t, uint64_t> timer_id_index_;
    /// timer id 累加器
    uint32_t base_id_ = 0;
};

}  // namespace app

#endif
