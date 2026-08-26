// Copyright (c) Tencent
// Author: bondshi
// Create: 2024-01-23

#ifndef COMLIB_DEFS_COMDEFS_H_
#define COMLIB_DEFS_COMDEFS_H_

// comlib/defs/comdefs.h
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

////////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32) || defined(_WIN64)
#define OS_WINDOWS
#elif defined(__APPLE__)
#define OS_MAC
#else
#define OS_LINUX
#endif

#define MGSE_API

// PY_CFFI_START

typedef uint64_t tbuspp_id_t;   // network byte order, busid saved in lower 32 bits
typedef uint64_t utimestamp_t;  // unit: us
typedef uint64_t uticktime_t;   // unit: us

enum { MGSE_LL_DEBUG = 0, MGSE_LL_INFO, MGSE_LL_WARN, MGSE_LL_ERROR, MGSE_LL_FATAL };

struct mgse_logging_context_t {
  const char *func;
  const char *file;
  int line;
  union {
    int64_t udata_i;
    void *udata_p;
  };
  // 记录时间
  struct tm log_time_tm;
  int log_time_usec;
};

#define MGSE_LOG_RAW_TEXT_LN -1

// PY_CFFI_END
#define MGSE_CURRENT_LOG_CTX \
  { __func__, __FILE__, __LINE__, {0}, {0}, 0 }
// PY_CFFI_START

typedef struct mgse_logging_context_t mgse_logging_context_t;
typedef void (*mgse_logging_printer_t)(int log_level, const char *data, int size,
                                       const mgse_logging_context_t *ctx);
typedef size_t (*mgse_logging_formatter_t)(char *data_buf, size_t buf_size, int log_level,
                                           const mgse_logging_context_t *ctx, const char *msg,
                                           size_t msg_size);

// PY_CFFI_END

#ifdef __cplusplus
extern "C" {
#endif

// PY_CFFI_START

// customer log output
MGSE_API void mgse_logging_set_printer(mgse_logging_printer_t printer);
MGSE_API void mgse_logging_set_printer_with_lock(bool enable);
// level: MGSE_LL_*
MGSE_API void mgse_logging_set_level(int level);
// return current log level in api
MGSE_API int mgse_logging_get_level();

// PY_CFFI_END

#ifdef __cplusplus
}
#endif

#ifdef OS_WINDOWS

#ifndef MGSE_DEFINED_IOV
#define MGSE_DEFINED_IOV
struct iovec {
  void *iov_base;
  size_t iov_len;
};
#endif

#else
#include <sys/uio.h>
#endif

// PY_CFFI_START

// comlib error: [-500, -999]
#define MGSE_ERR_GENERIC -1
#define MGSE_ERR_WRONG_ARG -10
#define MGSE_ERR_NOT_FOUND -11
#define MGSE_ERR_TIMEOUT -13

#define MGSE_ERR_WRONG_GIDMASK -15
#define MGSE_ERR_SHM_FAILED -16

#define MGSE_ERR_PKG_MAGIC -500
#define MGSE_ERR_PKG_SIZE -501
#define MGSE_ERR_CRYPTOR -502

// tbuspp2 definitions
#define TBUSPP_DOMAIN_LOCAL 0
// max alias size
#define TBUSPP_ALIAS_MAX_SIZE 127

// PY_CFFI_END

#ifdef OS_WINDOWS
#define __ORDER_BIG_ENDIAN__ 4321
#define __ORDER_LITTLE_ENDIAN__ 1234
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#endif

#ifdef __GNUC__
#define MGSE_FUNC_ATTR(x) __attribute__(x)
#else
#define MGSE_FUNC_ATTR(x)
#endif

#ifdef OS_WINDOWS
#define _WINSOCKAPI_  // suppress include <winsock.h>
#define NOMINMAX      // suppress global min/max macro define
#include <Windows.h>
#endif

#ifdef __cplusplus
#define MGSE_NS mgse
#define MGSE_NS_BEGIN namespace MGSE_NS {
#define MGSE_NS_END };

#define MGSE_NS_USING using namespace MGSE_NS;  // NOLINT
#endif

#endif  // COMLIB_DEFS_COMDEFS_H_
