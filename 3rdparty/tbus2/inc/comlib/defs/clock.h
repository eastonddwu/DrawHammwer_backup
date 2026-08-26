// Copyright (c) Tencent
// Author: bondshi
// Create: 2021-05-28

#ifndef COMLIB_DEFS_CLOCK_H_
#define COMLIB_DEFS_CLOCK_H_

// comlib/defs/clock.h
#pragma once

#include <time.h>
#include <string>
#include "comdefs.h"

////////////////////////////////////////////////////////////////////////////////

const int TIMESTAMP_SEC = 1000000;
const int TIMESTAMP_MS = 1000;

#ifdef OS_WINDOWS

#define MS_PER_SEC 1000ULL  // MS = milliseconds
#define US_PER_MS 1000ULL   // US = microseconds
#define HNS_PER_US 10ULL    // HNS = hundred-nanoseconds (e.g., 1 hns = 100 ns)
#define NS_PER_US 1000ULL

#define HNS_PER_SEC (MS_PER_SEC * US_PER_MS * HNS_PER_US)
#define NS_PER_HNS (100ULL)  // NS = nanoseconds
#define NS_PER_SEC (MS_PER_SEC * US_PER_MS * NS_PER_US)

static inline int clock_gettime_realtime(struct timespec *tv) {
  FILETIME ft;
  ULARGE_INTEGER hnsTime;

  GetSystemTimeAsFileTime(&ft);

  hnsTime.LowPart = ft.dwLowDateTime;
  hnsTime.HighPart = ft.dwHighDateTime;

  // To get POSIX Epoch as baseline,
  // subtract the number of hns intervals from Jan 1, 1601 to Jan 1, 1970.
  hnsTime.QuadPart -= (11644473600ULL * HNS_PER_SEC);

  // modulus by hns intervals per second first, then convert to ns, as not to lose resolution
  tv->tv_nsec = static_cast<time_t>((hnsTime.QuadPart % HNS_PER_SEC) * NS_PER_HNS);
  tv->tv_sec = static_cast<int32_t>(hnsTime.QuadPart / HNS_PER_SEC);

  return 0;
}

#endif

// realtime
static inline utimestamp_t GetTimeStampUs() {
  timespec tp;
#ifdef OS_WINDOWS
  int ret = clock_gettime_realtime(&tp);
  if (ret != 0) {
    return 0;
  }
#else
#ifdef OS_MAC
  int ret = clock_gettime(CLOCK_REALTIME, &tp);
#else
  int ret = clock_gettime(CLOCK_REALTIME_COARSE, &tp);
#endif
  if (ret != 0) {
    return 0;
  }
#endif

  return tp.tv_sec * 1000 * 1000 + tp.tv_nsec / 1000;
}
static inline time_t GetTimeStamp(utimestamp_t us) { return us / 1000000; }

// maybe system realtime changed
#define ADJUST_TIMESTAMP(t, now) \
  do {                           \
    if (t > now) t = now;        \
  } while (0)

// tick time
static inline uticktime_t GetTickUs() {
#ifdef OS_WINDOWS
  return GetTickCount64() * US_PER_MS;
#else
  timespec tp;
#ifdef OS_MAC
  int ret = clock_gettime(CLOCK_MONOTONIC, &tp);
#else
  int ret = clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);
#endif
  if (ret != 0) {
    return 0;
  }
  return static_cast<uticktime_t>(tp.tv_sec * 1000 * 1000 + tp.tv_nsec / 1000);
#endif
}
static inline uticktime_t GetTickSec(uticktime_t us) { return us / 1000000; }

static inline utimestamp_t GetTimestampFromTick(uticktime_t tick) {
  return GetTimeStampUs() - GetTickUs() + tick;
}

class SimpleTimer {
 public:
  SimpleTimer() {}
  SimpleTimer(utimestamp_t interval, utimestamp_t init_time) { Reset(interval, init_time); }

  bool CheckAndReset(utimestamp_t now) {
    if (interval_ == 0) {
      return false;
    }
    if (now >= expire_time_) {
      expire_time_ = now + interval_;
      return true;
    }

    // 修改时钟导致异常
    if ((expire_time_ - now) > (2 * interval_)) {
      expire_time_ = now + interval_;
    }

    return false;
  }

  void Reset(utimestamp_t interval, utimestamp_t now) {
    interval_ = interval;
    if (now > 0) {
      expire_time_ = now + interval;
    }
  }

 private:
  utimestamp_t interval_ = 0;
  utimestamp_t expire_time_ = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif  // COMLIB_DEFS_CLOCK_H_
