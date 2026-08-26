// Copyright (c) Tencent
// Author: bondshi
// Create: 2021-07-30
// Note: The features provided in this header requires C++14 or above

#ifndef COMLIB_STATS_STATS_OBJ_H_
#define COMLIB_STATS_STATS_OBJ_H_

// comlib/stats/stats_obj.h
#pragma once

#include <assert.h>
#include <string.h>
#include <algorithm>
#include <deque>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <vector>
#include "comlib/defs/comdefs.h"
#include "stats_comm.h"

MGSE_NS_BEGIN

/////////////////////////////////////////////////////////////////////////////////

enum {
  STATS_NULL = 0,
  STATS_ACCUM_INT64,
  STATS_ACCUM_DOUBLE,
  STATS_GAUGE_INT64,
  STATS_GAUGE_DOUBLE,
  STATS_SCALAR_INT64,
  STATS_SCALAR_DOUBLE,
  STATS_SUMMARY_INT64,
  STATS_SUMMARY_DOUBLE,
  STATS_LABED_ACCUM_INT64,
  STATS_LABED_ACCUM_DOUBLE,
  STATS_LABED_GAUGE_INT64,
  STATS_LABED_GAUGE_DOUBLE,
  STATS_LABED_SCALAR_INT64,
  STATS_LABED_SCALAR_DOUBLE,
  STATS_LABED_SUMMARY_INT64,
  STATS_LABED_SUMMARY_DOUBLE,
  STATS_MAX_TYPE
};

const int kStatsLabedObjMgrDiff = STATS_LABED_ACCUM_INT64 - STATS_ACCUM_INT64;

#ifndef STATS_MAX_PARALLEL_NUM
#define STATS_MAX_PARALLEL_NUM 100
#endif

template <typename ValType>
static inline ValType StatsCalcSum(const ValType *vals, int count) {
  ValType sum = 0;
  assert(vals != NULL);
  for (int i = 0; i < count; ++i) {
    sum += vals[i];
  }
  return sum;
}

class MGSE_API StatsObj;
class MGSE_API StatsObjRegistry {
 public:
  typedef std::unordered_map<std::string, StatsObj *> ObjMap;
  typedef std::vector<StatsObj *> ObjVec;
  static StatsObjRegistry &instance();

  void Register(const std::string &name, StatsObj *obj) {
    assert(obj != NULL);
    assert(Get(name) == NULL);
    if (inds_.find(name) == inds_.end()) {
      inds_[name] = obj;
      objs_.push_back(obj);
    }
  }

  void Unregister(const std::string &name) {
    auto it = inds_.find(name);
    if (it != inds_.end()) {
      auto obj = it->second;
      inds_.erase(it);

      auto it2 = std::find(objs_.begin(), objs_.end(), obj);
      if (it2 != objs_.end()) {
        objs_.erase(it2);
      }
    }
  }

  StatsObj *Get(const std::string &name) const {
    const auto it = inds_.find(name);
    return (it == inds_.end()) ? NULL : it->second;
  }

  ObjVec &objs() { return objs_; }
  const ObjVec &objs() const { return objs_; }

 private:
  StatsObjRegistry() {}

  ObjVec objs_;
  ObjMap inds_;
};

class MGSE_API StatsObj {
 public:
  StatsObj(const std::string &name, int type) : StatsObj(name, type, true) {}
  StatsObj(const std::string &name, int type, bool reg) : name_(name), type_(type), with_reg_(reg) {
    assert(CheckMetricName(name));
    if (with_reg_) {
      StatsObjRegistry::instance().Register(name, this);
    }
  }

  StatsObj(const StatsObj &) = delete;
  StatsObj &operator=(const StatsObj &) = delete;

  virtual ~StatsObj() {
    if (with_reg_) {
      StatsObjRegistry::instance().Unregister(name_);
    }
  }

  const std::string &name() const { return name_; }
  int type() const { return type_; }
  void SerializeHead(std::stringstream *ss) const {
    *ss << "# HELP " << name_ << " " << help_ << "\n";
    if (type_ == STATS_ACCUM_INT64 || type_ == STATS_ACCUM_DOUBLE) {
      *ss << "# TYPE " << name_ << " counter\n";
    } else if (type_ == STATS_GAUGE_INT64 || type_ == STATS_GAUGE_DOUBLE) {
      *ss << "# TYPE " << name_ << " gauge\n";
    } else if (type_ == STATS_SCALAR_INT64 || type_ == STATS_SCALAR_DOUBLE) {
      *ss << "# TYPE " << name_ << " histogram\n";
    } else if (type_ == STATS_SUMMARY_INT64 || type_ == STATS_SUMMARY_DOUBLE) {
      *ss << "# TYPE " << name_ << " summary\n";
    } else {
      *ss << "# TYPE " << name_ << " untyped\n";
    }
  }

  bool SerializeLabel(std::stringstream *ss) const {
    const std::string &global_label = GetGlobalLabel();
    if (!global_label.empty() && !labels_.empty()) {
      *ss << global_label << "," << labels_;
      return true;
    } else if (!global_label.empty()) {
      *ss << global_label;
      return true;
    } else if (!labels_.empty()) {
      *ss << labels_;
      return true;
    }
    return false;
  }

  virtual void SerializeBody(std::stringstream *ss) const = 0;

  void SetLabels(const std::string &labels) { labels_ = labels; }

  virtual void SerializeAndReset(std::stringstream *ss1) {
    SerializeToStream(ss1);
    Reset(false);
  }

  virtual std::string ToString() const {
    std::stringstream ss;
    SerializeToStream(&ss);
    return ss.str();
  }

  virtual std::string ToPromeString() const {
    std::stringstream ss;
    SerializeToPromeStream(&ss);
    return ss.str();
  }

  virtual void Reset(bool is_complete_reset) = 0;
  virtual void SerializeToStream(std::stringstream *ss1) const = 0;
  virtual void SerializeToPromeStream(std::stringstream *ss1) const = 0;
  virtual bool SetParaNum(int num) = 0;
  virtual void Update() = 0;
  virtual size_t Size() const = 0;

 protected:
  std::string name_;
  std::string help_;
  int type_ = 0;
  std::string labels_;
  bool with_reg_ = false;
};

template <typename ValType>
class Accumulator : public StatsObj {
 public:
  Accumulator(const std::string &name, int type, int para_num)
      : StatsObj(name, type), num_(para_num) {
    assert(para_num <= STATS_MAX_PARALLEL_NUM);
    Reset(true);
  }

  Accumulator(const std::string &name, int type, int para_num, bool reg)
      : StatsObj(name, type, reg), num_(para_num) {
    assert(para_num <= STATS_MAX_PARALLEL_NUM);
    Reset(true);
  }

  void Reset(bool is_complete_reset) override {
    memset(vals_, 0, sizeof(ValType) * num_);
    if (is_complete_reset) {
      memset(total_vals_, 0, sizeof(ValType) * num_);
    }
  }

  void Update() override {}
  size_t Size() const override { return 1; }

  bool SetParaNum(int para_num) override {
    if (para_num > STATS_MAX_PARALLEL_NUM) {
      return false;
    }

    num_ = para_num;
    Reset(true);
    return true;
  }

  void SerializeToStream(std::stringstream *ss1) const override {
    std::stringstream &ss = *ss1;
    ss << "cyc_" << name_ << (labels_.empty() ? "" : "{" + labels_ + "}") << ":num=" << num_
       << " vals=";
    for (int i = 0; i < num_; ++i) {
      ss << vals_[i] << ' ';
    }
  }

  void SerializeBody(std::stringstream *ss) const override {
    ValType total = 0;
    for (int i = 0; i < num_; ++i) {
      total += total_vals_[i];
    }
    *ss << name_ << "{";
    SerializeLabel(ss);
    *ss << "} " << total << "\n";
  }

  void SerializeToPromeStream(std::stringstream *ss1) const override {
    SerializeHead(ss1);
    SerializeBody(ss1);
  }

  int para_num() const { return num_; }

  const ValType *vals() const { return vals_; }

  void Inc(int index, const ValType &delta) {
    assert(index >= 0 && index < num_ && delta >= 0);
    vals_[index] += delta;
    total_vals_[index] += delta;
  }

  ValType GetAt(int index) const {
    assert(index >= 0 && index < num_);
    return vals_[index];
  }

  ValType GetAndReset(int index) {
    assert(index >= 0 && index < num_);
    ValType v = vals_[index];
    vals_[index] = 0;
    return v;
  }

  ValType Sum() const { return StatsCalcSum<ValType>(vals_, num_); }

 private:
  ValType vals_[STATS_MAX_PARALLEL_NUM];
  ValType total_vals_[STATS_MAX_PARALLEL_NUM];
  int num_ = 0;
};

template <typename ValType>
class GaugeStatsObj : public StatsObj {
 public:
  GaugeStatsObj(const std::string &name, int type, int para_num)
      : StatsObj(name, type), num_(para_num) {
    assert(para_num <= STATS_MAX_PARALLEL_NUM);
    Reset(true);
  }

  GaugeStatsObj(const std::string &name, int type, int para_num, bool reg)
      : StatsObj(name, type, reg), num_(para_num) {
    assert(para_num <= STATS_MAX_PARALLEL_NUM);
    Reset(true);
  }

  void Reset(bool is_complete_reset) override {
    if (is_complete_reset) {
      memset(vals_, 0, sizeof(ValType) * num_);
    }
  }

  void Update() override {}
  size_t Size() const override { return 1; }
  int para_num() const { return num_; }

  bool SetParaNum(int para_num) override {
    num_ = para_num;
    Reset(true);
    return true;
  }

  void SerializeToStream(std::stringstream *ss1) const override {
    std::stringstream &ss = *ss1;
    ss << name_ << (labels_.empty() ? "" : "{" + labels_ + "}") << ":num=" << num_ << " vals=";
    for (int i = 0; i < num_; ++i) {
      ss << vals_[i] << ' ';
    }
  }

  void SerializeBody(std::stringstream *ss) const override {
    *ss << name_ << "{";
    SerializeLabel(ss);
    *ss << "} " << Sum() << "\n";
  }

  void SerializeToPromeStream(std::stringstream *ss1) const override {
    SerializeHead(ss1);
    SerializeBody(ss1);
  }

  void Update(int index, const ValType &v) {
    assert(index >= 0 && index < num_);
    vals_[index] = v;
  }

  void Add(int index, const ValType &v) {
    assert(index >= 0 && index < num_);
    vals_[index] += v;
  }

  void Sub(int index, const ValType &v) {
    assert(index >= 0 && index < num_);
    vals_[index] -= v;
  }

  ValType GetAt(int index) const {
    assert(index >= 0 && index < num_);
    return vals_[index];
  }

  ValType Sum() const { return StatsCalcSum<ValType>(vals_, num_); }

 private:
  int num_ = 0;
  ValType vals_[STATS_MAX_PARALLEL_NUM];
};

template <typename ValType>
class ScalarStatsObj : public StatsObj {
 public:
  using histogram_count_t = uint64_t;
  /*
    histogram_borders:
    顺序的阈值区间，a1 < a2 < a3 ...
    统计区间计数值：count(v<=a1), count(v <= a2), ... count(v <= aN)
   */
  ScalarStatsObj(const std::string &name, int type, int para_num,
                 const ValType *histogram_borders = NULL, int border_num = 0, bool reg = true)
      : StatsObj(name, type, reg) {
    assert(para_num <= STATS_MAX_PARALLEL_NUM);
    num_ = para_num;

    if (histogram_borders != NULL && border_num > 0) {
      for (int i = 0; i < border_num - 1; ++i) {
        assert(histogram_borders[i] < histogram_borders[i + 1]);
      }

      border_num_ = border_num;
      hist_borders_ = new ValType[border_num];
      memcpy(hist_borders_, histogram_borders, sizeof(ValType) * border_num);
      hist_counts_ = new histogram_count_t[total_histogram_cell_num()];
      total_hist_counts_ = new histogram_count_t[total_histogram_cell_num()];
    }
    Reset(true);
  }

  ~ScalarStatsObj() {
    if (hist_borders_ == NULL) {
      return;
    }

    delete[] hist_borders_;
    delete[] hist_counts_;
    delete[] total_hist_counts_;
    hist_borders_ = NULL;
    hist_counts_ = NULL;
    total_hist_counts_ = NULL;
    border_num_ = 0;
  }

  void Reset(bool is_complete_reset) override {
    memset(sums_, 0, sizeof(ValType) * num_);
    memset(mins_, 0, sizeof(ValType) * num_);
    memset(maxs_, 0, sizeof(ValType) * num_);
    memset(counts_, 0, sizeof(histogram_count_t) * num_);

    if (border_num_ > 0) {
      memset(hist_counts_, 0, total_histogram_cell_num() * sizeof(histogram_count_t));
    }

    if (!is_complete_reset) {
      return;
    }

    memset(total_sums_, 0, sizeof(ValType) * num_);
    memset(total_counts_, 0, sizeof(histogram_count_t) * num_);
    if (border_num_ > 0) {
      memset(total_hist_counts_, 0, total_histogram_cell_num() * sizeof(histogram_count_t));
    }
  }

  void Update() override {}
  size_t Size() const override { return 1; }

  bool SetParaNum(int para_num) override {
    if (para_num > STATS_MAX_PARALLEL_NUM) {
      return false;
    }

    num_ = para_num;
    if (border_num_ > 0) {
      delete[] hist_counts_;
      hist_counts_ = new histogram_count_t[total_histogram_cell_num()];
      delete[] total_hist_counts_;
      total_hist_counts_ = new histogram_count_t[total_histogram_cell_num()];
    }

    Reset(true);
    return true;
  }

  void SerializeToStream(std::stringstream *ss1) const override {
    std::stringstream &ss = *ss1;
    ss << "cyc_" << name_ << (labels_.empty() ? "" : "{" + labels_ + "}") << ":num=" << para_num()
       << " vals(avg,max,min)=";
    for (int i = 0; i < num_; ++i) {
      ss << "(" << GetAvgAt(i) << ',' << GetMaxAt(i) << ',' << GetMinAt(i) << ") ";
    }

    if (hist_borders_ != NULL) {
      ss << "\n"
         << "cyc_" << name_ << (labels_.empty() ? "" : "{" + labels_ + "}")
         << ":histogram border: (-INF,";
      for (int i = 0; i < border_num_; ++i) {
        ss << hist_borders_[i] << ',';
      }

      ss << "+INF)";

      const int kStatsCellNum = 100;
      histogram_count_t sum_counts[kStatsCellNum];
      histogram_count_t *psum_counts = sum_counts;
      int cell_num = histogram_cell_num();
      if (cell_num > kStatsCellNum) {
        psum_counts = new histogram_count_t[cell_num];
      }

      ss << "\n"
         << "cyc_" << name_ << (labels_.empty() ? "" : "{" + labels_ + "}")
         << ":histogram total(para_num=" << num_ << "):";
      GetHistCounts(psum_counts);
      InputHistogramToStream(ss, psum_counts, cell_num);

      if (psum_counts != sum_counts) {
        delete[] psum_counts;
      }

      if (num_ > 1) {
        for (int i = 0; i < num_; ++i) {
          ss << "\n"
             << "cyc_" << name_ << (labels_.empty() ? "" : "{" + labels_ + "}") << ":histogram #"
             << i << ':';
          const auto hist = GetHistCountsAt(i);
          InputHistogramToStream(ss, hist, cell_num);
        }
      }
    }
  }

  void SerializeBody(std::stringstream *ss) const override {
    histogram_count_t total_count = 0;
    ValType total_sum = 0;
    ValType min = 0;
    ValType max = 0;
    bool min_max_set = false;
    for (int i = 0; i < num_; ++i) {
      total_count += total_counts_[i];
      total_sum += total_sums_[i];
      if (counts_[i] > 0) {
        min = (!min_max_set || mins_[i] < min) ? mins_[i] : min;
        max = (!min_max_set || maxs_[i] > max) ? maxs_[i] : max;
        min_max_set = true;
      }
    }
    *ss << name_ << "_count{";
    SerializeLabel(ss);
    *ss << "} " << total_count << "\n";
    *ss << name_ << "_sum{";
    SerializeLabel(ss);
    *ss << "} " << total_sum << "\n";
    *ss << name_ << "_max{";
    SerializeLabel(ss);
    *ss << "} " << max << "\n";
    *ss << name_ << "_min{";
    SerializeLabel(ss);
    *ss << "} " << min << "\n";
    int cell_num = histogram_cell_num();
    if (cell_num == 0) {
      return;
    }

    histogram_count_t total_cells_sum = 0;
    for (int cell_index = 0; cell_index < cell_num; cell_index++) {
      histogram_count_t cell_sum = 0;
      for (int thread_index = 0; thread_index < num_; ++thread_index) {
        const histogram_count_t *items = GetTotalHistCountsAt(thread_index);
        cell_sum += items[cell_index];
      }

      // bucket le=100 包含 le=1 的计数
      total_cells_sum += cell_sum;
      *ss << name_ << "_bucket{";
      if (SerializeLabel(ss)) {
        *ss << ",";
      }
      *ss << "le=\"";

      if (cell_index == border_num_) {
        *ss << "+Inf";
      } else {
        *ss << hist_borders_[cell_index];
      }

      *ss << "\"} " << total_cells_sum << "\n";
    }
  }

  // promethues histogrm:
  // https://prometheus.io/docs/prometheus/latest/querying/functions/#histogram_quantile
  // prom的bucket是ACCUM类型，tbus2上报的是SCALAR类型，配置p99的时候，对prom提供的表达式无需额外计算rate
  void SerializeToPromeStream(std::stringstream *ss1) const override {
    SerializeHead(ss1);
    SerializeBody(ss1);
  }

  void Update(int index, const ValType &v) {
    assert(index >= 0 && index < num_);
    if (counts_[index] == 0) {
      mins_[index] = v;
      maxs_[index] = v;
    } else if (v > maxs_[index]) {
      maxs_[index] = v;
    } else if (v < mins_[index]) {
      mins_[index] = v;
    }

    counts_[index]++;
    sums_[index] += v;
    total_counts_[index]++;
    total_sums_[index] += v;

    if (hist_borders_ != NULL) {
      histogram_count_t *cells = hist_counts_ + (index * histogram_cell_num());
      histogram_count_t *total_cells = total_hist_counts_ + (index * histogram_cell_num());
      bool setted = false;
      for (int i = 0; i < border_num_; ++i) {
        if (v <= hist_borders_[i]) {
          cells[i]++;
          total_cells[i]++;
          setted = true;
          break;
        }
      }
      if (!setted) {
        cells[border_num_]++;
        total_cells[border_num_]++;
      }
    }
  }

  int para_num() const { return num_; }
  const ValType *mins() const { return mins_; }
  const ValType *maxs() const { return maxs_; }

  ValType GetAvgAt(int index) const {
    assert(index >= 0 && index < num_);
    return counts_[index] == 0 ? 0 : sums_[index] / static_cast<ValType>(counts_[index]);
  }
  ValType GetMaxAt(int index) const {
    assert(index >= 0 && index < num_);
    return maxs_[index];
  }
  ValType GetMinAt(int index) const {
    assert(index >= 0 && index < num_);
    return mins_[index];
  }

  ValType GetAvg() const {
    ValType total_sum = StatsCalcSum<ValType>(sums_, num_);
    histogram_count_t total_count = StatsCalcSum<histogram_count_t>(counts_, num_);
    return total_count == 0 ? 0 : total_sum / static_cast<ValType>(total_count);
  }

  ValType GetMax() const {
    if (num_ == 0) {
      return 0;
    }

    ValType val = maxs_[0];
    for (int i = 1; i < num_; ++i) {
      if (maxs_[i] > val) {
        val = maxs_[i];
      }
    }

    return val;
  }

  ValType GetMin() const {
    if (num_ == 0) {
      return 0;
    }

    ValType val = mins_[0];
    for (int i = 1; i < num_; ++i) {
      if (mins_[i] < val) {
        val = mins_[i];
      }
    }

    return val;
  }

  int histogram_cell_num() const {
    if (hist_borders_ == NULL) {
      return 0;
    }
    return border_num_ + 1;
  }

  const histogram_count_t *GetTotalHistCountsAt(int index) const {
    assert(index < num_ && index >= 0);
    return total_hist_counts_ + (index * histogram_cell_num());
  }

  const histogram_count_t *GetHistCountsAt(int index) const {
    assert(index < num_ && index >= 0);
    return hist_counts_ + (index * histogram_cell_num());
  }

  // counts: size = histogram_cell_num
  void GetHistCounts(histogram_count_t *counts) const {
    assert(counts != NULL);
    int cell_num = histogram_cell_num();
    memset(counts, 0, cell_num * sizeof(histogram_count_t));
    for (int i = 0; i < num_; ++i) {
      const histogram_count_t *items = GetHistCountsAt(i);
      for (int j = 0; j < cell_num; ++j) {
        counts[j] += items[j];
      }
    }
  }

 private:
  int total_histogram_cell_num() const { return num_ * histogram_cell_num(); }

  static void InputHistogramToStream(std::stringstream &ss, const histogram_count_t *hist,
                                     int cell_num) {
    auto total_count = StatsCalcSum<histogram_count_t>(hist, cell_num);
    if (total_count == 0) {
      ss << "ZERO";
      return;
    }

    double flt_total_count = static_cast<double>(total_count);
    histogram_count_t hist_count = 0;
    for (int j = 0; j < cell_num; ++j) {
      char ratio[8] = {0};
      hist_count += hist[j];
      snprintf(ratio, sizeof(ratio), "%.3f", static_cast<double>(hist_count) / flt_total_count);
      ss << ' ' << hist_count << '/' << ratio;
    }
  }

 private:
  ValType mins_[STATS_MAX_PARALLEL_NUM];
  ValType maxs_[STATS_MAX_PARALLEL_NUM];
  ValType sums_[STATS_MAX_PARALLEL_NUM];
  histogram_count_t counts_[STATS_MAX_PARALLEL_NUM];
  histogram_count_t *hist_counts_ = NULL;

  ValType total_sums_[STATS_MAX_PARALLEL_NUM];
  histogram_count_t total_counts_[STATS_MAX_PARALLEL_NUM];
  histogram_count_t *total_hist_counts_ = NULL;

  ValType *hist_borders_ = NULL;
  int border_num_ = 0;
  int num_ = 0;
};

// 简单的 summary 实现，提供最近 cap 个原始数据的百分位信息
template <typename ValType>
class SummaryStatsObj : public StatsObj {
 public:
  SummaryStatsObj(const std::string &name, int type, int para_num, int cap,
                  const std::vector<double> &quantiles, bool reg = true)
      : StatsObj(name, type, reg) {
    assert(para_num <= STATS_MAX_PARALLEL_NUM);
    assert(cap > 0);
    assert(quantiles.size() > 0);

    for (size_t i = 0; i < quantiles.size(); ++i) {
      assert(quantiles[i] >= 0 && quantiles[i] <= 1);
      if (i > 0) {
        assert(quantiles[i] > quantiles[i - 1]);
      }
    }

    cap_ = cap;
    num_ = para_num;
    quantiles_ = quantiles;
    Reset(true);
  }

  ~SummaryStatsObj() {
    if (bufs_ != NULL) {
      delete[] bufs_;
      bufs_ = NULL;
    }
  }

  void Reset(bool is_complete_reset) override {
    if (!is_complete_reset) {
      return;
    }

    memset(sums_, 0, sizeof(ValType) * num_);
    memset(counts_, 0, sizeof(unsigned int) * num_);

    if (bufs_ != NULL) {
      delete[] bufs_;
    }
    bufs_ = new std::deque<ValType>[num_];
  }

  void Update() override {}
  size_t Size() const override { return 1; }

  bool SetParaNum(int para_num) override {
    if (para_num > STATS_MAX_PARALLEL_NUM) {
      return false;
    }

    num_ = para_num;
    Reset(true);
    return true;
  }

  void SerializeToStream(std::stringstream *ss1) const override {
    std::stringstream &ss = *ss1;
    ss << name_ << (labels_.empty() ? "" : "{" + labels_ + "}") << ":num=" << num_
       << " vals(count,sum)=";
    for (int i = 0; i < num_; ++i) {
      ss << "(" << counts_[i] << ',' << sums_[i] << ")";
    }

    ss << "\n" << name_ << (labels_.empty() ? "" : "{" + labels_ + "}") << ":summary quantiles: (";
    for (size_t i = 0; i < quantiles_.size() - 1; ++i) {
      ss << quantiles_[i] << ',';
    }
    ss << quantiles_.back() << ')';

    ss << "\n"
       << name_ << (labels_.empty() ? "" : "{" + labels_ + "}")
       << ":summary total(para_num=" << num_ << "): ";

    std::deque<ValType> all;
    for (auto i = 0; i < num_; ++i) {
      all.insert(all.end(), bufs_[i].begin(), bufs_[i].end());
    }
    InputSummaryToStream(ss, all);

    if (num_ > 1) {
      for (int i = 0; i < num_; ++i) {
        ss << "\n"
           << name_ << (labels_.empty() ? "" : "{" + labels_ + "}") << ":summary #" << i << ": ";

        std::deque<ValType> buf = bufs_[i];
        InputSummaryToStream(ss, buf);
      }
    }
  }

  void SerializeBody(std::stringstream *ss) const override {
    ValType sum = 0;
    auto count = 0;
    for (int i = 0; i < num_; ++i) {
      sum += sums_[i];
      count += counts_[i];
    }
    *ss << name_ << "_sum{";
    SerializeLabel(ss);
    *ss << "} " << sum << "\n";
    *ss << name_ << "_count{";
    SerializeLabel(ss);
    *ss << "} " << count << "\n";

    std::deque<ValType> all;
    for (auto i = 0; i < num_; ++i) {
      all.insert(all.end(), bufs_[i].begin(), bufs_[i].end());
    }
    std::sort(all.begin(), all.end());

    if (all.size() == 0) {
      return;
    }

    for (size_t i = 0; i < quantiles_.size(); ++i) {
      *ss << name_ << "{";
      if (SerializeLabel(ss)) {
        *ss << ',';
      }
      *ss << "quantile=\"" << quantiles_[i] << "\"} " << std::fixed << std::setprecision(2)
          << CalcQuantile(all, quantiles_[i]) << "\n";
    }
  }

  void SerializeToPromeStream(std::stringstream *ss1) const override {
    SerializeHead(ss1);
    SerializeBody(ss1);
  }

  void Update(int index, const ValType &v) {
    assert(index >= 0 && index < num_);
    counts_[index]++;
    sums_[index] += v;

    auto &buf = bufs_[index];
    if (buf.size() == static_cast<size_t>(cap_)) {
      buf.pop_front();
    }
    buf.push_back(v);
  }

 private:
  void InputSummaryToStream(std::stringstream &ss, std::deque<ValType> &buf) const {
    if (buf.size() == 0) {
      ss << "ZERO";
      return;
    }

    std::sort(buf.begin(), buf.end());
    for (size_t i = 0; i < quantiles_.size(); ++i) {
      ss << CalcQuantile(buf, quantiles_[i]) << ' ';
    }
  }

  // 通过线性插值法计算百分位
  static double CalcQuantile(const std::deque<ValType> &buf, double quantile) {
    assert(quantile >= 0 && quantile <= 1);
    assert(buf.size() > 0);

    double pos = quantile * static_cast<double>(buf.size() - 1);
    size_t idx = static_cast<size_t>(pos);
    if (idx == buf.size() - 1) {
      return static_cast<double>(buf[idx]);
    } else {
      double frac = pos - static_cast<double>(idx);
      return static_cast<double>(buf[idx]) * (1 - frac) + static_cast<double>(buf[idx + 1]) * frac;
    }
  }

 private:
  ValType sums_[STATS_MAX_PARALLEL_NUM];
  unsigned int counts_[STATS_MAX_PARALLEL_NUM];

  int num_ = 0;
  int cap_ = 0;
  std::deque<ValType> *bufs_ = NULL;
  std::vector<double> quantiles_;
};

using AccumInt64 = Accumulator<int64_t>;
using AccumDouble = Accumulator<double>;
using GaugeInt64 = GaugeStatsObj<int64_t>;
using GaugeDouble = GaugeStatsObj<double>;
using ScalarInt64 = ScalarStatsObj<int64_t>;
using ScalarDouble = ScalarStatsObj<double>;
using SummaryInt64 = SummaryStatsObj<int64_t>;
using SummaryDouble = SummaryStatsObj<double>;

#define DEFINE_STATS_OBJ(StatsObjType, name, ...) StatsObjType g_stats_##name(#name, __VA_ARGS__)
#define DECLARE_STATS_OBJ(StatsObjType, name) extern StatsObjType g_stats_##name

#define STATS_OBJ(name) g_stats_##name

/*
  静态注册接口，适用于编译时就能确定的全局统计对象
*/

#define DEFINE_STATS_ACCUM_INT64(name, para_num) \
  DEFINE_STATS_OBJ(::MGSE_NS::AccumInt64, name, ::MGSE_NS::STATS_ACCUM_INT64, para_num)
#define DECLARE_STATS_ACCUM_INT64(name) DECLARE_STATS_OBJ(::MGSE_NS::AccumInt64, name)

#define DEFINE_STATS_ACCUM_DOUBLE(name, para_num) \
  DEFINE_STATS_OBJ(::MGSE_NS::AccumDouble, name, ::MGSE_NS::STATS_ACCUM_DOUBLE, para_num)
#define DECLARE_STATS_ACCUM_DOUBLE(name) DECLARE_STATS_OBJ(::MGSE_NS::AccumDouble, name)

#define DEFINE_STATS_GAUGE_INT64(name, para_num) \
  DEFINE_STATS_OBJ(::MGSE_NS::GaugeInt64, name, ::MGSE_NS::STATS_GAUGE_INT64, para_num)
#define DECLARE_STATS_GAUGE_INT64(name) DECLARE_STATS_OBJ(::MGSE_NS::GaugeInt64, name)

#define DEFINE_STATS_GAUGE_DOUBLE(name, para_num) \
  DEFINE_STATS_OBJ(::MGSE_NS::GaugeDouble, name, ::MGSE_NS::STATS_GAUGE_DOUBLE, para_num)
#define DECLARE_STATS_GAUGE_DOUBLE(name) DECLARE_STATS_OBJ(::MGSE_NS::GaugeDouble, name)

#define DEFINE_STATS_SCALAR_INT64(name, para_num, ...)                                    \
  static int64_t g_scalar_histogram_borders_##name[] = {__VA_ARGS__};                     \
  DEFINE_STATS_OBJ(::MGSE_NS::ScalarInt64, name, ::MGSE_NS::STATS_SCALAR_INT64, para_num, \
                   g_scalar_histogram_borders_##name,                                     \
                   sizeof(g_scalar_histogram_borders_##name) / sizeof(int64_t))

#define DEFINE_STATS_SCALAR_INT64_NO_HISTOGRAM(name, para_num) \
  DEFINE_STATS_OBJ(::MGSE_NS::ScalarInt64, name, ::MGSE_NS::STATS_SCALAR_INT64, para_num)

#define DECLARE_STATS_SCALAR_INT64(name) DECLARE_STATS_OBJ(::MGSE_NS::ScalarInt64, name)

#define DEFINE_STATS_SCALAR_DOUBLE(name, para_num, ...)                                     \
  static double g_scalar_histogram_borders_##name[] = {__VA_ARGS__};                        \
  DEFINE_STATS_OBJ(::MGSE_NS::ScalarDouble, name, ::MGSE_NS::STATS_SCALAR_DOUBLE, para_num, \
                   g_scalar_histogram_borders_##name,                                       \
                   sizeof(g_scalar_histogram_borders_##name) / sizeof(double))

#define DEFINE_STATS_SCALAR_DOUBLE_NO_HISTOGRAM(name, para_num) \
  DEFINE_STATS_OBJ(::MGSE_NS::ScalarDouble, name, ::MGSE_NS::STATS_SCALAR_DOUBLE, para_num)

#define DECLARE_STATS_SCALAR_DOUBLE(name) DECLARE_STATS_OBJ(::MGSE_NS::ScalarDouble, name)

#define DEFINE_STATS_SUMMARY_INT64(name, para_num, cap, ...)                                     \
  DEFINE_STATS_OBJ(::MGSE_NS::SummaryInt64, name, ::MGSE_NS::STATS_SUMMARY_INT64, para_num, cap, \
                   {__VA_ARGS__})
#define DECLARE_STATS_SUMMARY_INT64(name) DECLARE_STATS_OBJ(::MGSE_NS::SummaryInt64, name)

#define DEFINE_STATS_SUMMARY_DOUBLE(name, para_num, cap, ...)                                      \
  DEFINE_STATS_OBJ(::MGSE_NS::SummaryDouble, name, ::MGSE_NS::STATS_SUMMARY_DOUBLE, para_num, cap, \
                   {__VA_ARGS__})
#define DECLARE_STATS_SUMMARY_DOUBLE(name) DECLARE_STATS_OBJ(::MGSE_NS::SummaryDouble, name)

/*
  动态注册接口，适用于运行时才能确定的统计对象
*/

static inline AccumInt64 *NewStatsAccumInt64(const std::string &name, int para_num) {
  return new AccumInt64(name, STATS_ACCUM_INT64, para_num);
}

static inline AccumDouble *NewStatsAccumDouble(const std::string &name, int para_num) {
  return new AccumDouble(name, STATS_ACCUM_DOUBLE, para_num);
}

static inline GaugeInt64 *NewStatsGaugeInt64(const std::string &name, int para_num) {
  return new GaugeInt64(name, STATS_GAUGE_INT64, para_num);
}

static inline GaugeDouble *NewStatsGaugeDouble(const std::string &name, int para_num) {
  return new GaugeDouble(name, STATS_GAUGE_DOUBLE, para_num);
}

static inline ScalarInt64 *NewStatsScalarInt64(const std::string &name, int para_num) {
  return new ScalarInt64(name, STATS_SCALAR_INT64, para_num);
}

static inline ScalarDouble *NewStatsScalarDouble(const std::string &name, int para_num) {
  return new ScalarDouble(name, STATS_SCALAR_DOUBLE, para_num);
}

static inline ScalarInt64 *NewStatsHistogramInt64(const std::string &name, int para_num,
                                                  std::initializer_list<int64_t> borders) {
  return new ScalarInt64(name, STATS_SCALAR_INT64, para_num, borders.begin(),
                         static_cast<int>(borders.size()));
}

static inline ScalarDouble *NewStatsHistogramDouble(const std::string &name, int para_num,
                                                    std::initializer_list<double> borders) {
  return new ScalarDouble(name, STATS_SCALAR_DOUBLE, para_num, borders.begin(),
                          static_cast<int>(borders.size()));
}

static inline SummaryInt64 *NewStatsSummaryInt64(const std::string &name, int para_num, int cap,
                                                 std::initializer_list<double> quantiles) {
  return new SummaryInt64(name, STATS_SUMMARY_INT64, para_num, cap, quantiles);
}

static inline SummaryDouble *NewStatsSummaryDouble(const std::string &name, int para_num, int cap,
                                                   std::initializer_list<double> quantiles) {
  return new SummaryDouble(name, STATS_SUMMARY_DOUBLE, para_num, cap, quantiles);
}

// IMPORTANT: 该函数必须在系统启动时根据并发更新计数器线程数先进行设置
MGSE_API bool StatsSetParaNum(int para_num);
// 将统计信息的周期值或当前值输出至文件，并对周期值进行重置
MGSE_API void StatsLogAll();

/////////////////////////////////////////////////////////////////////////////////
MGSE_NS_END
#endif  // COMLIB_STATS_STATS_OBJ_H_