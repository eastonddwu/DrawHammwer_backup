/*
 * * file name: log.h
 * * description: APP_LOG_*日志宏，底层接入tsf4g的tlog库（真正落地文件，而非stderr）。
 * *              LogService（log_service.h）负责tlog上下文的初始化/释放和category持有，
 * *              AppServer::Init会在最开始完成LogService::Init，之后才允许调用这些宏。
 * *              LogService未初始化（Category()为nullptr）时宏内部直接跳过，不会崩溃，
 * *              但也不会有任何输出，故务必确保初始化时机早于第一条日志。
 * */

#ifndef _APP_LOG_H_
#define _APP_LOG_H_

#include <tlog/tlog.h>
#include "log_service.h"

#define APP_LOG_IMPL(tlog_func, gid, fmt, ...) \
    do \
    { \
        LPTLOGCATEGORYINST app_log_cat_ = app::LogService::GetInst().Category(); \
        if (app_log_cat_) \
            tlog_func(app_log_cat_, 0, 0, "[gid=%lu][%s:%d] " fmt, static_cast<unsigned long>(gid), __FILE__, \
                      __LINE__, ##__VA_ARGS__); \
    } while (0)

#define APP_LOG_TRACE(gid, fmt, ...) APP_LOG_IMPL(tlog_trace, gid, fmt, ##__VA_ARGS__)
#define APP_LOG_DEBUG(gid, fmt, ...) APP_LOG_IMPL(tlog_debug, gid, fmt, ##__VA_ARGS__)
#define APP_LOG_INFO(gid, fmt, ...) APP_LOG_IMPL(tlog_info, gid, fmt, ##__VA_ARGS__)
#define APP_LOG_WARN(gid, fmt, ...) APP_LOG_IMPL(tlog_warn, gid, fmt, ##__VA_ARGS__)
#define APP_LOG_ERROR(gid, fmt, ...) APP_LOG_IMPL(tlog_error, gid, fmt, ##__VA_ARGS__)

#endif
