/*
 * * file name: timeout_queue.cpp
 * * description: ...
 * */

#include "timeout_queue.h"
#include "log.h"

namespace app
{
uint32_t TimeoutQueue::Add(Task task, uint64_t expire_time, uint32_t interval_time)
{
    uint32_t new_id = GenerateID();
    if (!timer_id_index_.insert({new_id, expire_time}).second)
    {
        APP_LOG_ERROR(0, "timer_id_index_ insert new_id(%u) error", new_id);
        return 0;
    }

    if (!timer_queue_.insert({new_id, interval_time, expire_time, std::move(task)}).second)
    {
        timer_id_index_.erase(new_id);
        APP_LOG_ERROR(0, "timer_queue_ insert new_id(%u) error", new_id);
        return 0;
    }

    return new_id;
}

bool TimeoutQueue::Cancel(uint32_t timer_id)
{
    auto iter = timer_id_index_.find(timer_id);
    if (iter == timer_id_index_.end())
    {
        APP_LOG_DEBUG(0, "can not found timer_id(%u)", timer_id);
        return false;
    }

    timer_queue_.erase({timer_id, 0, iter->second, nullptr});
    timer_id_index_.erase(iter);
    return true;
}

uint32_t TimeoutQueue::TimeOut(uint64_t now)
{
    uint32_t count = 0;
    while (!timer_queue_.empty())
    {
        auto iter = timer_queue_.begin();
        if (iter->expire_time > now)
            break;

        // 拷贝一份出来后先删除定时器，防止task中有逻辑对这个定时器有操作
        Timer tmp = (*iter);
        timer_id_index_.erase(iter->seq_id);
        timer_queue_.erase(iter);

        // 是循环定时器，重新加进去
        if (tmp.interval_time > 0)
        {
            tmp.expire_time += tmp.interval_time;

            if (timer_id_index_.insert({tmp.seq_id, tmp.expire_time}).second)
            {
                if (!timer_queue_.insert(tmp).second)
                {
                    timer_id_index_.erase(tmp.seq_id);
                    APP_LOG_ERROR(0, "timer_queue_ insert interval id(%u) error", tmp.seq_id);
                }
            }
            else
            {
                APP_LOG_ERROR(0, "timer_id_index_ insert interval id(%u) error", tmp.seq_id);
            }
        }

        tmp.task(tmp.seq_id, tmp.interval_time);
        ++count;
    }

    return count;
}

bool TimeoutQueue::Exist(uint32_t timer_id) const
{
    return timer_id_index_.find(timer_id) != timer_id_index_.end();
}

void TimeoutQueue::Clear()
{
    timer_queue_.clear();
    timer_id_index_.clear();
}

uint32_t TimeoutQueue::GenerateID()
{
    ++base_id_;
    if (base_id_ == 0)
        ++base_id_;
    return base_id_;
}

}  // namespace app
