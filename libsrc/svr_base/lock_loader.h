/*
 * * file name: lock_loader.h
 * * description: 通用分布式锁加载器（移植自 ua_server libsrc/util/lock_loader.h 的简化版）。
 * *              DB 记录里存「锁持有者 busid + 时间戳」，本类封装「带锁加载」的重试/抢占流程：
 * *                - LoadFromDB 返回 kNotData → 无数据，直接返回（调用方新建）
 * *                - 返回 kLocked → 被他人持有 → KickLock 通知其存盘释放锁 → 重试一次
 * *                - 返回 0 → 拿到锁并加载成功
 * *              锁的过期与授予由 DB 侧(dbproxy CommonGetLockData)判定，本类只负责流程编排。
 * *              app_server 无 svr_version 维度，故 LockIdentity 仅含 bus_id。
 * */

#ifndef _APP_LOCK_LOADER_H_
#define _APP_LOCK_LOADER_H_

#include <cstdint>
#include <string>
#include "core/log.h"

namespace app
{

/// LockLoader 的加载结果约定（由子类 LoadFromDB 返回，与具体 DB 错误码解耦）
enum LockLoadResult : int
{
    kLockLoadOk = 0,       // 拿到锁并加载成功
    kLockLoadNotData = 1,  // DB 无该 key 的数据
    kLockLoadLocked = 2,   // 被其他实例持有且未过期（out_owner 返回持有者 busid）
    kLockLoadError = 3,    // 其他错误
};

template <typename T>
class LockLoader
{
public:
    LockLoader() = default;
    virtual ~LockLoader() = default;

    /// 带锁加载 key 对应的数据到 data。
    /// 返回：kLockLoadOk=成功持锁; kLockLoadNotData=无数据; 其他=失败。
    int LoadWithLock(const std::string& key, T* data)
    {
        uint32_t my_busid = MyBusId();

        uint32_t owner = 0;
        int ret = LoadFromDB(key, my_busid, data, &owner);

        if (ret == kLockLoadNotData)
            return kLockLoadNotData;

        if (ret == kLockLoadLocked)
        {
            APP_LOG_INFO(0, "key(%s) locked by busid(%u), my(%u), kick it", key.c_str(), owner, my_busid);
            int kret = KickLock(key, owner);
            if (kret != 0)
            {
                APP_LOG_ERROR(0, "kick lock failed, key(%s), ret(%d)", key.c_str(), kret);
                return kLockLoadError;
            }
            // 重试一次
            ret = LoadFromDB(key, my_busid, data, &owner);
            if (ret != kLockLoadOk)
            {
                APP_LOG_ERROR(0, "load after kick failed, key(%s), ret(%d)", key.c_str(), ret);
                return ret;
            }
        }
        else if (ret != kLockLoadOk)
        {
            APP_LOG_WARN(0, "load data failed, key(%s), ret(%d)", key.c_str(), ret);
            return ret;
        }

        return kLockLoadOk;
    }

protected:
    /// 从 DB 带锁读取。成功时把 my_busid 写入记录锁字段；被他人持有时 *out_owner 返回持有者 busid。
    /// 返回 LockLoadResult。
    virtual int LoadFromDB(const std::string& key, uint32_t my_busid, T* data, uint32_t* out_owner) = 0;
    /// 通知锁持有者(owner_busid)存盘并释放锁。
    virtual int KickLock(const std::string& key, uint32_t owner_busid) = 0;
    /// 本实例 busid（锁持有者身份）。
    virtual uint32_t MyBusId() = 0;
};

}  // namespace app

#endif
