// Copyright (c) Tencent
// Author: roypang
// Create: 2025-04-16
// Note: The features provided in this header requires C++14 or above

#ifndef COMLIB_STATS_LABED_STATS_OBJ_H_
#define COMLIB_STATS_LABED_STATS_OBJ_H_

// comlib/stats/labed_stats_obj.h

#pragma once
#include <assert.h>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <tuple>
#include "comlib/defs/clock.h"
#include "comlib/defs/comdefs.h"
#include "stats_comm.h"
#include "stats_obj.h"

MGSE_NS_BEGIN

#ifndef DEFAULT_STATS_MAX_LABED_OBJ_NUM
#define DEFAULT_STATS_MAX_LABED_OBJ_NUM 10000
#endif

// default 1h
#ifndef DEFAULT_STATS_MAX_LABED_OBJ_EVICT_SEC
#define DEFAULT_STATS_MAX_LABED_OBJ_EVICT_SEC 60 * 60
#endif

// Wrapper shared_ptr for StatsObj, for call StatsObj Inc/Update methods
template <typename StatsObjType, typename ValType>
class StatsObjWrapper {
 public:
  explicit StatsObjWrapper(std::shared_ptr<StatsObjType> obj) : obj_(std::move(obj)) {}

  const std::shared_ptr<StatsObjType>& GetObject() const noexcept { return obj_; }

  // For AccumInt64/AccumDouble
  template <typename T = StatsObjType>
  std::enable_if_t<std::is_same_v<T, AccumInt64> || std::is_same_v<T, AccumDouble>, void> Inc(
      int index, const ValType& v) {
    obj_->Inc(index, v);
  }

  // For GaugeInt64/GaugeDouble
  template <typename T = StatsObjType>
  std::enable_if_t<std::is_same_v<T, GaugeInt64> || std::is_same_v<T, GaugeDouble>, void> Add(
      int index, const ValType& v) {
    obj_->Add(index, v);
  }

  template <typename T = StatsObjType>
  std::enable_if_t<std::is_same_v<T, GaugeInt64> || std::is_same_v<T, GaugeDouble>, void> Sub(
      int index, const ValType& v) {
    obj_->Sub(index, v);
  }

  // For GaugeInt64/GaugeDouble/ScalarInt64/ScalarDouble/SummaryInt64/SummaryDouble
  template <typename T = StatsObjType>
  std::enable_if_t<std::is_same_v<T, GaugeInt64> || std::is_same_v<T, GaugeDouble> ||
                       std::is_same_v<T, ScalarInt64> || std::is_same_v<T, ScalarDouble> ||
                       std::is_same_v<T, SummaryInt64> || std::is_same_v<T, SummaryDouble>,
                   void>
  Update(int index, const ValType& v) {
    obj_->Update(index, v);
  }

 private:
  std::shared_ptr<StatsObjType> obj_;
};

// Item for stats obj
template <typename StatsObjType>
struct LabedStatsItem {
  std::shared_ptr<StatsObjType> obj_;
  utimestamp_t update_time_us_;

  explicit LabedStatsItem(std::shared_ptr<StatsObjType> obj_ptr, utimestamp_t time) noexcept
      : obj_(std::move(obj_ptr)), update_time_us_(time) {}

  LabedStatsItem(LabedStatsItem&& other) noexcept
      : obj_(std::move(other.obj_)), update_time_us_(other.update_time_us_) {}

  std::shared_ptr<StatsObjType> Object() const { return obj_; }
  void UpdateTime() { update_time_us_ = GetTimeStampUs(); }
  utimestamp_t GetUpdateTime() const { return update_time_us_; }

 private:
  LabedStatsItem(const LabedStatsItem&) = delete;
  LabedStatsItem& operator=(const LabedStatsItem&) = delete;
};

// default label stringlizer, You can inherit from this class and override operator() to implement
// custom types of ostream
class DefaultLabelStringlizer {
 public:
  template <typename LabelFieldType>
  std::ostream& operator()(std::ostream& os, const LabelFieldType& v) const {
    os << v;
    return os;
  }
};

// LabedStatsObj for managing labed StatsObj objects
template <typename StatsObjType, typename ValType, typename LabelValuesType,
          typename LabelStringlizer>
class MGSE_API LabedStatsObj : public StatsObj {
 public:
  using LabedStatsObjItem = LabedStatsItem<StatsObjType>;

  LabedStatsObj(const std::string& name, int para_num, const LabelKeys& label_keys)
      : StatsObj(name, GetStatsObjType() + kStatsLabedObjMgrDiff), para_num_(para_num) {
    assert(label_keys.size() == std::tuple_size_v<LabelValuesType>);
    label_keys_ = label_keys;
  }

  LabedStatsObj(const std::string& name, int para_num, const LabelKeys& label_keys,
                const ExtraValues<ValType>& borders)
      : StatsObj(name, GetStatsObjType() + kStatsLabedObjMgrDiff), para_num_(para_num) {
    assert(label_keys.size() == std::tuple_size_v<LabelValuesType>);
    label_keys_ = label_keys;

    for (size_t i = 0; i + 1 < borders.size(); ++i) {
      assert(borders[i] < borders[i + 1]);
    }
    hist_borders_ = borders;
  }

  LabedStatsObj(const std::string& name, int para_num, const LabelKeys& label_keys, int cap,
                const std::vector<double>& quantiles)
      : StatsObj(name, GetStatsObjType() + kStatsLabedObjMgrDiff), para_num_(para_num) {
    assert(label_keys.size() == std::tuple_size_v<LabelValuesType>);
    assert(cap > 0);
    assert(quantiles.size() > 0);

    for (size_t i = 0; i < quantiles.size(); ++i) {
      assert(quantiles[i] >= 0 && quantiles[i] <= 1);
      if (i > 0) {
        assert(quantiles[i] > quantiles[i - 1]);
      }
    }

    label_keys_ = label_keys;
    quantiles_ = quantiles;
    cap_ = cap;
  }

  void Reset(bool is_complete_reset) override {
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    for (auto& entry : stats_objs_) {
      entry.second.Object()->Reset(is_complete_reset);
    }
  }

  void SerializeAndReset(std::stringstream* ss1) override {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    for (auto& entry : stats_objs_) {
      entry.second.Object()->SerializeAndReset(ss1);
      *ss1 << "\n";
    }
  }

  void SerializeToStream(std::stringstream* ss1) const override {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    for (auto& entry : stats_objs_) {
      entry.second.Object()->SerializeToStream(ss1);
      *ss1 << "\n";
    }
  }

  void SerializeBody(std::stringstream* ss) const override {}

  void SerializeToPromeStream(std::stringstream* ss1) const override {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);

    bool with_head = true;
    for (const auto& entry : stats_objs_) {
      if (with_head) {
        entry.second.Object()->SerializeHead(ss1);
      }
      entry.second.Object()->SerializeBody(ss1);
      with_head = false;
    }
  }

  // StatsObj超过最大个数，则淘汰过期StatsObj
  void Update() override {
    // 写锁，淘汰过期StatsObj
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    size_t process_num = 0;
    constexpr size_t max_process_num = 100;
    const utimestamp_t current_time = GetTimeStampUs();
    const utimestamp_t evict_time_us = max_stats_obj_evict_sec_ * 1000 * 1000;

    while (!evict_heap_.empty()) {
      if (++process_num > max_process_num) {
        break;
      }

      // 检查是否超过最大数量或有过期对象
      const auto& [update_time_us, label_values] = evict_heap_.top();
      bool is_expired = (current_time - update_time_us) > evict_time_us;
      if (!is_expired && stats_objs_.size() <= max_stats_obj_num_) {
        break;
      }

      // 对象被Remove
      auto obj_it = stats_objs_.find(label_values);
      if (obj_it == stats_objs_.end()) {
        evict_heap_.pop();
        continue;
      }

      // 已经更新
      if (obj_it->second.GetUpdateTime() != update_time_us) {
        auto copy_label_values = label_values;
        evict_heap_.pop();
        evict_heap_.push({obj_it->second.GetUpdateTime(), std::move(copy_label_values)});
        continue;
      }

      // 删除主对象
      stats_objs_.erase(obj_it);
      evict_heap_.pop();
    }
  }

  bool SetParaNum(int para_num) override {
    if (para_num > STATS_MAX_PARALLEL_NUM) {
      return false;
    }

    para_num_ = para_num;
    return true;
  }

  size_t Size() const override {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return stats_objs_.size();
  }

  StatsObjWrapper<StatsObjType, ValType> GetWithAdd(const LabelValuesType& label_values) {
    // get
    {
      std::shared_lock<std::shared_mutex> lock(rw_mutex_);
      auto it = stats_objs_.find(label_values);
      if (it != stats_objs_.end()) {
        it->second.UpdateTime();
        return StatsObjWrapper<StatsObjType, ValType>(it->second.Object());
      }
    }

    // add
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    //  get again
    auto it = stats_objs_.find(label_values);
    if (it != stats_objs_.end()) {
      it->second.UpdateTime();
      return StatsObjWrapper<StatsObjType, ValType>(it->second.Object());
    }
    //  do add
    LabedStatsObjItem* ptr = Add(label_values);
    assert(ptr != nullptr);
    return StatsObjWrapper<StatsObjType, ValType>(ptr->Object());
  }

  size_t Remove(const LabelValuesType& label_values) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    size_t num = 0;
    auto it = stats_objs_.find(label_values);
    if (it != stats_objs_.end()) {
      stats_objs_.erase(it);
      num++;
    }
    return num;
  }

  // 设置StatsObj的最大数量和最大淘汰时间，默认为DEFAULT_STATS_MAX_LABED_OBJ_NUM和DEFAULT_STATS_MAX_LABED_OBJ_EVICT_SEC
  // 10000个StatsObj占用内存25MB左右，系统运行过程中，需要主动调用Update来自动淘汰过期指标；超过最大个数或者最大淘汰时间都会被淘汰
  // 如果淘汰不及时，超过最大的StatsObj个数的指标的维度全部标识为metrics_name{"label":"overlimit"}，保证StatsObj的内存占用在一定范围内
  void SetMaxStatsObjNum(size_t max_labed_obj_num) { max_stats_obj_num_ = max_labed_obj_num; }
  size_t GetMaxStatsObjNum() const { return max_stats_obj_num_; }
  void SetMaxStatsObjEvictSec(size_t evict_sec) { max_stats_obj_evict_sec_ = evict_sec; }
  size_t GetMaxStatsObjEvictSec() const { return max_stats_obj_evict_sec_; }

 private:
  static constexpr int GetStatsObjType() {
    if constexpr (std::is_same_v<StatsObjType, AccumInt64>) {
      return STATS_ACCUM_INT64;
    } else if constexpr (std::is_same_v<StatsObjType, AccumDouble>) {
      return STATS_ACCUM_DOUBLE;
    } else if constexpr (std::is_same_v<StatsObjType, GaugeInt64>) {
      return STATS_GAUGE_INT64;
    } else if constexpr (std::is_same_v<StatsObjType, GaugeDouble>) {
      return STATS_GAUGE_DOUBLE;
    } else if constexpr (std::is_same_v<StatsObjType, ScalarInt64>) {
      return STATS_SCALAR_INT64;
    } else if constexpr (std::is_same_v<StatsObjType, ScalarDouble>) {
      return STATS_SCALAR_DOUBLE;
    } else if constexpr (std::is_same_v<StatsObjType, SummaryInt64>) {
      return STATS_SUMMARY_INT64;
    } else if constexpr (std::is_same_v<StatsObjType, SummaryDouble>) {
      return STATS_SUMMARY_DOUBLE;
    } else {
      return STATS_NULL;
    }
  }

  LabedStatsObjItem* Add(const LabelValuesType& label_values) {
    if constexpr (std::is_same_v<StatsObjType, AccumInt64> ||
                  std::is_same_v<StatsObjType, AccumDouble>) {
      return AddAccum(label_values);
    } else if constexpr (std::is_same_v<StatsObjType, GaugeInt64> ||
                         std::is_same_v<StatsObjType, GaugeDouble>) {
      return AddGauge(label_values);
    } else if constexpr (std::is_same_v<StatsObjType, ScalarInt64> ||
                         std::is_same_v<StatsObjType, ScalarDouble>) {
      return AddScalar(label_values);
    } else if constexpr (std::is_same_v<StatsObjType, SummaryInt64> ||
                         std::is_same_v<StatsObjType, SummaryDouble>) {
      return AddSummary(label_values);
    }
    return nullptr;
  }

  LabedStatsObjItem* AddAccum(const LabelValuesType& label_values) {
    auto accum_obj = std::make_shared<StatsObjType>(name_, GetStatsObjType(), para_num_, false);
    accum_obj->SetLabels(GetLabelString(label_values));
    auto time_us = GetTimeStampUs();
    auto result =
        stats_objs_.emplace(label_values, LabedStatsObjItem(std::move(accum_obj), time_us));
    evict_heap_.push({time_us, label_values});
    return &result.first->second;
  }

  LabedStatsObjItem* AddGauge(const LabelValuesType& label_values) {
    auto gauge_obj = std::make_shared<StatsObjType>(name_, GetStatsObjType(), para_num_, false);
    gauge_obj->SetLabels(GetLabelString(label_values));
    auto time_us = GetTimeStampUs();
    auto result =
        stats_objs_.emplace(label_values, LabedStatsObjItem(std::move(gauge_obj), time_us));
    evict_heap_.push({time_us, label_values});
    return &result.first->second;
  }

  LabedStatsObjItem* AddScalar(const LabelValuesType& label_values) {
    auto scalar_obj = std::make_shared<StatsObjType>(
        name_, GetStatsObjType(), para_num_, hist_borders_.data(), hist_borders_.size(), false);
    auto time_us = GetTimeStampUs();
    scalar_obj->SetLabels(GetLabelString(label_values));
    auto result =
        stats_objs_.emplace(label_values, LabedStatsObjItem(std::move(scalar_obj), time_us));
    evict_heap_.push({time_us, label_values});
    return &result.first->second;
  }

  LabedStatsObjItem* AddSummary(const LabelValuesType& label_values) {
    auto summary_obj = std::make_shared<StatsObjType>(name_, GetStatsObjType(), para_num_, cap_,
                                                      quantiles_, false);
    auto time_us = GetTimeStampUs();
    summary_obj->SetLabels(GetLabelString(label_values));
    auto result =
        stats_objs_.emplace(label_values, LabedStatsObjItem(std::move(summary_obj), time_us));
    evict_heap_.push({time_us, label_values});
    return &result.first->second;
  }

  std::string GetLabelString(const LabelValuesType& label_values) const {
    if (stats_objs_.size() > max_stats_obj_num_) {
      return "{label=\"over_limit\"}";
    }
    if (label_keys_.empty()) {
      return "{label=\"key_empty\"}";
    }

    return GetLabelDimensionStr<LabelValuesType, LabelStringlizer>(label_keys_, label_values);
  }

 private:
  // common attr for all labed stats objects
  mutable std::shared_mutex rw_mutex_;
  std::map<LabelValuesType, LabedStatsObjItem> stats_objs_;
  using EvictElement = std::pair<utimestamp_t, LabelValuesType>;
  std::priority_queue<EvictElement, std::vector<EvictElement>, std::greater<EvictElement>>
      evict_heap_;
  LabelKeys label_keys_;
  int para_num_ = 0;  // for multi thread
  size_t max_stats_obj_num_ = DEFAULT_STATS_MAX_LABED_OBJ_NUM;
  size_t max_stats_obj_evict_sec_ = DEFAULT_STATS_MAX_LABED_OBJ_EVICT_SEC;

  // 用于Histogram的统计对象
  ExtraValues<ValType> hist_borders_;

  // 用于Summary的统计对象
  int cap_ = 0;
  std::vector<double> quantiles_;
};

#define DECLARE_LABED_STATS_OBJ(StatsObjType, ValueType, name, ...)                             \
  extern ::MGSE_NS::LabedStatsObj<StatsObjType, ValueType, ::MGSE_NS::LabelValues<__VA_ARGS__>, \
                                  ::MGSE_NS::DefaultLabelStringlizer>                           \
      g_labed_stats_##name

#define DECLARE_LABED_STATS_OBJ_STRINGLIZER(StatsObjType, ValueType, name, StringLizer, ...)    \
  extern ::MGSE_NS::LabedStatsObj<StatsObjType, ValueType, ::MGSE_NS::LabelValues<__VA_ARGS__>, \
                                  StringLizer>                                                  \
      g_labed_stats_##name

// 定义公共的实例化宏，目前支持label最多4个，如果有需要扩展，扩展这里的宏即可
#define DEFINE_LABED_ACCUM_STATS_OBJ_1(name, para_num, ValueType, StringLizer, key1, type1) \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::Accumulator<ValueType>, ValueType,                    \
                           ::MGSE_NS::LabelValues<type1>, StringLizer>                      \
      g_labed_stats_##name(#name, para_num, {key1})

#define DEFINE_LABED_ACCUM_STATS_OBJ_2(name, para_num, ValueType, StringLizer, key1, type1, key2, \
                                       type2)                                                     \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::Accumulator<ValueType>, ValueType,                          \
                           ::MGSE_NS::LabelValues<type1, type2>, StringLizer>                     \
      g_labed_stats_##name(#name, para_num, {key1, key2})

#define DEFINE_LABED_ACCUM_STATS_OBJ_3(name, para_num, ValueType, StringLizer, key1, type1, key2, \
                                       type2, key3, type3)                                        \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::Accumulator<ValueType>, ValueType,                          \
                           ::MGSE_NS::LabelValues<type1, type2, type3>, StringLizer>              \
      g_labed_stats_##name(#name, para_num, {key1, key2, key3})

#define DEFINE_LABED_ACCUM_STATS_OBJ_4(name, para_num, ValueType, StringLizer, key1, type1, key2, \
                                       type2, key3, type3, key4, type4)                           \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::Accumulator<ValueType>, ValueType,                          \
                           ::MGSE_NS::LabelValues<type1, type2, type3, type4>, StringLizer>       \
      g_labed_stats_##name(#name, para_num, {key1, key2, key3, key4})

#define DEFINE_LABED_GAUGE_STATS_OBJ_1(name, para_num, ValueType, StringLizer, key1, type1) \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::GaugeStatsObj<ValueType>, ValueType,                  \
                           ::MGSE_NS::LabelValues<type1>, StringLizer>                      \
      g_labed_stats_##name(#name, para_num, {key1})

#define DEFINE_LABED_GAUGE_STATS_OBJ_2(name, para_num, ValueType, StringLizer, key1, type1, key2, \
                                       type2)                                                     \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::GaugeStatsObj<ValueType>, ValueType,                        \
                           ::MGSE_NS::LabelValues<type1, type2>, StringLizer>                     \
      g_labed_stats_##name(#name, para_num, {key1, key2})

#define DEFINE_LABED_GAUGE_STATS_OBJ_3(name, para_num, ValueType, StringLizer, key1, type1, key2, \
                                       type2, key3, type3)                                        \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::GaugeStatsObj<ValueType>, ValueType,                        \
                           ::MGSE_NS::LabelValues<type1, type2, type3>, StringLizer>              \
      g_labed_stats_##name(#name, para_num, {key1, key2, key3})

#define DEFINE_LABED_GAUGE_STATS_OBJ_4(name, para_num, ValueType, StringLizer, key1, type1, key2, \
                                       type2, key3, type3, key4, type4)                           \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::GaugeStatsObj<ValueType>, ValueType,                        \
                           ::MGSE_NS::LabelValues<type1, type2, type3, type4>, StringLizer>       \
      g_labed_stats_##name(#name, para_num, {key1, key2, key3, key4})

#define DEFINE_LABED_SCALAR_STATS_OBJ_1(name, para_num, ValueType, StringLizer, key1, type1, ...) \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::ScalarStatsObj<ValueType>, ValueType,                       \
                           ::MGSE_NS::LabelValues<type1>, StringLizer>                            \
      g_labed_stats_##name(#name, para_num, {key1}, {__VA_ARGS__})

#define DEFINE_LABED_SCALAR_STATS_OBJ_2(name, para_num, ValueType, StringLizer, key1, type1, key2, \
                                        type2, ...)                                                \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::ScalarStatsObj<ValueType>, ValueType,                        \
                           ::MGSE_NS::LabelValues<type1, type2>, StringLizer>                      \
      g_labed_stats_##name(#name, para_num, {key1, key2}, {__VA_ARGS__})

#define DEFINE_LABED_SCALAR_STATS_OBJ_3(name, para_num, ValueType, StringLizer, key1, type1, key2, \
                                        type2, key3, type3, ...)                                   \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::ScalarStatsObj<ValueType>, ValueType,                        \
                           ::MGSE_NS::LabelValues<type1, type2, type3>, StringLizer>               \
      g_labed_stats_##name(#name, para_num, {key1, key2, key3}, {__VA_ARGS__})

#define DEFINE_LABED_SCALAR_STATS_OBJ_4(name, para_num, ValueType, StringLizer, key1, type1, key2, \
                                        type2, key3, type3, key4, type4, ...)                      \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::ScalarStatsObj<ValueType>, ValueType,                        \
                           ::MGSE_NS::LabelValues<type1, type2, type3, type4>, StringLizer>        \
      g_labed_stats_##name(#name, para_num, {key1, key2, key3, key4}, {__VA_ARGS__})

#define DEFINE_LABED_SUMMARY_STATS_OBJ_1(name, para_num, ValueType, StringLizer, key1, type1, cap, \
                                         ...)                                                      \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::SummaryStatsObj<ValueType>, ValueType,                       \
                           ::MGSE_NS::LabelValues<type1>, StringLizer>                             \
      g_labed_stats_##name(#name, para_num, {key1}, cap, {__VA_ARGS__})

#define DEFINE_LABED_SUMMARY_STATS_OBJ_2(name, para_num, ValueType, StringLizer, key1, type1, \
                                         key2, type2, cap, ...)                               \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::SummaryStatsObj<ValueType>, ValueType,                  \
                           ::MGSE_NS::LabelValues<type1, type2>, StringLizer>                 \
      g_labed_stats_##name(#name, para_num, {key1, key2}, cap, {__VA_ARGS__})

#define DEFINE_LABED_SUMMARY_STATS_OBJ_3(name, para_num, ValueType, StringLizer, key1, type1, \
                                         key2, type2, key3, type3, cap, ...)                  \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::SummaryStatsObj<ValueType>, ValueType,                  \
                           ::MGSE_NS::LabelValues<type1, type2, type3>, StringLizer>          \
      g_labed_stats_##name(#name, para_num, {key1, key2, key3}, cap, {__VA_ARGS__})

#define DEFINE_LABED_SUMMARY_STATS_OBJ_4(name, para_num, ValueType, StringLizer, key1, type1, \
                                         key2, type2, key3, type3, key4, type4, cap, ...)     \
  ::MGSE_NS::LabedStatsObj<::MGSE_NS::SummaryStatsObj<ValueType>, ValueType,                  \
                           ::MGSE_NS::LabelValues<type1, type2, type3, type4>, StringLizer>   \
      g_labed_stats_##name(#name, para_num, {key1, key2, key3, key4}, cap, {__VA_ARGS__})

/*
  静态注册接口，适用于编译时就能确定的全局统计对象
*/

// define/declare ValType int64_t -----

#define DEFINE_LABED_STATS_ACCUM_INT64(name, para_num, label_count, ...) \
  DEFINE_LABED_ACCUM_STATS_OBJ_##label_count(name, para_num, int64_t,    \
                                             ::MGSE_NS::DefaultLabelStringlizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_ACCUM_INT64(name, ...) \
  DECLARE_LABED_STATS_OBJ(::MGSE_NS::AccumInt64, int64_t, name, __VA_ARGS__)

#define DEFINE_LABED_STATS_GAUGE_INT64(name, para_num, label_count, ...) \
  DEFINE_LABED_GAUGE_STATS_OBJ_##label_count(name, para_num, int64_t,    \
                                             ::MGSE_NS::DefaultLabelStringlizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_GAUGE_INT64(name, ...) \
  DECLARE_LABED_STATS_OBJ(::MGSE_NS::GaugeInt64, int64_t, name, __VA_ARGS__)

#define DEFINE_LABED_STATS_SCALAR_INT64(name, para_num, label_count, ...) \
  DEFINE_LABED_SCALAR_STATS_OBJ_##label_count(name, para_num, int64_t,    \
                                              ::MGSE_NS::DefaultLabelStringlizer, __VA_ARGS__)

#define DEFINE_LABED_STATS_SCALAR_INT64_NO_HISTOGRAM(name, para_num, label_count, ...) \
  DEFINE_LABED_SCALAR_STATS_OBJ_##label_count(name, para_num, int64_t,                 \
                                              ::MGSE_NS::DefaultLabelStringlizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_SCALAR_INT64(name, ...) \
  DECLARE_LABED_STATS_OBJ(::MGSE_NS::ScalarInt64, int64_t, name, __VA_ARGS__)

#define DEFINE_LABED_STATS_SUMMARY_INT64(name, para_num, label_count, ...) \
  DEFINE_LABED_SUMMARY_STATS_OBJ_##label_count(name, para_num, int64_t,    \
                                               ::MGSE_NS::DefaultLabelStringlizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_SUMMARY_INT64(name, ...) \
  DECLARE_LABED_STATS_OBJ(::MGSE_NS::SummaryInt64, int64_t, name, __VA_ARGS__)

// define/declare ValType int64_t with StringLizer-----

#define DEFINE_LABED_STATS_ACCUM_INT64_STRINGLIZER(name, para_num, StringLizer, label_count, ...) \
  DEFINE_LABED_ACCUM_STATS_OBJ_##label_count(name, para_num, int64_t, StringLizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_ACCUM_INT64_STRINGLIZER(name, StringLizer, ...)              \
  DECLARE_LABED_STATS_OBJ_STRINGLIZER(::MGSE_NS::AccumInt64, int64_t, name, StringLizer, \
                                      __VA_ARGS__)

#define DEFINE_LABED_STATS_GAUGE_INT64_STRINGLIZER(name, para_num, StringLizer, label_count, ...) \
  DEFINE_LABED_GAUGE_STATS_OBJ_##label_count(name, para_num, int64_t, StringLizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_GAUGE_INT64_STRINGLIZER(name, StringLizer, ...)              \
  DECLARE_LABED_STATS_OBJ_STRINGLIZER(::MGSE_NS::AccumInt64, int64_t, name, StringLizer, \
                                      __VA_ARGS__)

#define DEFINE_LABED_STATS_SCALAR_INT64_STRINGLIZER(name, para_num, StringLizer, label_count, ...) \
  DEFINE_LABED_SCALAR_STATS_OBJ_##label_count(name, para_num, int64_t, StringLizer, __VA_ARGS__)

#define DEFINE_LABED_STATS_SCALAR_INT64_NO_HISTOGRAM_STRINGLIZER(name, para_num, StringLizer, \
                                                                 label_count, ...)            \
  DEFINE_LABED_SCALAR_STATS_OBJ_##label_count(name, para_num, int64_t, StringLizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_SCALAR_INT64_STRINGLIZER(name, StringLizer, ...)              \
  DECLARE_LABED_STATS_OBJ_STRINGLIZER(::MGSE_NS::ScalarInt64, int64_t, name, StringLizer, \
                                      __VA_ARGS__)

#define DEFINE_LABED_STATS_SUMMARY_INT64_STRINGLIZER(name, para_num, StringLizer, label_count, \
                                                     ...)                                      \
  DEFINE_LABED_SUMMARY_STATS_OBJ_##label_count(name, para_num, int64_t, StringLizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_SUMMARY_INT64_STRINGLIZER(name, StringLizer, ...)              \
  DECLARE_LABED_STATS_OBJ_STRINGLIZER(::MGSE_NS::SummaryInt64, int64_t, name, StringLizer, \
                                      __VA_ARGS__)

// define/declare ValType double -----

#define DEFINE_LABED_STATS_ACCUM_DOUBLE(name, para_num, label_count, ...) \
  DEFINE_LABED_ACCUM_STATS_OBJ_##label_count(name, para_num, double,      \
                                             ::MGSE_NS::DefaultLabelStringlizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_ACCUM_DOUBLE(name, ...) \
  DECLARE_LABED_STATS_OBJ(::MGSE_NS::AccumDouble, double, name, __VA_ARGS__)

#define DEFINE_LABED_STATS_GAUGE_DOUBLE(name, para_num, label_count, ...) \
  DEFINE_LABED_GAUGE_STATS_OBJ_##label_count(name, para_num, double,      \
                                             ::MGSE_NS::DefaultLabelStringlizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_GAUGE_DOUBLE(name, ...) \
  DECLARE_LABED_STATS_OBJ(::MGSE_NS::GaugeDouble, double, name, __VA_ARGS__)

#define DEFINE_LABED_STATS_SCALAR_DOUBLE(name, para_num, label_count, ...) \
  DEFINE_LABED_SCALAR_STATS_OBJ_##label_count(name, para_num, double,      \
                                              ::MGSE_NS::DefaultLabelStringlizer, __VA_ARGS__)
#define DEFINE_LABED_STATS_SCALAR_DOUBLE_NO_HISTOGRAM(name, para_num, label_count, ...) \
  DEFINE_LABED_SCALAR_STATS_OBJ_##label_count(name, para_num, double,                   \
                                              ::MGSE_NS::DefaultLabelStringlizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_SCALAR_DOUBLE(name, ...) \
  DECLARE_LABED_STATS_OBJ(::MGSE_NS::ScalarDouble, double, name, __VA_ARGS__)

#define DEFINE_LABED_STATS_SUMMARY_DOUBLE(name, para_num, label_count, ...) \
  DEFINE_LABED_SUMMARY_STATS_OBJ_##label_count(name, para_num, double,      \
                                               ::MGSE_NS::DefaultLabelStringlizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_SUMMARY_DOUBLE(name, ...) \
  DECLARE_LABED_STATS_OBJ(::MGSE_NS::SummaryDouble, double, name, __VA_ARGS__)

// define/declare ValType double with StringLizer-----

#define DEFINE_LABED_STATS_ACCUM_DOUBLE_STRINGLIZER(name, para_num, StringLizer, label_count, ...) \
  DEFINE_LABED_ACCUM_STATS_OBJ_##label_count(name, para_num, double, StringLizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_ACCUM_DOUBLE_STRINGLIZER(name, StringLizer, ...)             \
  DECLARE_LABED_STATS_OBJ_STRINGLIZER(::MGSE_NS::AccumDouble, double, name, StringLizer, \
                                      __VA_ARGS__)

#define DEFINE_LABED_STATS_GAUGE_DOUBLE_STRINGLIZER(name, para_num, StringLizer, label_count, ...) \
  DEFINE_LABED_GAUGE_STATS_OBJ_##label_count(name, para_num, double, StringLizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_GAUGE_DOUBLE_STRINGLIZER(name, StringLizer, ...)             \
  DECLARE_LABED_STATS_OBJ_STRINGLIZER(::MGSE_NS::AccumDouble, double, name, StringLizer, \
                                      __VA_ARGS__)

#define DEFINE_LABED_STATS_SCALAR_DOUBLE_STRINGLIZER(name, para_num, StringLizer, label_count, \
                                                     ...)                                      \
  DEFINE_LABED_SCALAR_STATS_OBJ_##label_count(name, para_num, double, StringLizer, __VA_ARGS__)
#define DEFINE_LABED_STATS_SCALAR_DOUBLE_NO_HISTOGRAM_STRINGLIZER(name, para_num, StringLizer, \
                                                                  label_count, ...)            \
  DEFINE_LABED_SCALAR_STATS_OBJ_##label_count(name, para_num, double, StringLizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_SCALAR_DOUBLE_STRINGLIZER(name, StringLizer, ...)             \
  DECLARE_LABED_STATS_OBJ_STRINGLIZER(::MGSE_NS::ScalarDouble, double, name, StringLizer, \
                                      __VA_ARGS__)

#define DEFINE_LABED_STATS_SUMMARY_DOUBLE_STRINGLIZER(name, para_num, StringLizer, label_count, \
                                                      ...)                                      \
  DEFINE_LABED_SUMMARY_STATS_OBJ_##label_count(name, para_num, double, StringLizer, __VA_ARGS__)
#define DECLARE_LABED_STATS_SUMMARY_DOUBLE_STRINGLIZER(name, StringLizer, ...)             \
  DECLARE_LABED_STATS_OBJ_STRINGLIZER(::MGSE_NS::SummaryDouble, double, name, StringLizer, \
                                      __VA_ARGS__)

// define get stats obj
#define LABED_STATS_OBJ(name, ...) g_labed_stats_##name.GetWithAdd(std::make_tuple(__VA_ARGS__))

#define LABED_STATS_OBJ_REMOVE(name, ...) g_labed_stats_##name.Remove(std::make_tuple(__VA_ARGS__))

#define LABED_STATS_OBJ_GROUP(name) g_labed_stats_##name

/*
  动态注册接口，适用于运行时才能确定的统计对象
*/

template <typename... LabelTypes>
using LabedAccumInt64 =
    LabedStatsObj<AccumInt64, int64_t, LabelValues<LabelTypes...>, DefaultLabelStringlizer>;
template <typename... LabelTypes>
using LabedAccumDouble =
    LabedStatsObj<AccumDouble, double, LabelValues<LabelTypes...>, DefaultLabelStringlizer>;
template <typename... LabelTypes>
using LabedScalarInt64 =
    LabedStatsObj<ScalarInt64, int64_t, LabelValues<LabelTypes...>, DefaultLabelStringlizer>;
template <typename... LabelTypes>
using LabedScalarDouble =
    LabedStatsObj<ScalarDouble, double, LabelValues<LabelTypes...>, DefaultLabelStringlizer>;
template <typename... LabelTypes>
using LabedSummaryInt64 =
    LabedStatsObj<SummaryInt64, int64_t, LabelValues<LabelTypes...>, DefaultLabelStringlizer>;
template <typename... LabelTypes>
using LabedSummaryDouble =
    LabedStatsObj<SummaryDouble, double, LabelValues<LabelTypes...>, DefaultLabelStringlizer>;

template <typename... LabelTypes>
static inline LabedAccumInt64<LabelTypes...>* NewLabedStatsAccumInt64(
    const std::string& name, int para_num, std::initializer_list<std::string> label_keys) {
  assert(sizeof...(LabelTypes) == label_keys.size());
  return new LabedStatsObj<Accumulator<int64_t>, int64_t, LabelValues<LabelTypes...>,
                           DefaultLabelStringlizer>(name, para_num, label_keys, false);
}

template <typename... LabelTypes>
static inline LabedAccumDouble<LabelTypes...>* NewLabedStatsAccumDouble(
    const std::string& name, int para_num, std::initializer_list<std::string> label_keys) {
  assert(sizeof...(LabelTypes) == label_keys.size());
  return new LabedStatsObj<Accumulator<double>, double, LabelValues<LabelTypes...>,
                           DefaultLabelStringlizer>(name, para_num, label_keys, false);
}

template <typename... LabelTypes>
static inline LabedScalarInt64<LabelTypes...>* NewLabedStatsScalarInt64(
    const std::string& name, int para_num, std::initializer_list<std::string> label_keys) {
  assert(sizeof...(LabelTypes) == label_keys.size());
  return new LabedStatsObj<ScalarStatsObj<int64_t>, int64_t, LabelValues<LabelTypes...>,
                           DefaultLabelStringlizer>(name, para_num, label_keys, true, {});
}

template <typename... LabelTypes>
static inline LabedScalarDouble<LabelTypes...>* NewLabedStatsScalarDouble(
    const std::string& name, int para_num, std::initializer_list<std::string> label_keys) {
  assert(sizeof...(LabelTypes) == label_keys.size());
  return new LabedStatsObj<ScalarStatsObj<double>, double, LabelValues<LabelTypes...>,
                           DefaultLabelStringlizer>(name, para_num, label_keys, true, {});
}

template <typename... LabelTypes>
static inline LabedScalarInt64<LabelTypes...>* NewLabedStatsHistogramInt64(
    const std::string& name, int para_num, std::initializer_list<std::string> label_keys,
    std::initializer_list<int64_t> borders) {
  assert(sizeof...(LabelTypes) == label_keys.size());
  return new LabedStatsObj<ScalarStatsObj<int64_t>, int64_t, LabelValues<LabelTypes...>,
                           DefaultLabelStringlizer>(name, para_num, label_keys, true, borders);
}

template <typename... LabelTypes>
static inline LabedScalarDouble<LabelTypes...>* NewLabedStatsHistogramDouble(
    const std::string& name, int para_num, std::initializer_list<std::string> label_keys,
    std::initializer_list<double> borders) {
  assert(sizeof...(LabelTypes) == label_keys.size());
  return new LabedStatsObj<ScalarStatsObj<double>, double, LabelValues<LabelTypes...>,
                           DefaultLabelStringlizer>(name, para_num, label_keys, true, borders);
}

template <typename... LabelTypes>
static inline LabedSummaryInt64<LabelTypes...>* NewLabedStatsSummaryInt64(
    const std::string& name, int para_num, std::initializer_list<std::string> label_keys, int cap,
    std::initializer_list<double> quantiles) {
  assert(sizeof...(LabelTypes) == label_keys.size());
  return new LabedStatsObj<SummaryStatsObj<int64_t>, int64_t, LabelValues<LabelTypes...>,
                           DefaultLabelStringlizer>(name, para_num, label_keys, true, cap,
                                                    quantiles);
}

template <typename... LabelTypes>
static inline LabedSummaryDouble<LabelTypes...>* NewLabedStatsSummaryDouble(
    const std::string& name, int para_num, std::initializer_list<std::string> label_keys, int cap,
    std::initializer_list<double> quantiles) {
  assert(sizeof...(LabelTypes) == label_keys.size());
  return new LabedStatsObj<SummaryStatsObj<double>, double, LabelValues<LabelTypes...>,
                           DefaultLabelStringlizer>(name, para_num, label_keys, true, cap,
                                                    quantiles);
}

/**
 * 自动淘汰StatsObj。淘汰规则：调用每个LabedStatsObj的Update进行淘汰。
 * 建议调用频率，一分钟一次。不支持多线程调用。
 */
MGSE_API void UpdateLabedStatsObj();

MGSE_NS_END

#endif