// Copyright (c) Tencent
// Author: roypang
// Create: 2025-04-16
// Note: The features provided in this header requires C++14 or above

#ifndef COMLIB_STATS_STATS_COMM_H_
#define COMLIB_STATS_STATS_COMM_H_

// comlib/stats/stats_comm.h

#pragma once

#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include "comlib/busid/busid_ops.h"
#include "comlib/defs/comdefs.h"

MGSE_NS_BEGIN

// Tuple for storing label values of different types
template <typename... Types>
using LabelValues = std::tuple<Types...>;

// Vector for storing label key strings
using LabelKeys = std::vector<std::string>;

// Vector for storing like border values
template <typename ValType>
using ExtraValues = std::vector<ValType>;

/**
 * Check if the metric name is valid according to Prometheus naming conventions
 * https://prometheus.io/docs/concepts/data_model/
 * The metric name must follow the regex pattern: [a-zA-Z_:][a-zA-Z0-9_:]*
 */
MGSE_API bool CheckMetricName(const std::string &name);

/**
 * Check if the label name is valid
 * https://prometheus.io/docs/concepts/data_model/
 * The label name regex is "[a-zA-Z_][a-zA-Z0-9_]*"
 */
MGSE_API bool CheckLabelName(const std::string &name, int type);

/**
 * 获取所有StatsObj的个数
 */
MGSE_API size_t GetStatsObjNum();

/**
 * 通过Label的K/V构建Promenade的Dimension，返回格式：src=\"1.0\",dest=\"2.0\"
 */

template <typename LabelValuesType, typename LabelStringlizer>
MGSE_API std::string GetLabelDimensionStr(const LabelKeys &label_keys,
                                          const LabelValuesType &label_values) {
  assert(label_keys.size() == std::tuple_size_v<LabelValuesType>);
  std::ostringstream oss;

  // Process each key-value pair
  std::apply(
      [&](const auto &...args) {
        bool first = true;
        size_t index = 0;

        auto format_one = [&](const auto &arg) {
          if (!first) oss << ",";
          oss << label_keys[index++] << "=\"";
          // 同一个流操作链中既传递流引用又修改流状态，无法被链式调用
          LabelStringlizer()(oss, arg);
          oss << "\"";
          first = false;
        };

        (format_one(args), ...); 
      },
      label_values);
  return oss.str();
}

MGSE_API void SetGlobalLabel(const std::string& value);
MGSE_API const std::string& GetGlobalLabel();

MGSE_NS_END

#endif  // COMLIB_STATS_STATS_COMM_H_
