// Copyright (c) Tencent
// Author: bondshi
// Create: 2021-11-10

/**
 * tbuspp2.0 消息过滤器插件编程接口：
 *
 * 1. Agent 提供TbusppHostApi 接口，供plugin 获取到运行时信息
 * 2. Plugin 提供 TbusppFilter 对象，在 4 个Hook 点（消息输出前后、消息输入前后）扩展消息处理逻辑
 * 3. Plugin SO 需要导出 tbuspp_filter_get_filter, tbuspp_filter_free_filter 两个函数，Agent
 * 调用这两个函数 获取和释放 TbusppFilter Object
 * 4. Plugin Reload 特性支持,可执行tbus2.sh reload_plugin [agent_id] <so_path>;
 * 4.1 g++高版本(4.8.5以后)编译的Plugin使用libstdc++静态库在reload后,部分符号执行结果不符合预期（eg:
 * std::stringstream的>> <<), 要求Plugin尽量采用动态库方式链接；
 * 4.2 由于Plugin可能存在部分符号采用UNIQUE方式bind（eg:libstdc++ string),会导致reload
 * 以后旧so在agent进程中未完全unmap掉(不会影响新so使用);https://zhuanlan.zhihu.com/p/31120126
 */

// tbuspp2_filter.h
#pragma once

#include <string>
#include <vector>
#include "tbuspp2_defs.h"

////////////////////////////////////////////////////////////////////////////////

#define TBUSPP_FILTER_VERSION 7              //! 当前编译plugin后版本
#define TBUSPP_REQUIRE_MIN_FILTER_VERSION 6  //! 当前agent要求插件最低版本,不符合需重新编译
/// 插件相关函数调用环境信息
struct tbuspp_filter_ctx_t {
  int worker_id;  //! agent networker_id, Filter 回调执行环境
};

struct tbuspp_endpoint_info_t {
  tbuspp_id_t busid;
  tbuspp_id_t agent_id;
  uint64_t cookie;
  uint32_t mod_version;
  uint32_t shard_id;
  int link_status;      /// see TBUSPP_LINK_STATUS_*
  int endpoint_status;  /// see TBUSPP_ENDPOINT_STATUS_*
};

struct tbuspp_shard_weight_t {
  uint32_t shard_id;
  uint32_t weight;
};
typedef struct tbuspp_shard_weight_t tbuspp_shard_weight_t;

struct tbuspp_group_info_t {
  tbuspp_id_t gid;
  uint32_t version;
  int32_t member_count;

  //! 如果有路由策略，第一个为默认路由策略
  int32_t route_type_num;
  int32_t route_types[TBUSPP_ROUTE_TYPE_MAX_NUM];
  tbuspp_id_t master_busid;

  //! 子群数量，Member 中shard_id 的数量(不包括0)(原 sub_group_num)
  int32_t shard_num;
  const tbuspp_shard_weight_t *shard_weights;  // 在线的ep shard权重值, type: array, len = shard_num
};

// First RouteType Item is Default RouteType
static inline int tbuspp_group_default_route_type(const tbuspp_group_info_t *g) {
  return (g != NULL && g->route_type_num > 0) ? g->route_types[0] : 0;
}

enum tbuspp_filter_code_t {
  TBUSPP_FC_CONTINUE = 0,  //! 继续处理消息
  TBUSPP_FC_ABORT,         //! 放弃消息
  TBUSPP_FC_REDIR  //! 重分发，针对OnBeforeRecvMsg下p2g消息，如果业务还想在当前gid再次路由，设置该标记
};

struct tbuspp_filter_result_t {
  tbuspp_filter_code_t result;

  // msg head 中对应可修改字段的地址( OnBeforeRouting上不允许修改dest)
  tbuspp_id_t *dest;
  uint16_t *msg_type;

  // 支持消息路由参数修改，仅在 HOOK_BEFORE_SENDMSG/HOOK_BEFORE_RECVMSG 中有效
  tbuspp_outq_msg_meta_t *outq_meta;

  // 如下字段只在 OnBeforeRecvMsg 的中Agent设置, 提供插件判断
  tbuspp_id_t p2g_dest;         // p2g 选路的真实目标
  uint32_t self_group_version;  // 当前dest的group_version
};

// Tbuspp 环境为插件提供的调用接口 (called by TbusppFilter object)
class TbusppHostApi {
 public:
  virtual ~TbusppHostApi() {}

  //! @brief  write log to agent log file
  //! @param ctx  call context, 先由 agent 传递给 TbusppFilter 对象接口
  //! @param level  log level, see TBUSPP_LL_* in tbuspp2_defs.h
  virtual void Log(const tbuspp_filter_ctx_t *ctx, int level, const char *fmt, ...)
      MGSE_FUNC_ATTR((__format__(__printf__, 4, 5))) = 0;

  //! @return local agent_id
  virtual tbuspp_id_t GetAgentId(const tbuspp_filter_ctx_t *ctx) = 0;

  //! @return 0 is successful, otherwise failed
  virtual int GetGroupList(const tbuspp_filter_ctx_t *ctx, std::vector<tbuspp_id_t> *groups) = 0;

  //! @return 0 is successful, otherwise failed
  virtual int GetGroupInfo(const tbuspp_filter_ctx_t *ctx, tbuspp_id_t gid,
                           tbuspp_group_info_t *info) = 0;

  //! @return 0 is successful, otherwise failed
  virtual int GetGroupMembers(const tbuspp_filter_ctx_t *ctx, tbuspp_id_t group_id,
                              std::vector<tbuspp_id_t> *members) = 0;

  //! @return 0 is successful, otherwise failed
  virtual int GetEndpointInfo(const tbuspp_filter_ctx_t *ctx, tbuspp_id_t busid,
                              tbuspp_endpoint_info_t *info) = 0;

  //! @return 0 is successful, otherwise failed
  virtual int GetEndpointsOfAgent(const tbuspp_filter_ctx_t *ctx, tbuspp_id_t agent_id,
                                  std::vector<tbuspp_id_t> *endpoints) = 0;

  //! results store in params
  virtual int GetBusidTemplate(const tbuspp_filter_ctx_t *ctx, char *template_str, size_t size,
                               tbuspp_id_t *gid_mask) = 0;
};

#define TBUSPP_FILTER_HOOK_BEFORE_SENDMSG 0x01
#define TBUSPP_FILTER_HOOK_BEFORE_RECVMSG 0x02
#define TBUSPP_FILTER_HOOK_BEFORE_ROUTING 0x04

#define TBUSPP_FILTER_HOOK_ALL                                             \
  (TBUSPP_FILTER_HOOK_BEFORE_SENDMSG | TBUSPP_FILTER_HOOK_BEFORE_RECVMSG | \
   TBUSPP_FILTER_HOOK_BEFORE_ROUTING)

/**
 * @brief 业务实现如下回调接口，对消息进行改写或者其它处理
 *
 * Msg process flow:
 *
 * 1. outgoing msg
 *
 *   < App > [ output-mq ]  < Agent (OnBeforeSendMsg) [ group_txq/endpoint_txq ] >
 *   [ mesh-connection(tcp) ]
 *
 *   [ group-txq ]  < Agent (OnBeforeRouting) > [ endpoint_txq ]
 *
 * 2. ingoing msg
 *
 *   [ mesh-connection(tcp) ]  <Agent (OnBeforeRecvMsg) > [ input-mq ] < App >
 *
 * OnBeforeSendMsg/OnBeforeRecvMsg/OnBeforeRouting: Hook points
 */
class TbusppFilter {
 public:
  virtual ~TbusppFilter() {}

  // FilterObject 提供自身支持的 HookMask (TBUSPP_FILTER_HOOK_*)
  virtual int GetHookMask() = 0;

  // @return true when reload config success or no need to reload
  // NOTE: reload not support now
  virtual bool ReloadConfig(const tbuspp_filter_ctx_t *ctx, const char *config) = 0;

  virtual void OnBeforeSendMsg(const tbuspp_filter_ctx_t *ctx, const tbuspp_msg_t *msg,
                               tbuspp_filter_result_t *res) = 0;

  virtual void OnBeforeRecvMsg(const tbuspp_filter_ctx_t *ctx, const tbuspp_msg_t *msg,
                               tbuspp_filter_result_t *res) = 0;

  // BeforeRouting 在执行Group 路由选择前调用, 保障GroupInfo 为最新版本, 这里不可修改dest
  virtual void OnBeforeRouting(const tbuspp_filter_ctx_t *ctx, const tbuspp_msg_t *msg,
                               tbuspp_filter_result_t *res) = 0;
};

//! msg layout
//!  msg := [head][body]
//!  body:= [meta][app-msg-data]
//!  meta is empty when head.meta_size = 0
//!  msg_size = head_size + meta_size + app-msg-data_size

static inline uint32_t tbuspp_get_msg_data_size(const tbuspp_msg_t &msg) {
  return msg.head.size - static_cast<uint32_t>(sizeof(tbuspp_msg_head_t)) - msg.head.meta_size;
}

static inline const char *tbuspp_get_msg_data_ptr(const tbuspp_msg_t &msg) {
  return msg.body + msg.head.meta_size;
}

//!
//! filter object 创建参数
//!
struct tbuspp_filter_param_t {
  int host_version;  //! 当前agent 提供接口版本  <in>
  int self_version;  //! 编程生成 plugin 时的接口版本  <out>
  TbusppHostApi *api;
  const char *config;
};

#ifdef OS_WINDOWS
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/**
 * @brief plugin SO 中需要实现如下函数，导出和销毁 TbusppFilter 对象
 *
 * 如果agent 启动是配置了支持 filter plugin，会在每个networker thread 中分别调用
 * tbuspp_filter_get_filter(), tbuspp_filter_free_filter()，agent 默认是为每个worker 创建不同的
 * filter 对象，如果 plugin 实现为多个worker 内共享 filter object,
 * 则自身需要做好线程安全相关处理，并尽量避免加锁，以降低对并发能力的影响
 *
 * plugin 将自身接口版本(TBUSPP_FILTER_VERSION) 通过 param->self_version 带出
 *
 * 调用时刻：
 * 1. networker thread 启动与退出时
 * 2. plugin hot update 时 （暂未支持plugin hot update） *
 */
extern "C" DLLEXPORT TbusppFilter *tbuspp_filter_get_filter(const tbuspp_filter_ctx_t *ctx,
                                                            tbuspp_filter_param_t *param);

extern "C" DLLEXPORT void tbuspp_filter_free_filter(const tbuspp_filter_ctx_t *ctx,
                                                    TbusppFilter *filter);

////////////////////////////////////////////////////////////////////////////////
