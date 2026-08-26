// Copyright (c) Tencent
// Author: bondshi
// Create: 2021-05-26
// Encoding: utf-8

#ifndef COMLIB_UTILS_LOGGING_H_
#define COMLIB_UTILS_LOGGING_H_

// comlib/utils/logging.h
#pragma once

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <utility>
#include "comlib/defs/clock.h"
#include "comlib/defs/comlib.h"

#ifdef EXPORT_SHARED_LIBRARY
#undef WITH_GLOG
#endif

#ifdef WITH_GLOG
#ifdef OS_WINDOWS
#define GLOG_NO_ABBREVIATED_SEVERITIES
#endif

#include "glog/logging.h"
#endif

////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

#define MGSE_MAX_LOG_DIR_SIZE 255
#define MGSE_MAX_LOG_CONTENT_SIZE 4096
#define MGSE_DEFAULT_ASYNC_LOG_BUF_SIZE (10 * 1024 * 1024)
#define MGSE_DEFAULT_ASYNC_LOG_MAX_TRY_NUM 5

enum { MGSE_LOGTYPE_DEFAULT = 0, MGSE_LOGTYPE_SYNC, MGSE_LOGTYPE_ASYNC };
struct mgse_log_conf_t {
  char log_dir[MGSE_MAX_LOG_DIR_SIZE + 1];  // save log dir
  bool is_utc_time;
  int log_level;
  int log_type;               // ASYCN or SYNC
  int async_log_buf_size;     // in bytes, 0 use default value
  int async_log_max_try_num;  // 0 use default value, -1 always try util success
};

void mgse_logging_init(const char *prog, const mgse_log_conf_t *param);
void mgse_logging_fini();
// if wait_ms < 0, then wait until buffer is cleared
void mgse_logging_flush(int wait_ms);

void mgse_logging_set_printer(mgse_logging_printer_t printer);
// 暂时只在异步模式下生效，由外部拼接日志内容
void mgse_logging_set_formatter(mgse_logging_formatter_t formatter);

void mgse_logging_set_level(int level);
int mgse_logging_get_level();
const mgse_log_conf_t *mgse_logging_get_conf();

void mgse_logging_log(int level, const mgse_logging_context_t &ctx, const char *fmt, ...)
    MGSE_FUNC_ATTR((__format__(__printf__, 3, 4)));
void mgse_logging_vlog(int level, const mgse_logging_context_t &ctx, const char *fmt, va_list va);

void mgse_logging_log_mod(const char *module, int level, const char *fmt, ...)
    MGSE_FUNC_ATTR((__format__(__printf__, 3, 4)));
void mgse_logging_vlog_mod(const char *module, int level, const char *fmt, va_list va);

void mgse_logging_dump_bytes(int level, const void *buf, int dump_size);
inline void mgse_debug_dump_bytes(const void *buf, int size) {
  mgse_logging_dump_bytes(MGSE_LL_DEBUG, buf, size);
}
#ifdef __cplusplus
}
#endif

static inline bool mgse_logging_can_log(int level) { return (level >= mgse_logging_get_level()); }

static inline const char *mgse_logging_basename(const char *filename) {
  const char *base = strrchr(filename, '/');
#ifdef OS_WINDOWS
  if (base == NULL) {
    base = strrchr(filename, '\\');
  }
#endif
  return base ? base + 1 : filename;
}

static inline const mgse_logging_context_t mgse_logging_context(const char *func,
                                                                const char *filename, int line) {
  mgse_logging_context_t ctx = {0};
  ctx.func = func;
  ctx.file = mgse_logging_basename(filename);
  ctx.line = line;

  const int kOneSecUs = 1000000;
  auto now = GetTimeStampUs();
  auto secs = static_cast<time_t>(now / kOneSecUs);
  ctx.log_time_usec = static_cast<int>(now % kOneSecUs);

  auto logconf = mgse_logging_get_conf();
  assert(logconf != nullptr);
  if (unlikely(logconf->is_utc_time)) {
#ifdef OS_WINDOWS
    gmtime_s(&ctx.log_time_tm, &secs);
#else
    gmtime_r(&secs, &ctx.log_time_tm);
#endif
  } else {
#ifdef OS_WINDOWS
    localtime_s(&ctx.log_time_tm, &secs);
#else
    localtime_r(&secs, &ctx.log_time_tm);
#endif
  }

  return ctx;
}

#define _MAKE_LOG_CTX() mgse_logging_context(__FUNCTION__, __FILE__, __LINE__)

#define _MAKE_PLOG_FMT(fmt) "[%d:%s]" fmt
#define _MAKE_PLOG_COMMON_ARGS() errno, strerror(errno)

#ifdef _MSC_VER
// MSVC
#define MGSE_LOG(level, fmt, ...)                                 \
  if (mgse_logging_can_log(level)) {                              \
    mgse_logging_log(level, _MAKE_LOG_CTX(), fmt, ##__VA_ARGS__); \
  }

#define MGSE_PLOG(level, fmt, ...)                                                          \
  if (mgse_logging_can_log(level)) {                                                        \
    mgse_logging_log(level, _MAKE_LOG_CTX(), _MAKE_PLOG_FMT(fmt), _MAKE_PLOG_COMMON_ARGS(), \
                     ##__VA_ARGS__);                                                        \
  }

#define MGSE_LOG_DEBUG(fmt, ...) MGSE_LOG(MGSE_LL_DEBUG, fmt, ##__VA_ARGS__)
#define MGSE_LOG_INFO(fmt, ...) MGSE_LOG(MGSE_LL_INFO, fmt, ##__VA_ARGS__)
#define MGSE_LOG_WARN(fmt, ...) MGSE_LOG(MGSE_LL_WARN, fmt, ##__VA_ARGS__)
#define MGSE_LOG_ERROR(fmt, ...) MGSE_LOG(MGSE_LL_ERROR, fmt, ##__VA_ARGS__)
#define MGSE_LOG_FATAL(fmt, ...) MGSE_LOG(MGSE_LL_FATAL, fmt, ##__VA_ARGS__)

#define MGSE_PLOG_DEBUG(fmt, ...) MGSE_PLOG(MGSE_LL_DEBUG, fmt, ##__VA_ARGS__)
#define MGSE_PLOG_INFO(fmt, ...) MGSE_PLOG(MGSE_LL_INFO, fmt, ##__VA_ARGS__)
#define MGSE_PLOG_WARN(fmt, ...) MGSE_PLOG(MGSE_LL_WARN, fmt, ##__VA_ARGS__)
#define MGSE_PLOG_ERROR(fmt, ...) MGSE_PLOG(MGSE_LL_ERROR, fmt, ##__VA_ARGS__)
#define MGSE_PLOG_FATAL(fmt, ...) MGSE_PLOG(MGSE_LL_FATAL, fmt, ##__VA_ARGS__)

#else
// GCC
#define MGSE_LOG(level, fmt, args...)                      \
  if (mgse_logging_can_log(level)) {                       \
    mgse_logging_log(level, _MAKE_LOG_CTX(), fmt, ##args); \
  }

#define MGSE_PLOG(level, fmt, args...)                                                      \
  if (mgse_logging_can_log(level)) {                                                        \
    mgse_logging_log(level, _MAKE_LOG_CTX(), _MAKE_PLOG_FMT(fmt), _MAKE_PLOG_COMMON_ARGS(), \
                     ##args);                                                               \
  }

#define MGSE_LOG_DEBUG(fmt, args...) MGSE_LOG(MGSE_LL_DEBUG, fmt, ##args)
#define MGSE_LOG_INFO(fmt, args...) MGSE_LOG(MGSE_LL_INFO, fmt, ##args)
#define MGSE_LOG_WARN(fmt, args...) MGSE_LOG(MGSE_LL_WARN, fmt, ##args)
#define MGSE_LOG_ERROR(fmt, args...) MGSE_LOG(MGSE_LL_ERROR, fmt, ##args)
#define MGSE_LOG_FATAL(fmt, args...) MGSE_LOG(MGSE_LL_FATAL, fmt, ##args)

#define MGSE_PLOG_DEBUG(fmt, args...) MGSE_PLOG(MGSE_LL_DEBUG, fmt, ##args)
#define MGSE_PLOG_INFO(fmt, args...) MGSE_PLOG(MGSE_LL_INFO, fmt, ##args)
#define MGSE_PLOG_WARN(fmt, args...) MGSE_PLOG(MGSE_LL_WARN, fmt, ##args)
#define MGSE_PLOG_ERROR(fmt, args...) MGSE_PLOG(MGSE_LL_ERROR, fmt, ##args)
#define MGSE_PLOG_FATAL(fmt, args...) MGSE_PLOG(MGSE_LL_FATAL, fmt, ##args)

#endif  // _MSC_VER

#ifndef LOG_DEBUG
#define LOG_DEBUG MGSE_LOG_DEBUG
#endif

#ifndef LOG_INFO
#define LOG_INFO MGSE_LOG_INFO
#endif

#ifndef LOG_WARN
#define LOG_WARN MGSE_LOG_WARN
#endif

#ifndef LOG_ERROR
#define LOG_ERROR MGSE_LOG_ERROR
#endif

#ifndef LOG_FATAL
#define LOG_FATAL MGSE_LOG_FATAL
#endif

#ifndef PLOG_DEBUG
#define PLOG_DEBUG MGSE_PLOG_DEBUG
#endif

#ifndef PLOG_INFO
#define PLOG_INFO MGSE_PLOG_INFO
#endif

#ifndef PLOG_WARN
#define PLOG_WARN MGSE_PLOG_WARN
#endif

#ifndef PLOG_ERROR
#define PLOG_ERROR MGSE_PLOG_ERROR
#endif

#ifndef PLOG_FATAL
#define PLOG_FATAL MGSE_PLOG_FATAL
#endif

////////////////////////////////////////////////////////////////////////////////

#endif  // COMLIB_UTILS_LOGGING_H_
