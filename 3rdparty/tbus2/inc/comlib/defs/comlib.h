// Copyright (c) Tencent
// Author: bondshi
// Create: 2024-01-23

#ifndef COMLIB_DEFS_COMLIB_H_
#define COMLIB_DEFS_COMLIB_H_

// comlib/defs/comlib.h
#pragma once

#include <assert.h>
#include <signal.h>
#include <string.h>
#include <string>
#include "comdefs.h"
#include "errors.h"

////////////////////////////////////////////////////////////////////////////////

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MGSE_MAGIC_NUM 0x54425332  // TBS2
#else
#define MGSE_MAGIC_NUM 0x32534254  // TBS2
#endif

#define MGSE_MAKE_VERSION(major, minor, patch) ((major << 16) | (minor << 8) | patch)

#ifdef __GNUC__
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define PRAGMA_DIAG_PUSH _Pragma("GCC diagnostic push")
#define PRAGMA_DIAG_POP _Pragma("GCC diagnostic pop")
#define PRAGMA_DIAG_NO_CONVERSION _Pragma("GCC diagnostic ignored \"-Wconversion\"")
#else
#define likely(x) x
#define unlikely(x) x
#define PRAGMA_DIAG_PUSH
#define PRAGMA_DIAG_POP
#define PRAGMA_DIAG_NO_CONVERSION
#endif

#define i_sizeof(x) static_cast<int>(sizeof(x))
#define i_offsetof(type, x) static_cast<int>(offsetof(type, x))

#ifndef SIGHUP
#define SIGHUP 1
#endif

#ifndef SIGUSR1
#define SIGUSR1 10
#endif

#ifndef SIGUSR2
#define SIGUSR2 12
#endif

#ifdef OS_WINDOWS
#define random rand
#define strncasecmp strnicmp
#endif

#if !defined(OS_WINDOWS) && !defined(__PRI64_PREFIX)
// support compile on tlinux1.2
#define PRIu64 "lu"
#define PRId64 "ld"
#define PRIx64 "lx"
#define PRIu32 "u"
#define PRId32 "d"
#endif

MGSE_NS_BEGIN

////////////////////////////////////////////////////////////////////////////////

static inline void ParseVersion(uint32_t ver, int *major, int *minor, int *patch) {
  assert(major != NULL && minor != NULL && patch != NULL);
  *major = (ver & 0xFFFF0000U) >> 16;
  *minor = (ver & 0xFFFFU) >> 8;
  *patch = (ver & 0xFFU);
}

static inline std::string GetVersionStr(uint32_t ver) {
  int major, minor, patch;
  ParseVersion(ver, &major, &minor, &patch);
  char buf[32] = {0};
  snprintf(buf, sizeof(buf), "%d.%d.%d", major, minor, patch);
  return std::string(buf);
}

// response cmd = relative request cmd + 1000
const int kRpcReqAndResCmdDiff = 1000;

static inline int GetRpcResponseCmdByReq(int req_cmd) { return req_cmd + kRpcReqAndResCmdDiff; }

static inline int GetRpcRequestCmdByRes(int res_cmd) { return res_cmd - kRpcReqAndResCmdDiff; }

template <typename MsgType>
static inline void MakePbMsg(MsgType *msg, int cmd, uint32_t seqno) {
  assert(msg != NULL);
  msg->set_cmd(cmd);
  msg->set_seqno(seqno);
}

template <typename MsgType>
static inline void MakePbRes(MsgType *res, const MsgType &req) {
  MakePbMsg<MsgType>(res, GetRpcResponseCmdByReq(req.cmd()), req.seqno());
}

template <typename MsgType>
static inline int GetPbMsgCmd(const MsgType &msg) {
  return msg.cmd();
}

////////////////////////////////////////////////////////////////////////////////

MGSE_NS_END
#endif  // COMLIB_DEFS_COMLIB_H_
