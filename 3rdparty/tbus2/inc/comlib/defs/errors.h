// Copyright (c) Tencent
// Author: bondshi
// Create: 2024-01-24
// comlib errors

#ifndef COMLIB_DEFS_ERRORS_H_
#define COMLIB_DEFS_ERRORS_H_

// comlib/defs/errors.h
#pragma once

#include <map>
#include "comdefs.h"

MGSE_NS_BEGIN

////////////////////////////////////////////////////////////////////////////////

MGSE_API const char *GetErrorString(int e);

class MGSE_API ErrRegistry {
 public:
  typedef std::map<int, const char *> ErrMap;

  ErrRegistry() {}

  static ErrRegistry *Instance();
  static void RegErrors(std::initializer_list<ErrMap::value_type> init) {
    auto &errs = Instance()->errs_;
    errs.insert(init);
  }

  const char *GetErrStr(int e) const {
    static const char *kUnknownStr = "Unknown Error";
    auto it = errs_.find(e);
    if (it == errs_.end()) {
      return kUnknownStr;
    }

    return it->second;
  }

 private:
  ErrRegistry(const ErrRegistry &) = delete;
  ErrRegistry(ErrRegistry &&) = delete;

  ErrMap errs_;
};

class MGSE_API ErrRegHelper {
 public:
  ErrRegHelper(std::initializer_list<ErrRegistry::ErrMap::value_type> init) {
    ErrRegistry::RegErrors(init);
  }
};

#define MGSE_ERR_ITEM(e) \
  { e, #e }
#define MGSE_REG_ERRORS static MGSE_NS::ErrRegHelper g_errRegHelper

////////////////////////////////////////////////////////////////////////////////

MGSE_NS_END
#endif  // COMLIB_DEFS_ERRORS_H_
