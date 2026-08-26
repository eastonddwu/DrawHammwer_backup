// Copyright (c) Tencent
// Author: bondshi
// Create: 2021-05-07
// Encoding: utf-8

#ifndef TBUSPP2_INC_TBUSPP2_H_
#define TBUSPP2_INC_TBUSPP2_H_

#include "comlib/busid/busid_ops.h"
#include "tbuspp2_defs.h"

////////////////////////////////////////////////////////////////////////////////

/**
  tbuspp2 api 线程安全原则说明

  1. 在不同线程中操作不同的 endpoint 对象（包括其 queue 对象）是安全的
  2. 对同一endpoint 对象操作：
     - 与 agent 存在调用的函数不是线程安全：tbuspp_update, tbuspp_join_group, tbuspp_exit_group,
       tbuspp_query_endpoint_status, tbuspp_query_group_info,
       tbuspp_subscribe_group, tbuspp_unsubscribe_group
     - 其它线程安全：tbuspp_set_callback, tbuspp_get_input_queue, tbuspp_get_output_queue,
       tbuspp_get_busid, tbuspp_get_agentid, tbuspp_endpoint_fd, tbuspp_is_ready
  3. 对 mq 对象的操作不是线程安全的，不能同时在多个线程中对相同mq 对象进行读或者写操作
  4. 对同一 endpoint 对象的 output_mq, input_mq 对象划分到两个线程中操作是安全的
  5. 对 endpoint 操作（包括其所属 queue 对象），需要在 endpoint 对象生存期内(tbuspp_open()
  返回开始，tbuspp_close()关闭结束) 进行
  6. 全局函数线程安全：tbuspp_open, tbuspp_error_string, tbuspp_logging_set_printer
 */

#ifdef __cplusplus
extern "C" {
#endif

// PY_CFFI_START

struct tbuspp_endpoint_t;
struct tbuspp_queue_t;
struct tbuspp_event_t;

typedef struct tbuspp_endpoint_t tbuspp_endpoint_t;
typedef struct tbuspp_queue_t tbuspp_queue_t;
typedef struct tbuspp_msg_t tbuspp_msg_t;
typedef struct tbuspp_event_t tbuspp_event_t;

// ret = cb(ep, evt, udata)
// if call tbuspp_close(ep) in cb, then must return -1
typedef int (*tbuspp_event_cb_t)(tbuspp_endpoint_t *ep, const tbuspp_event_t *evt, void *udata);

#pragma pack(push, 8)
/** endpoint 实例注册配置信息
 *
 * tbuspp_open 参数，设置 endpoint 基础属性
 * \sa tbuspp_open()
 */
struct tbuspp_endpoint_conf_t {
  /**
   * local agent endpoint url，格式: [proto://]<host>:<port>, []内可选，<>内必填
   *
   * proto: 目前只支持tcp, 默认是tcp
   * 如果为空，则默认使用：tcp://127.0.0.1:10708
   */
  char agent_url[TBUSPP_AGENT_URL_MAX_SIZE];

  /**
   * endpoint 唯一标识，tbuspp_id_t 是uint64_t, 目前只作为uint32_t 使用。
   * 支持按照与tbus 相同的 busid template 方式规划busid，但 tbus2 目前使用主机序保存busid
   * （tbus内部使用网络序）。
   *
   * 在 busid 基础，扩展gid 概念，gid 是一种特殊形式的busid，用以标识一个组：gid = busid & gid_mask
   * 系统内部只能拥有一个 busid template 与 gid_mask
   *
   * 关于busid 相关操作函数 参见 comlib/busid/busid_ops.h
   *
   * 默认 busid template: 8.8.8.8
   * 默认 gid_mask: 0xFFFF0000
   *
   * 如果注册时 busid 设置为group_id, 则系统会自动分配一个busid (namesvr 中可配置自动分配 busid
   * 的范围) 若 busid 为 0，则使用 busid_str 作为标识
   *
   * `busid` 与 下面 `busid_str` 两者意义相同，只需提供其中之一即可，
   * 一般可以直接使用 busid_str 设置字符串形式 busid
   */
  tbuspp_id_t busid;

  /**
   * busid 的字符串形式，使用字符串形式，可以省略为AppSvr 配置 busid_template, gid_mask。
   * 注册过程中 agent 返回 这两项，api 自动为 AppSvr 进行配置
   * 若是以字母开头，则表示使用别名注册。
   */
  char busid_str[TBUSPP_ALIAS_MAX_SIZE + 1];

  /**
   * ns模式下启动后，先不参与到 group，业务调用 `tbuspp_join_group` 后再加入
   */
  bool start_standby;

  /**
   * ns模式下设置 prefer_runas_last_status = true 时，尽可能以上一次运行状态方式注册，
   * 如果上一次状态ns无法获取到，采用 start_standby 设置的参数启动；注册成功后，
   * 可以使用 tbuspp_query_endpoint_status 在 api 侧查询自身状态
   * 适用于业务运行状态和启动状态不一致，热更，异常重启时，不期望group路由发生变动。
   *
   * 特别地：对于需要业务层参与协商的有状态组的成员，如果在参与自身join_group/exit_group
   * 变更协商过程中出现crash，开启该选项才能快速重新启动并继续参与变更协商；未开启则需要等
   * 到变更协商失败后才能重新启动成功。
   */
  bool prefer_runas_last_status;

  /**
   * appsvr 模块版本，版本号格式：major << 16 | minor << 8 | patch
   */
  uint32_t mod_version;

  /**
   * 当使用自动 busid 分配时，如果 cookie 不为 0，namesvr 会记录新分配的 busid 和该
   * cookie 的映射关系，此时该 cookie 相当于业务自定义的组内实例 ID，当后续注册提供相
   * 同的 cookie 时，namesvr 会保证分配对应的 busid，除非长期未使用被过期淘汰。
   * 注意：如果 cookie 已被其他在线实例占用，则后续注册请求会失败，业务需要保证 cookie
   * 在 group 下的唯一性。
   */
  uint64_t cookie;

  /**
  如果是 async open(wait_ms=0), 可以先设置回调接口，获得 open 完成通知,
  也可以轮询查询 endpoint 状态。
  \sa tbuspp_is_ready()
  */
  tbuspp_event_cb_t cb;
  void *cb_udata;  // 透传给 cb 的参数: cb(endpoint, evt, cb_udata)

  /**
   * endpoint的子域划分shard_id，shard_id相同的endpoint划分到同一个subGroup
   *
   * ModHash/ConsistentHash/Master选路通过指定shard_id，在SubGroup选路
   */
  uint32_t shard_id;

  /**
   * 业务keepalive状态维护标记, 默认false
   * 如果设置为true,需要业务周期发送心跳报文(周期调用tbuspp_update即可)，否者超过30s强制下线(-mq_space_hb_expire参数
   * or tbuspp_set_restart_reserve_time可修改超时时间) 若未设置为false,
   * agent代理endpoint保活(agent通过和ep信令面建立的socket判定是否在线，socket断开超时则强制下线)
   * \sa tbuspp_set_keepalive_with_ping()
   * \sa tbuspp_set_restart_reserve_time()
   */
  bool keepalive_with_ping;

  /**
   * 如果使用别名注册，退出时是否保存当前别名分配的busid，默认false
   */
  bool keep_alias_when_close;

  /*
   * 设置热更期间或者Api和Agent异常断开时，Agent保存队列最大时间，unit(second)
   * \sa tbuspp_set_restart_reserve_time()
   */
  int restart_reserve_time;

  /*
   * 当节点异常退出时，agent直接将目标注销
   * 适用节点本身不具备快速恢复能力，如crash/oom等场景下，业务重启恢复时间超过restart_reserve_time
   * 在restart_reserve_time期间内，agent未将目标下线，其他节点的消息依然能够选路到这个节点，
   * 这部分消息会由于等待处理时间太长导致超时。
   *
   * 开启keepalive_with_ping场景下，不适合通过设置较短的restart_reserve_time避免如上场景，
   * 容易由于机器卡顿导致节点震荡上下线。
   * \sa tbuspp_set_close_by_unexpect_exit;
   */
  bool close_by_unexpect_exit;

  /**
   * 要求agentd的最低版本，版本号格式：major << 16 | minor << 8 | patch
   * 如果为0，则不检查
   */
  uint32_t require_agent_version;

  /**
   * 要求ns的最低版本，版本号格式：major << 16 | minor << 8 | patch
   * 如果为0，则不检查
   */
  uint32_t require_ns_version;
};

typedef struct tbuspp_endpoint_conf_t tbuspp_endpoint_conf_t;

struct tbuspp_context_t {
  uint32_t size;  //! struct size, 避免使用未初始化的无效结构实例 \sa tbuspp_init_context()
  uint32_t reserved1;
  /*
   * call_id 异步请求时设置参数，TBUSPP_EVT_*事件通告时携带回来，适用于业务异步处理上下文
   */
  uint64_t call_id;

  /*
   * 同call_id实现，支持业务自定义扩展, meta 限制长度最大 TBUSPP_CONTEXT_MAX_UDATA_SIZE
   * 暂不支持 tbuspp_open, tbuspp_connect, tbuspp_register 接口
   */
  uint32_t udata_size;
  char udata[TBUSPP_CONTEXT_MAX_UDATA_SIZE];
};

typedef struct tbuspp_context_t tbuspp_context_t;

// PY_CFFI_END

static inline void tbuspp_init_context(tbuspp_context_t *p) {
  memset(p, 0, sizeof(tbuspp_context_t));
  p->size = sizeof(tbuspp_context_t);
}

// PY_CFFI_START

/**
 * \sa tbuspp_queue_get_status()
 */
struct tbuspp_queue_status_t {
  uint64_t capacity;
  uint64_t msg_size;

  /**
   * extent_info 获取存在额外成本，按需返回
   */
  struct {
    uint32_t msg_num;
  } extent_info;
};

typedef struct tbuspp_queue_status_t tbuspp_queue_status_t;

#define MQ_STATUS_FLAG_MSG_NUM 0x01

enum tbuspp_trace_span_kind_t {
  TBUSPP_SPAN_KIND_UNSPECIFIED = 0,
  // default
  TBUSPP_SPAN_KIND_INTERNAL = 1,
  // server
  TBUSPP_SPAN_KIND_SERVER = 2,
  // client
  TBUSPP_SPAN_KIND_CLIENT = 3,
  // producer
  TBUSPP_SPAN_KIND_PRODUCER = 4,
  // consumer
  TBUSPP_SPAN_KIND_CONSUMER = 5,
};
typedef enum tbuspp_trace_span_kind_t tbuspp_trace_span_kind_t;

struct tbuspp_trace_span_status_t {
  // TBUSPP_SPAN_STATUS_CODE_XXX, 允许业务自定义code直接赋值
  int code;
  char message[TBUSPP_SPAN_STATUS_ERR_SIZE + 1];
};
typedef struct tbuspp_trace_span_status_t tbuspp_trace_span_status_t;

struct tbuspp_otlp_attr_t {
  // key/value max size TBUSPP_OTLP_ATTR_KEY_VALUE_MAX_SIZE
  const char *key;
  const char *value;
};
typedef struct tbuspp_otlp_attr_t tbuspp_otlp_attr_t;

// 父子 Span 传播的 Ctx。如果是跨进程通信，通过 param.span_ctx 携带到对端
struct tbuspp_trace_span_ctx_t {
  /**
   * 必填字段
   * 调用 tbuspp_trace_start_span 全部自动生成；
   * trace_id&span_id生成算法是std::mt19937_64，seed为AgentId和nowTime
   */
  char trace_id[TBUSPP_TRACE_ID_SIZE + 1];
  char span_id[TBUSPP_SPAN_ID_SIZE + 1];

  /**
   * flags 字段基于 otlp traces.proto 的 SpanFlags 定义，具体参考如下：
   * https://github.com/open-telemetry/opentelemetry-proto/blob/main/opentelemetry/proto/trace/v1/trace.proto
   *
   * flags 有两个作用，通过对应的位判断，相关宏定义TBUSPP_SPAN_FLAGS_*：
   * 1. 是否远端：root_span默认is_remote为false。当 root_span的 span_ctx 被跨进程传递后，flags
   *    的对应第8和第9位自动设置为1，表示是 is_remote 为true。
   * 2. 是否采样：本地创建的 root_spans 默认为不采样；跨进程 child_spans 默认由 root_spans 传递的
   *    span_ctx.flags 决定
   *    1. 业务可以对所有 span 设置 flags |= TBUSPP_SPAN_FLAGS_SAMPLED_MASK，表示强制上报。
   *    2. 本地 root_spans 是否采样，通过 Agent 结合 span_attr 确定采样结果。并将结果赋值给 flags
   *       传递给跨进程的 child_spans
   *    3. 跨进程 child_spans 是否采样，由继承的 flags & TBUSPP_SPAN_FLAGS_SAMPLED_MASK 决定，为
   *       false 则不上报
   * 3. 默认 child_span_ctx 会自动继承 parent_span_ctx 的 Flags 结果，并一直传递。
   */
  uint32_t flags;

  /**
   *
   * Span 的属性，end_span 会上报到服务中心，并支持结合 NameSvr 的过滤规则在 Agent
   * 进行过滤采样结果。
   */
  uint16_t attrs_num;
  const tbuspp_otlp_attr_t *attrs;

  /*
   * 自定义信息, 创建根span的时候填入，子span通过 tbuspp_trace_start_span 自动继承。支持 binary data
   */
  uint16_t udata_size;
  char udata[TBUSPP_SPAN_UDATA_MAX_SIZE + 1];
};
typedef struct tbuspp_trace_span_ctx_t tbuspp_trace_span_ctx_t;

// Span 描述信息
struct tbuspp_trace_span_info_t {
  /**
   * 调用 tbuspp_trace_span_info_init 自动初始化为当前结构体大小
   */
  uint16_t info_size;

  /**
   * 要关联的 resource_id，需要提前通过 tbuspp_otlp_set_resource 设置，
   * 如果不需要关联其他的 Resource 属性，填零即可，Agent 只会添加默认的属性
   */
  uint16_t resource_id;

  /**
   * 部分字段必填
   * 父子 Span 传播的字段。调用 tbuspp_trace_start_span 自动继承
   */
  tbuspp_trace_span_ctx_t span_ctx;

  /**
   * 必填字段
   */
  char span_name[TBUSPP_ALIAS_MAX_SIZE + 1];

  /**
   * 必填字段
   * 调用 tbuspp_trace_start_span 自动生成 start_time;
   * 调用 tbuspp_trace_end_span 自动填充 end_time(如果end_time不为0)
   */
  uint64_t start_time_us;
  uint64_t end_time_us;

  /**
   * 必填字段
   * 调用 tbuspp_trace_start_span 自动赋值为 span_ctx.span_id
   */
  char parent_span_id[TBUSPP_SPAN_ID_SIZE + 1];

  /**
   * 可选字段
   * Span 状态
   */
  tbuspp_trace_span_kind_t kind;
  tbuspp_trace_span_status_t status;
};
typedef struct tbuspp_trace_span_info_t tbuspp_trace_span_info_t;
struct tbuspp_metrics_data_t {
  /**
   * 要关联的 resource_id，需要提前通过 tbuspp_otlp_set_resource 设置，
   * 如果不需要关联其他的 Resource 属性，填零即可，Agent 只会添加默认的属性
   */
  uint16_t resource_id;

  /**
   * 业务要上报的指标数据，保存的内容必须为 OTLP 协议中 ScopeMetrics 结构体的序列化结果，
   * Agent 会解析并补充 Resource 信息，ScopeMetrics 结构体定义可以参考：
   * https://github.com/open-telemetry/opentelemetry-proto/blob/main/opentelemetry/proto/metrics/v1/metrics.proto
   */
  size_t size;
  const char *data;
};

typedef struct tbuspp_metrics_data_t tbuspp_metrics_data_t;

/// 消息发送参数(写入)
struct tbuspp_msg_param_t {
  uint16_t param_size;  //! struct size, 避免使用未初始化的无效结构实例

  /*
   * 透传到目标节点，接收侧获取消息时可通过tbuspp_msg_desc_t->msg_type字段判断
   * 举例场景：由于不同模块之间可能存在协议不同，可用于定义消息发送者的模块类型，方便接收侧读取消息时区分解析
   * 部分值已经被内部系统占用，值参考TBUSPP_MSG_TYPE_*的定义, 业务自定义需要避开保留值.
   */
  uint16_t msg_type;

  // 设置消息发送控制标志：TBUSPP_MSG_FLAG_* (tbuspp2_defs.h)
  uint16_t usr_flags;
  char reserved1[2];

  /**
   * shard 选路规则：
   * 寻路目标，目标结点注册的shard_id用来划分subGroup。
   * 当shard_id != 0 && conf.shard_route_policy !=
   *GROUP_SHARDING_OFF，优先尝试选择shard_id对应的SubGroup进行寻路。 当shard_id == 0 ||
   *(conf.shard_route_policy == GROUP_SHARDING_SAFE &&
   *当前Group没有此shard_id的时候)，则在全组寻路。
   * 1. M_HASH:
   * 会建立基于全组和每个shard_id对应的SubGroup的M_Hash。然后shard选路规则选择指定的M_Hash寻路，寻路成功返回指定节点，不成功报错。
   * 2. C_HASH:
   * 会建立基于全组和每个shard_id对应的SubGroup的C_Hash。然后shard选路规则选择指定的C_Hash寻路，寻路成功返回指定节点，不成功报错。
   * 3. MASTER:
   * 会建立基于全组和每个shard_id对应的SubGroup的Master。然后shard选路规则选择指定的Master，寻路成功返回指定节点，不成功报错。
   * 4. RANDOM:
   * 会建立基于全组和每个shard_id对应的SubGroup的List。然后shard选路规则随机选择目标（本质是轮询），寻路成功返回指定节点，不成功报错。
   **/
  uint32_t shard_id;
  char reserved2[4];

  tbuspp_id_t origin_busid;  //! 真实消息来源节点ID (异构系统代理消息)

  // 消息路由策略设置，还需根据需要设置 route_param_type 以及相关参数;
  // 若 require_route_type=0，将使用 GroupConf
  // 的第一个路由策略作为默认策略(GroupConf.route_types[0]), 若 reuire_route_type 值并不包含在dest
  // 对应Group 配置的 route_types 中，则会发送失败，丢弃消息
  tbuspp_route_type_t require_route_type;
  char reserved3[4];

  /**
   * 版本寻路参数，目标节点版本(endpoint.module_version) 需要在 [min_require_version,
   * max_require_version] 中，若某项为0， 则表示不关心下界(min_version)
   * 或者上界(max_version)，若均为0，则表示任意版本均匹配。
   *
   * 版本寻路是基于基本路由策略之上的约束条件，叠加使用: VersionFilter( RouteSelect(g) ->
   * CandidateTargets)
   *
   * 若最终使用 ROUTE_TYPE_RANDOM 则叠加版本约束条件，选择符合要求的节点
   */
  uint32_t min_require_version;
  uint32_t max_require_version;

  // 如果 hash_key != 0, 并且下面 require_route_type!=TBUSPP_ROUTE_TYPE_RANDOM,
  // 则认为调用者要求采用Hash寻路方式(C_HASH or M_HASH), 忽略 min_require_version,
  // max_require_version 参数(仅在 RANDOM 路由下有效)
  uint64_t hash_key;

  // 多播（Multicast）：extra_dest_num > 0，消息将发送到 [dest] + [extra_dests]
  // 指定的所有目标（可以包含 gid）, extra_dest_num 为 extra_dests 指向数据元素个数。
  // 特别的，若 dest==0, 则会忽略dest指向目标，仅发送给 extra_dests 中包含目标
  // extra_dest_num数量上限TBUSPP_MAX_EXTRA_DESTS, 超过业务侧分多条消息发送
  uint16_t extra_dest_num;
  char reserved4[6];

  tbuspp_id_t *extra_dests;
  // span_ctx 不为null，对端 peek 消息后从 desc->span_ctx 获取
  tbuspp_trace_span_ctx_t *span_ctx;

  // 支持以目标别名发送
  // 如果dest_alias不为null，且以 '@{domain_id}' 结尾，则视作跨域消息
  // 其中domain_id为业务的目标集群id，如 'world.room.instance@1'
  // \sa tbuspp_busid_alias_set_domain_id()
  const char *dest_alias;

  // 如果设置了msg_id, agent将信息透传到对端，可在 tbuspp_msg_desc_ex_t中获取 msg_id
  // 支持消息打回特性， \sa tbuspp_msg_sendback_level_t
  uint64_t msg_id;

  // 设置消息agent无法处理时，是否打回
  tbuspp_msg_sendback_level_t sendback_level;
};

typedef struct tbuspp_msg_param_t tbuspp_msg_param_t;

// PY_CFFI_END

static inline void tbuspp_init_msg_param(tbuspp_msg_param_t *p) {
  memset(p, 0, sizeof(tbuspp_msg_param_t));
  p->param_size = sizeof(tbuspp_msg_param_t);
}

// PY_CFFI_START

/// 消息描述信息(读取)
struct tbuspp_msg_desc_t {
  tbuspp_id_t src;
  tbuspp_id_t dest;
  tbuspp_id_t proxy;

  /**
    消息写入到输入队列时间（绝对时间，单位: us), 业务可以根据这个时间过滤掉
    过期消息
   */
  uint64_t ctime;

  // \sa tbuspp_msg_param_t->msg_type
  uint16_t msg_type;
  /*
    组路由下，业务可以通过获取发送方发送消息时的group version做特殊处理
    group_version=0, 则表示并非P2G路由消息(针对广播，随机消息，group_version也为0)
  */
  uint32_t group_version;
};

typedef struct tbuspp_msg_desc_t tbuspp_msg_desc_t;

/* tbuspp_msg_desc_ex_t为tbuspp_msg_desc_t后续版本扩展,部分字段含义相同
   使用前需要调用tbuspp_init_msg_desc_ex进行初始化，否则读取消息时，无法解析desc内容
   \sa tbuspp_queue_peek_desc_ex()/tbuspp_msg_get_data_ex()/tbuspp_queue_read_ex()
 */
struct tbuspp_msg_desc_ex_t {
  // 内部使用
  uint16_t desc_size;
  uint16_t reserved1;

  // 同tbuspp_msg_desc_t定义
  tbuspp_id_t src;
  tbuspp_id_t dest;
  tbuspp_id_t proxy;
  uint64_t ctime;
  uint16_t msg_type;
  uint32_t group_version;

  // extern
  tbuspp_trace_span_ctx_t *span_ctx;
  uint64_t msg_id;
  tbuspp_msg_sendback_level_t sendback_level;

  // 消息被打回原因, 支持 tbuspp_error_string 打印错误
  // \sa TBUSPP_MSG_SENDBACK_LEVEL_XXX
  tbuspp_sendback_reason_t reason;

  // 消息以dest_alias发送时，会携带发送方的alias
  // 当发送方以 a.b.c.d 即endpoint alias注册时，会携带endpoint alias
  // 当发送方以 a.b.0.1 的拼接alias注册时，会携带a.b.0.1
  // 当发送方以 a.b.*.* 的group alias注册时，会携带a.b.*.*
  // 当发送方未以上述形式注册，则不携带
  char src_alias[TBUSPP_ALIAS_MAX_SIZE + 1];
  // 发送方填入的dest_alias
  char dest_alias[TBUSPP_ALIAS_MAX_SIZE + 1];
};

typedef struct tbuspp_msg_desc_ex_t tbuspp_msg_desc_ex_t;

// PY_CFFI_END

static inline void
#ifdef __cplusplus
tbuspp_init_msg_desc_ex(tbuspp_msg_desc_ex_t *p, tbuspp_trace_span_ctx_t *span_ctx = NULL) {
#else
tbuspp_init_msg_desc_ex(tbuspp_msg_desc_ex_t *p, tbuspp_trace_span_ctx_t *span_ctx) {
#endif
  memset(p, 0, sizeof(tbuspp_msg_desc_ex_t));
  p->desc_size = sizeof(tbuspp_msg_desc_ex_t);
  if (span_ctx != NULL) {
    p->span_ctx = span_ctx;
  }
}

// PY_CFFI_START

struct tbuspp_tls_conf_t {
  /*!
    采用动态加载tls库的方式实现调用open ssl接口
    ssl_crypto_so_path = NULL, 使用默认值(linux/mac:"lib/libcrypto.so",
      win: "lib\\libcrypto.dll")
    ssl_so_path = NULL, 使用默认值(linux/mac:"lib/libssl.so",
      win: "lib\\libssl.dll")
  */
  const char *ssl_crypto_so_path;
  const char *ssl_so_path;
  // CA 证书，可通过gen_tls_cert.sh生成，具体查看官方远程队列文档描述
  const char *agent_ca_pem_file;
};
typedef struct tbuspp_tls_conf_t tbuspp_tls_conf_t;

/*!
  获取 tbuspp endpoint 句柄，endpoint 将与agent 建立本地连接(loopback tcp or unix connection)
    wait_ms = 0: 异步建立连接与握手，app 可以通过 `TBUSPP_EVT_FINISH_OPEN` 通知，
    或者调用 `tbuspp_is_ready` 获得当前状态, ready 之后，才能获取到队列对象
  wait_ms > 0: 同步握手，在设定时间内（毫秒数）等待握手应答
  wait_ms < 0: 无意义, 直接返回 NULL
  ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
  err 如果 err != NULL, 则保存错误码
  return: 成功返回 tbuspp_endpoint_t 对象，失败返回 NULL

  有状态组注意事项：
  - 对于 GroupConf.group_trans_level = 3 的组，err 会返回 Errs::NS_ERR_IN_PROGRESS (参考
  tbus2_defs.proto), IN_PROGRESS 期间能够正常进行消息收发；
  - 返回 Errs::NS_ERR_IN_PROGRESS 不一定需要业务层实际参与, 如果需要业务层参与变更,
  会收到协调通知(TBUSPP_EVT_GROUP_TRANS_EVT), 业务层需要通过 tbuspp_report_trans_state
  接口推动协商流程；
  - 在整个协商流程完成后，业务层会收到 TBUSPP_EVT_SET_STATUS_RES 事件，通知变更结果 & 最新状态；
  - 业务层如果在等待变更结果超时后进行重试, 需注意当 group 在进行其他变更时 join_group/exit_group
  可能会返回错误
*/
tbuspp_endpoint_t *tbuspp_open(const tbuspp_endpoint_conf_t *conf, int wait_ms,
                               const tbuspp_context_t *ctx, int *err);
void tbuspp_close(tbuspp_endpoint_t *self);

/*!
  tbuspp远程队列使用加密功能设置，非必要，全局只需要设置一次即可
*/
int tbuspp_tls_init(const tbuspp_tls_conf_t *conf);
void tbuspp_tls_fini();

/*!
  tbuspp_open = tbuspp_connect + tbuspp_register();
  tbuspp_connect支持连接agent但是不注册，连接agent后可以使用查询api接口获取信息后再进行注册节点
  \sa tbuspp_is_connected() 可以查询当前是否和agent建立连接
*/
tbuspp_endpoint_t *tbuspp_connect(const char *agent_url, int wait_ms, const tbuspp_context_t *ctx,
                                  int *err);
int tbuspp_register(tbuspp_endpoint_t *self, const tbuspp_endpoint_conf_t *conf, int wait_ms,
                    const tbuspp_context_t *ctx);

/*!
  缓存endpoint共享队列超时设置:在业务退出时,将缓存当前endpoint队列直到超时
  如果显式设置 reserve_secs > 0, 则调用 tbuspp_close() 后，agent 亦会在设定间隔内保留 endpoint
  注册状态。 如希望调用 tbuspp_close() 立即注销，则需在退出前调用该Api 设置 reserve_secs 为 0.

  @param
         ep: tbuspp endpoint 句柄
         reserve_secs: endpoint 状态数据在Agent 内保留时长，若为0，则使用Agent自身设置。
                      Agent 默认在Endpoint 信令连接断开超时未恢复时清理队列资源（mq_space_hb_expire,
  default 30s） wait_ms   > 0: 最大等待时长 (milliseconds) = 0:
  则只发送请求，查询结果在tbuspp_update() 回调事件中给出 < 0: 永久等待，直到返回结果或者通讯失败
          ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
  return: 成功返回 0，失败返回 != 0
*/
int tbuspp_set_restart_reserve_time(tbuspp_endpoint_t *ep, unsigned int reserve_secs, int wait_ms,
                                    const tbuspp_context_t *ctx);

/**
 * @brief 设置endpoint心跳保活方式,支持endpoint注册后修改
 * @param enable true: endpoint自身保活,需要周期调用tbuspp_update()
 *               false: agent代理endpoint保活
 *        ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0为成功；非0失败
 */
int tbuspp_set_keepalive_with_ping(tbuspp_endpoint_t *ep, bool enable, int wait_ms,
                                   const tbuspp_context_t *ctx);

/**
 * @brief 设置endpoint异常退出时是否直接注销
 * @param enable true/false: 开启/关闭
 *        ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0为成功；非0失败
 */
int tbuspp_set_close_by_unexpect_exit(tbuspp_endpoint_t *ep, bool enable, int wait_ms,
                                      const tbuspp_context_t *ctx);

/**
 * ns模式下，退出group, 不参与群组消息接收（优雅退出, 热更重启）
 * @param wait_ms   > 0: 最大等待时长 (milliseconds)
 *                 = 0: 则只发送请求，查询结果在tbuspp_update() 回调事件中给出
 *                 < 0: 永久等待，直到返回结果或者通讯失败
 *        ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return  0 执行成功，否则失败
 * @see tbuspp_open 有状态组注意事项
 */
int tbuspp_exit_group(tbuspp_endpoint_t *self, int wait_ms, const tbuspp_context_t *ctx);
/**
 * ns模式下，加入群组, 恢复群组通信 (standby -> ready)
 * @param wait_ms   \sa tbuspp_exit_group
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return  0 执行成功，否则失败
 * @see tbuspp_open 有状态组注意事项
 */
int tbuspp_join_group(tbuspp_endpoint_t *self, int wait_ms, const tbuspp_context_t *ctx);
/**
 * 查询指定 endpoint 对象状态（READY/STANDBY/EXIT），
 * 如果 指定busid 是自身(即self)且status不为NULL，status状态从本地获取，否则请求发送至Agent
 * 如果Agent内存在对应endpoint状态，则从agent返回，否则Agent会发送查询请求至namesvr 进行查询
 * @param busid   目标busid, 如果为0，则查询自身状态
 * @param wait_ms  \sa tbuspp_join_group
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @param status  当wait_ms > 0 时，保存查询结果
 * @return  0 执行成功，否则失败
 */
int tbuspp_query_endpoint_status(tbuspp_endpoint_t *self, tbuspp_id_t busid, int wait_ms,
                                 const tbuspp_context_t *ctx, int *status);

/**
  业务需要定期调用 tbuspp_update, 或者通过 epoll 等事件驱动调用(参见 `tbuspp_endpoint_fd`)
  在tbuspp_update() 中会执行信令通道断线重连、事件通知回调等操作
  wait_ms 为等待信令通道READ 事件的最大时间，如果业务控制了调用频率，可以设置为 0,
  如果 wait_ms < 0, 将不进行 select 可读检查，适合已将信令句柄注册到调用者 epoll 中场景.

  agent 会依据 endpoint 信令连接是否存在作为 endpoint 是否存在的主要依据，如果信令连接关闭时间超过
  设定时长(--mq_space_hb_expire 参数值)，且对应 mq 均为空，则会认为endpoint 已经退出，可以清理资源。

  @return if user callback invoked and retval < 0, then return -1, otherwise 0
 */
int tbuspp_update(tbuspp_endpoint_t *self, int wait_ms);

//! callback function called in tbuspp_update()
void tbuspp_set_callback(tbuspp_endpoint_t *self, tbuspp_event_cb_t cb, void *udata);

/**
 * \defgroup queue operations
 * @{
 */
tbuspp_queue_t *tbuspp_get_input_queue(tbuspp_endpoint_t *self);
tbuspp_queue_t *tbuspp_get_output_queue(tbuspp_endpoint_t *self);

/**
 * @brief 写入消息到队列中
 *
 * @param q  output queue of endpoint
 * @param dest  target busid or group_id
 * @param msg_data
 * @param msg_size
 * @param param 消息发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_msg_param`
 * 对参数进初始化
 * @return  error code, defined in tbuspp2_defs.h (TBUSPP_ERR*), if param != NULL, but not init by
 * tbuspp_int_msg_param, then return TBUSPP_ERR_WRONG_ARG
 * @note 当环境变量 `TBUS2_IGNORE_BBD_CHECK` 被设置为 1 时，跳过 BBD 流控
 */
int tbuspp_queue_write(tbuspp_queue_t *q, tbuspp_id_t dest, const void *msg_data, uint32_t msg_size,
                       const tbuspp_msg_param_t *param);

/**
 * @brief 写入消息到队列(Hash 路由)
 *
 * @param q
 * @param dest
 * @param msg_data
 * @param msg_size
 * @param hash_key  hash_key of message
 * @return int
 */
int tbuspp_queue_write_hash(tbuspp_queue_t *q, tbuspp_id_t dest, const void *msg_data,
                            uint32_t msg_size, uint64_t hash_key);

/**
 * @brief 写入一个消息，消息数据存在与多个buffer 中
 *
 * @param q  output queue of endpoint
 * @param dest  target busid or group_id
 * @param iov  消息数据 buffer 清单
 * @param iov_num  bufer 数量
 * @param param \sa tbuspp_queue_write
 * @return int  error code, defined in tbuspp2_defs.h (TBUSPP_ERR*)
 */
int tbuspp_queue_writev(tbuspp_queue_t *q, tbuspp_id_t dest, const struct iovec *iov, int iov_num,
                        const tbuspp_msg_param_t *param);

/**
 * @brief 写入消息到队列(Hash 路由)
 *
 * @param q
 * @param dest
 * @param iov  \sa tbuspp_queue_writev
 * @param iov_num
 * @param hash_key  hash_key of message
 * @return int
 */
int tbuspp_queue_writev_hash(tbuspp_queue_t *q, tbuspp_id_t dest, const struct iovec *iov,
                             int iov_num, uint64_t hash_key);

// 直接获取消息队列 buffer，zero-copy
// 内存布局： [meta_ptr<meta_size>][msg_ptr<msg_size>]
// 业务直接操作 msg_ptr, meta_ptr 内存时，务必不能越界
// 准备好数据后，调用 `tbuspp_queue_commit_buf` 提交更新
int tbuspp_queue_lock_buf(tbuspp_queue_t *q, tbuspp_id_t dest, const tbuspp_msg_param_t *param,
                          uint32_t max_msg_size, char **msg_ptr);

int tbuspp_queue_lock_buf_hash(tbuspp_queue_t *q, tbuspp_id_t dest, uint64_t hash_key,
                               uint32_t max_msg_size, char **msg_ptr);

// zero copy 完成后，设置消息就绪
// msg_size <= max_msg_size (in tbuspp_queue_lock_buf)
int tbuspp_queue_commit_buf(tbuspp_queue_t *q, uint32_t msg_size);

// return NULL if queue is empty, otherwise is msg data ptr
// msg size saved in `size`, context saved in `desc`
const char *tbuspp_queue_peek(tbuspp_queue_t *q, uint32_t *size, tbuspp_msg_desc_t *desc);

// this api get more msg desc info(eg: span trace), Must be initialized
// before use(tbuspp_init_msg_desc_ex()), otherwise the desc content cannot be obtained
// \sa tbuspp_msg_desc_ex_t
const char *tbuspp_queue_peek_desc_ex(tbuspp_queue_t *q, uint32_t *size,
                                      tbuspp_msg_desc_ex_t *desc);

// return NULL if queue is empty, otherwise is the next msg after `last_msg`
// if `lasg_msg` is NULL, return first msg in q
// `ret` can get the error code
tbuspp_msg_t *tbuspp_queue_peek_ex(tbuspp_queue_t *q, tbuspp_msg_t *last_msg, int *ret);

// get msg data from tbuspp_msg_t
// msg size saved in `size`, context saved in `desc`
// be careful to get msg data after tbuspp_queue_pop or tbuspp_msg_mark_discard
const char *tbuspp_msg_get_data(tbuspp_msg_t *msg, uint32_t *size, tbuspp_msg_desc_t *desc);

// this api get more msg desc info(eg: span trace), Must be initialized
// before use(tbuspp_init_msg_desc_ex()), otherwise the desc content cannot be obtained
// \sa tbuspp_msg_desc_ex_t
const char *tbuspp_msg_get_data_ex(tbuspp_msg_t *msg, uint32_t *size, tbuspp_msg_desc_ex_t *desc);

// mark the given msg discarded
// be careful to discard msg after tbuspp_queue_pop
int tbuspp_msg_mark_discard(tbuspp_msg_t *msg);

/**
 * @brief read msg from input queue, equal tbuspp_queue_peek + copy_msg_data + tbuspp_queue_pop
 * @return  0: success, only success will pop msg
 *          TBUSPP_ERR_QUEUE_EMPTY: no msg
 *          TBUSPP_ERR_LESS_MSG_BUF: buffer is not enough, require buffer size saved in msg_size
 */
int tbuspp_queue_read(tbuspp_queue_t *q, void *buf, uint32_t size, uint32_t *msg_size,
                      tbuspp_msg_desc_t *desc);

// this api get more msg desc info(eg: span trace), Must be initialized
// before use(tbuspp_init_msg_desc_ex()), otherwise the desc content cannot be obtained
// \sa tbuspp_msg_desc_ex_t
int tbuspp_queue_read_ex(tbuspp_queue_t *q, void *buf, uint32_t size, uint32_t *msg_size,
                         tbuspp_msg_desc_ex_t *desc);

// pop msg in queue head (read direction)
void tbuspp_queue_pop(tbuspp_queue_t *q);

// discard all data of mq
// return 0 if success, otherwise failed
int tbuspp_queue_clear(tbuspp_queue_t *q);

// 获取与指定可发送消息窗口大小，若为0，则窗口关闭，制定目标当前繁忙
// 当 write queue 返回 TBUSPP_ERR_QUEUE_BUSY,
// 可以调用此函数判断是否为队列本身空间不足，还是对某些目标繁忙
uint32_t tbuspp_queue_get_dest_window_size(tbuspp_queue_t *q, tbuspp_id_t dest);

// extern_flags: 设置一些扩展操作标志，这些操作可能需要枚举队列，比较耗时 (MQ_STATUS_FLAG_*)
void tbuspp_queue_get_status(const tbuspp_queue_t *q, tbuspp_queue_status_t *status,
                             int extent_flags);

tbuspp_endpoint_t *tbuspp_queue_get_owner(const tbuspp_queue_t *q);

/**
 * @brief:
 * 共享内存发送队列水线设置,用于及时唤醒agent处理队列消息
 * 当outq消息数量 > water_mark,触发api发送socket唤醒agent处理消息(\sa tbuspp_push_send_notice)
 * @param water_mark 水线阀门值百分比, limit [0, 100], 默认 50
 *          = 0：开启功能,不限制水线,均唤醒agent
 *          > 0: 超过水线则唤醒agent
 *          < 0: 关闭功能
 * @return 0: 执行成功，否则失败
 */
int tbuspp_queue_set_busy_report(tbuspp_queue_t *outq, int water_mark);

/**
 * @brief endpoint写入消息后主动通知agent立即处理MQ消息，可降低消息发送1-2ms延迟和抖动
 *        该api适用于低频发包且对消息延迟及抖动敏感场景，高频发包不适用
 */
int tbuspp_push_send_notice(tbuspp_endpoint_t *self);

// end queue operations
/**@}*/

// PY_CFFI_END

// miscs

// customer log output. see comlib/defs/comdefs.h
typedef mgse_logging_context_t tbuspp_logging_context_t;
typedef void (*tbuspp_logging_printer_t)(int log_level, const char *data, int size,
                                         const tbuspp_logging_context_t *ctx);
static inline void tbuspp_logging_set_printer(tbuspp_logging_printer_t printer) {
  mgse_logging_set_printer(printer);
}
static inline void tbuspp_logging_set_level(int level) { mgse_logging_set_level(level); }
static inline int tbuspp_logging_get_level() { return mgse_logging_get_level(); }

// PY_CFFI_START

const char *tbuspp_error_string(int e);

/**
 * \defgroup endpoint info
 * @{
 */

uint32_t tbuspp_get_domain_id(tbuspp_endpoint_t *self);
const char *tbuspp_get_domain_alias(tbuspp_endpoint_t *self);
tbuspp_id_t tbuspp_get_busid(const tbuspp_endpoint_t *self);
tbuspp_id_t tbuspp_get_agent_id(const tbuspp_endpoint_t *self);
uint32_t tbuspp_get_agent_version(const tbuspp_endpoint_t *self);

/**
 connected 状态是 app 与 agent 之间命令通道正常，此时 app 不一定注册（tbuspp_connect）
 */
bool tbuspp_is_connected(const tbuspp_endpoint_t *self);

/**
 ready 状态是 app 与 agent 之间注册完成，并且命令通道正常，与 mq 的读写无关
 app 在 tbuspp_open() 成功后，获取到 mq, 后续即使命令管道临时断开，不影响队列读写
 */
bool tbuspp_is_ready(const tbuspp_endpoint_t *self);

/**
  获取本地命令通道 socket fd, app 可以将fd 注册到自身维护的 epoll中，由READ 事件驱动
  [tbuspp_update()](@ref tbuspp_update()) 调用。
  重要：将信令句柄注册到 epoll 后，业务需要处理 `TBUSPP_EVT_AGENT_BROKEN`,
  `TBUSPP_EVT_AGENT_RECOVERY` 事件，在信令连接断开或者重连后，重新获取句柄并注册epoll
*/
int tbuspp_endpoint_fd(const tbuspp_endpoint_t *self);

/**
 * @brief 重置上一次信令通道连接时间
 *
 * `tbuspp_update` 调用中，如果与当前agent
 * 连接断开（信令通道），则会尝试重连，为避免过于频繁的重连， Api
 * 固定最小间隔为1S，如果执行重置连接时间，则会在下一次 `tbuspp_update` 调用中立刻发起重连
 */
void tbuspp_reset_conn_time(tbuspp_endpoint_t *self);

/**@}*/

struct tbuspp_endpoint_info_t {
  tbuspp_id_t busid;
  uint32_t shard_id;
  int status;
  char alias[TBUSPP_ALIAS_MAX_SIZE];
  // 以下仅在 tbuspp_cache_get_endpoint_info 查询缓存时有效
  int load;
  utimestamp_t load_update_time;  // 为0时表明load不可用(该endpoint的load未设置或尚未缓存)
};
typedef struct tbuspp_endpoint_info_t tbuspp_endpoint_info_t;

struct tbuspp_shard_weight_t {
  uint32_t shard_id;
  uint32_t weight;
};
typedef struct tbuspp_shard_weight_t tbuspp_shard_weight_t;

struct tbuspp_group_info_t {
  tbuspp_id_t gid;
  uint32_t version;
  // 全量Ready成员计数，目前与member_count相同，后续支持超大规模组，可能只返回部分成员，
  // 则member_size>member_count
  int32_t member_size;
  int32_t route_type_num;  // 如果有路由策略，第一个为默认路由策略
  int32_t route_types[TBUSPP_ROUTE_TYPE_MAX_NUM];
  tbuspp_id_t master_busid;
  int32_t member_count;                // member count (ready members)
  tbuspp_endpoint_info_t *member_eps;  // ready members
  int32_t standby_count;               // count of standby_eps
  tbuspp_endpoint_info_t *standby_eps;
  char alias[TBUSPP_ALIAS_MAX_SIZE + 1];
  int32_t sws_count;
  // shard weight 只包含在线 ep 所属的shard的权重，如果shard weight未配置，默认100
  tbuspp_shard_weight_t *sws;
};

typedef struct tbuspp_group_info_t tbuspp_group_info_t;

struct tbuspp_group_conf_t {
  int32_t route_types_num;
  tbuspp_route_type_t route_types[TBUSPP_ROUTE_TYPE_MAX_NUM];
  uint32_t route_c_hash_replica;  // 参数范围[0-1000]，若为0，则取默认值 100
  tbuspp_shard_route_policy_t shard_route_policy;
  bool allow_msg_wrong_version;
};
typedef struct tbuspp_group_conf_t tbuspp_group_conf_t;

struct tbuspp_local_endpoints_info_t {
  int32_t count;
  const tbuspp_id_t *busids;
};

typedef struct tbuspp_local_endpoints_info_t tbuspp_local_endpoints_info_t;

struct tbuspp_group_fullset_info_t {
  uint32_t revision;
  int32_t count;
  const tbuspp_id_t *gids;
};

typedef struct tbuspp_group_fullset_info_t tbuspp_group_fullset_info_t;

struct tbuspp_route_param_t {
  tbuspp_route_type_t route_type;  // 请求的路由类型，不填则为默认路由类型
  uint64_t hash_key;
  uint32_t shard_id;
};

typedef struct tbuspp_route_param_t tbuspp_route_param_t;

// PY_CFFI_END

// First RouteType Item is Default RouteType
static inline int tbuspp_group_default_route_type(const tbuspp_group_info_t *g) {
  return (g != NULL && g->route_type_num > 0) ? g->route_types[0] : 0;
}

// PY_CFFI_START
/**
 * @brief: 枚举出本地已注册的 endpoints，信息存放在eps_info中。
 * @param eps_info 传出参数，
 * @param wait_ms   > 0: 最大等待时长 (milliseconds)
 *                  = 0: 则只发送请求，查询结果在tbuspp_update() 回调事件中给出
 *                  < 0: 永久等待，直到返回结果或者通讯失败
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0为成功；非0失败
 */
int tbuspp_get_local_endpoints(tbuspp_endpoint_t *self, tbuspp_local_endpoints_info_t *eps_info,
                               int wait_ms, const tbuspp_context_t *ctx);

/**
 * @brief: 查询Group信息的接口，Group信息存放在group_info中。
 * @param group_id 待查询的gid
 * @param group_info 传出参数，
 *    wait_ms !=
 * 0同步模式，传入*group_info的指针，获取其内容。不需要主动释放，由Api动态申请和释放空间。 wait_ms =
 * 0异步模式，传入为NULL即可。
 * @param flags
 *    NS模式：
 *        flags == 0：
 *          获取agent本地Group信息,如果agent本地存在并且未过期,则获取本地Group,否则向NS请求信息。适用于通用周期查询。
 *        flags & TBUSPP_QUERY_GROUP_FLAG_LOAD_FROM_NS：
 *          消息转发到ns进行查询group信息,多endpoint频繁调用会影响ns性能,适用于少量及时性要求高的查询
 *
 *    非NS模式：无效
 * @param wait_ms   > 0: 最大等待时长 (milliseconds)
 *                  = 0: 则只发送请求，查询结果在tbuspp_update() 回调事件中给出
 *                  < 0: 永久等待，直到返回结果或者通讯失败
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0为成功；非0失败
 */
int tbuspp_query_group_info(tbuspp_endpoint_t *self, tbuspp_id_t group_id,
                            const tbuspp_group_info_t **group_info, uint32_t flags, int wait_ms,
                            const tbuspp_context_t *ctx);

/**
 * @brief: 设置group配置,支持覆盖domain的通配group conf,domain配置中非通配group不允许设置
 *         当前只支持ns模式。 k8s
 * ns单例模式下，需将ns安装目录下var/api_groups.json共享出来持久化避免ns pod重启丢失配置
 * @param conf group配置
 * @param enable_overwrite 是否覆盖已经设置group配置
 * @param wait_ms   > 0: 最大等待时长 (milliseconds)
 *                  = 0: 则只发送请求，查询结果在tbuspp_update() 回调事件中给出
 *                  < 0: 永久等待，直到返回结果或者通讯失败
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0为成功；非0失败
 */
int tbuspp_set_group_conf(tbuspp_endpoint_t *self, tbuspp_id_t group_id,
                          const tbuspp_group_conf_t *conf, bool enable_overwrite, int wait_ms,
                          const tbuspp_context_t *ctx);
/**
 * @brief:
 * 订阅指定的Group，订阅后，目标组内Endpoint成员上下线事件会发送至到此Endpoint。
 * 当调用tbuspp_close后，所有订阅关系自动会自动取消。下次启动需要再次订阅。
 *
 * 重要：如果项目需要自定义路由，则可以调用 `tbuspp_cache_enable_group`,
 * `tbuspp_cache_enable_glob_group`
 * 等Cache相关Api，Tbuspp2将在Api内按需缓存并更新目标Group数据（在tbuspp_update()调用中更新）。
 *
 * Group订阅目前作用是使GameSvr获得 `TBUSPP_EVT_ENDPOINT_CHANGE_EVT`
 * 事件，若GameSvr处理EndpintChangeEvt的目的是
 * 维护目标组内Endppoint成员清单，则直接使用tbuspp_cache_* 相关Api更为简便，无需调用SubscribeGroup
 *
 * 在EndpointChangeEvt发送期间，若发生相关NameSvr实例，Agent，GameSvr的重启，则可能会丢失部分通知消息
 *
 * @param group_id gid
 * @param wait_ms   > 0: 最大等待时长 (milliseconds)
 *                  = 0: 则只发送请求，查询结果在tbuspp_update() 回调事件中给出
 *                  < 0: 永久等待，直到返回结果或者通讯失败
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0为成功；非0失败
 */
int tbuspp_subscribe_group(tbuspp_endpoint_t *self, tbuspp_id_t group_id, int wait_ms,
                           const tbuspp_context_t *ctx);

/**
 * @brief:
 * 取消订阅指定的Group，取消后，目标组内Endpoint成员上下线事件不会发送至该Endpoint。
 * 当调用tbuspp_close后，所有订阅关系自动会自动取消。
 *
 * @param group_id gid
 * @param wait_ms   > 0: 最大等待时长 (milliseconds)
 *                  = 0: 则只发送请求，查询结果在tbuspp_update() 回调事件中给出
 *                  < 0: 永久等待，直到返回结果或者通讯失败
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0为成功；非0失败
 */
int tbuspp_unsubscribe_group(tbuspp_endpoint_t *self, tbuspp_id_t group_id, int wait_ms,
                             const tbuspp_context_t *ctx);

/**
 * @brief:
 * 设置实例的 userdata 信息，设置成功后其他实例可查询获取，实例注销后自动清理
 * @param userdata 仅支持可见字符，最大长度参考 TBUSPP_ENDPOINT_UDATA_MAX_SIZE
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0: 执行成功，否则失败
 */
int tbuspp_set_endpoint_udata(tbuspp_endpoint_t *self, const char *userdata, size_t size,
                              int wait_ms, const tbuspp_context_t *ctx);

/**
 * @brief:
 * 查询自己或者其他实例的 userdata 信息
 * @param busid 要查询的实例，如果设零, 表示查询自己
 * @param buf 保存查询到的 userdata 信息，正常情况下 buf_size > val_size - 1, 否则丢弃剩余信息
 * @param val_size 出参，可传空指针，同步等待时保存实际 userdata 大小，异步等待时无意义
 * @param wait_ms > 0: 同步等待，业务需要提供有效的 buf 和 buf_size
 *                = 0: 异步等待，业务不需要提供有效的 buf 和 buf_size, 查询结果通过处理回调事件获取
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0: 执行成功，否则失败
 */
int tbuspp_get_endpoint_udata(tbuspp_endpoint_t *self, tbuspp_id_t busid, char *buf,
                              size_t buf_size, size_t *val_size, int wait_ms,
                              const tbuspp_context_t *ctx);

/**
 * @brief:
 * 查询alise对应的busid
 * @param alias 要查询的别名，单个别名最长TBUSPP_ALIAS_MAX_SIZE
 * @param busid 出参，别名解析后的busid，异步模式传NULL即可
 * @param wait_ms > 0: 同步等待
 *                = 0: 异步等待
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0: 执行成功，否则失败
 */
int tbuspp_query_alias_busid(tbuspp_endpoint_t *self, const char *alias, tbuspp_id_t *busid,
                             int wait_ms, const tbuspp_context_t *ctx);

/**
 * @brief:
 * 创建alise对应的busid，已存在busid则返回已存在的结果;
 * 不存在则创建，目前只支持GroupAlias的主动创建。
 * @param alias 要创建的别名，单个别名最长TBUSPP_ALIAS_MAX_SIZE
 * @param busid 出参，别名解析后的busid，异步模式传NULL即可
 * @param wait_ms > 0: 同步等待
 *                = 0: 异步等待
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0: 执行成功，否则失败
 */
int tbuspp_create_alias_busid(tbuspp_endpoint_t *self, const char *alias, tbuspp_id_t *busid,
                              int wait_ms, const tbuspp_context_t *ctx);

/**
 * @brief:
 * 设置ep的负载，设置完成后，会实时通知订阅当前Ep所属Group的订阅者。如果是广播，则全量通知。
 * @param load 负载值。
 * @param wait_ms > 0: 同步等待，最大等待时长 (milliseconds)
 *                = 0: 则只发送请求，设置结果在tbuspp_update() 回调事件中给出
 *                < 0: 永久等待，直到返回结果或者通讯失败
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0: 执行成功，否则失败
 */
int tbuspp_set_endpoint_load(tbuspp_endpoint_t *self, int32_t load, int wait_ms,
                             const tbuspp_context_t *ctx);

/**
 * @brief:
 * 获取ep的负载。
 * @param busid 要查询的ep busid，如果设零, 表示查询自己
 * @param load 保存查询到的 load 信息
 * @param load_update_time load的更新时间，为0表示不存在load，单位us
 * @param wait_ms > 0: 同步等待，最大等待时长 (milliseconds)
 *                = 0: 则只发送请求，查询结果在tbuspp_update() 回调事件中给出
 *                < 0: 永久等待，直到返回结果或者通讯失败
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0: 执行成功，否则失败
 */
int tbuspp_query_endpoint_load(tbuspp_endpoint_t *self, tbuspp_id_t busid, int32_t *load,
                               utimestamp_t *load_update_time, int wait_ms,
                               const tbuspp_context_t *ctx);

/**
 * @brief: 获取所有group id列表
 * @param fullset 传出参数，
 *    wait_ms != 0: 同步模式，传入*fullset的指针，获取其内容。不需要主动释放，
 *                  由Api动态申请和释放空间。
 *    wait_ms  = 0: 异步模式，传入为NULL即可。
 *
 * @param wait_ms > 0: 同步等待，最大等待时长 (milliseconds)
 *                = 0: 则只发送请求，查询结果在tbuspp_update() 回调事件中给出
 *                < 0: 永久等待，直到返回结果或者通讯失败
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0: 执行成功，否则失败
 */
int tbuspp_query_group_fullset(tbuspp_endpoint_t *self, const tbuspp_group_fullset_info_t **fullset,
                               int wait_ms, const tbuspp_context_t *ctx);

struct tbuspp_servant_param_t {
  tbuspp_id_t group_id;

  /**
   * shard_id = 0 表示在整个 Group 下分配路由
   * shard_id > 0 表示只在对应 ShardId 下分配路由
   */
  uint32_t shard_id;

  /**
   * 请求方会给目标节点增加的负载量
   */
  int32_t payload_value;

  /**
   * 会话信息标识，具体内容由业务自己定义，空值时无意义，非空值时，NS 会保存此 ID 对应的路由
   * 分配结果，以支持调用方重启后分配相同的目标节点，同个 ID 多次分配不会导致负载重复增加
   */
  char session_id[64];

  /**
   * 是否重新分配路由，忽略 NS 当前保存的 session_id 已分配的结果
   */
  bool is_new_session;
};

typedef struct tbuspp_servant_param_t tbuspp_servant_param_t;

// PY_CFFI_END

// 使用前请进行初始化操作，避免传入错误参数
static inline void tbuspp_init_servant_param(tbuspp_servant_param_t *param) {
  memset(param, 0, sizeof(tbuspp_servant_param_t));
}

// 兼容旧命名
#define tbuspp_cache_enable_cache_group tbuspp_cache_enable_group

// PY_CFFI_START

/**
 * @brief: 申请在目标 Group 下基于所有成员的负载信息精确分配路由
 * @param param 负载分配请求参数
 * @param dest 路由分配结果
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 * @return 0: 执行成功，否则失败
 */
int tbuspp_require_payload_servant(tbuspp_endpoint_t *self, const tbuspp_servant_param_t *param,
                                   tbuspp_id_t *dest, int wait_ms, const tbuspp_context_t *ctx);

/**
 * @brief: 释放在目标 Group 下使用 session_id 分配到的路由信息
 * @return 0: 执行成功，否则失败
 */
int tbuspp_release_payload_servant(tbuspp_endpoint_t *self, tbuspp_id_t group_id,
                                   const char *session_id, int wait_ms,
                                   const tbuspp_context_t *ctx);

/**
 * \defgroup cache
 * 路由缓存
 * @{
 */

/**
 * @brief 设置是否在 Api 内缓存所有 group 数据（包括其成员信息）；
 * 无需调用 `tbuspp_cache_enable_group` `tbuspp_cache_enable_glob_group` 开启特定组的缓存
 *
 * @note
 * 数据同步在 tbuspp_update 中进行；
 * 对于已订阅组，组版本变更时，TBUSPP_EVT_GROUP_CHANGE_EVT 事件将延后至缓存更新完成后才进行回调；
 * 对于未订阅组，缓存中组版本变更时，也将触发 TBUSPP_EVT_GROUP_CHANGE_EVT 事件回调；
 * 如需缓存所有 endpoint 的 load 信息，需要额外开启 namesvr 变更广播或订阅指定组；
 * enable=false 尚未实现；
 */
int tbuspp_cache_enable_all_groups(tbuspp_endpoint_t *self, bool enable);

/**
 * @brief 获取 Api 内缓存的 group 全量列表；
 * 需提前调用 `tbuspp_cache_enable_all_groups` 或 `tbuspp_cache_enable_glob_group`；
 * @return 非空时有效
 */
const tbuspp_group_fullset_info_t *tbuspp_cache_get_group_fullset(tbuspp_endpoint_t *self);

/**
 * @brief 设置是否在 Api 内缓存指定 group 数据（包括其成员信息）；数据同步在 tbuspp_update 中进行；
 * @param gid  目标group id
 * @return 0为成功；非0失败
 * @note see `tbuspp_cache_enable_all_groups` note
 */
int tbuspp_cache_enable_group(tbuspp_endpoint_t *self, tbuspp_id_t gid, bool enable);

/**
 * @brief 设置是否在 Api 内缓存匹配 glob_gid 的 group 数据（包括其成员信息）
 * @param glob_gid  通配gid
 * @return 0为成功；非0失败
 * @note see `tbuspp_cache_enable_all_groups` note
 */
int tbuspp_cache_enable_glob_group(tbuspp_endpoint_t *self, tbuspp_id_t glob_gid, bool enable);

/**
 * @brief 设置是否在 Api 内缓存指定 alias 的 group 数据（包括其成员信息）
 * @param alias 目标 group 的别名，例如 "a.b.*.*"
 * @return 0: 执行成功，否则失败
 * @note see `tbuspp_cache_enable_all_groups` note
 */
int tbuspp_cache_enable_group_by_alias(tbuspp_endpoint_t *self, const char *alias, bool enable);

/**
 * @brief 设置是否在 Api 内缓存匹配 glob_alias 的 group 数据（包括其成员信息）
 * @param glob_alias 通配别名，对于具体的 group alias 例如 "a.b.*.*", 支持以下通配方式：
 *                   "a.*.*.*"、"*.b.*.*", 注意不能省略分隔符 '.'
 * @return 0: 执行成功，否则失败
 * @note see `tbuspp_cache_enable_all_groups` note
 */
int tbuspp_cache_enable_glob_group_by_alias(tbuspp_endpoint_t *self, const char *glob_alias,
                                            bool enable);

/**
 * @brief 设置是否在 Api 内缓存 endpoint 的 userdata 信息
 * @return 0: 执行成功，否则失败
 * @note
 * see `tbuspp_cache_enable_all_groups` note
 * 对于已缓存的组，实例变更 userdata 时会触发 TBUSPP_EVT_ENDPOINT_UDATA_CHANGE_EVT
 */
int tbuspp_cache_enable_endpoint_udata(tbuspp_endpoint_t *self, bool enable);

/**
 * @brief 检查目标组的缓存是否可用, \sa tbuspp_cache_enable_group
 * @param gid: 目标组, 仅支持指定 gid
 * @return true: 缓存已加载；false：缓存尚未加载成功，或该组尚未创建
 */
bool tbuspp_cache_group_is_ready(tbuspp_endpoint_t *self, tbuspp_id_t gid);

/**
 * @brief 检查目标组的缓存是否可用, \sa tbuspp_cache_enable_glob_group
 * @param gid: 目标组, 仅支持glob gid
 * @return true: 缓存已加载；false：缓存尚未加载成功，或该组尚未创建
 *               由于Ns是动态创建group(endpoint上线时触发Ns创建所属group，创建后一直保持)，
 *               因此返回true时只保证当前已创建、所属于glob gid的group members ready
 */
bool tbuspp_cache_glob_group_is_ready(tbuspp_endpoint_t *self, tbuspp_id_t gid);

/**
 * @brief 检查别名对应的 group 的缓存是否可用, \sa tbuspp_cache_enable_group_by_alias
 * @param alias 目标别名
 * @return true: 缓存已加载，false: 缓存尚未加载成功，或该别名尚未创建对应的 group
 */
bool tbuspp_cache_group_is_ready_by_alias(tbuspp_endpoint_t *self, const char *alias);

/**
 * @brief 检查通配别名对应的 group 的缓存是否可用, \sa tbuspp_cache_enable_glob_group_by_alias
 * @param glob_alias 目标通配别名
 * @return true: 缓存已加载，false: 缓存尚未加载成功，或该通配别名尚未创建任何 group
 * @note see `tbuspp_cache_glob_group_is_ready` note
 */
bool tbuspp_cache_glob_group_is_ready_by_alias(tbuspp_endpoint_t *self, const char *glob_alias);

/**
 * @brief 检查所有groups是否可用, \sa tbuspp_cache_enable_all_groups
 * @return true: 缓存已加载；false：缓存尚未加载成功，或没有all cache没有enabled
 *               由于Ns是动态创建group(endpoint上线时触发Ns创建所属group，创建后一直保持)，
 *               因此返回true时只保证当前所有groups ready, 未创建group无法保证
 */
bool tbuspp_cache_all_groups_is_ready(tbuspp_endpoint_t *self);

/**
 * @brief 从缓存中查询指定 group 信息
 * @param gid  目标组
 * @return 非空时有效
 */
const tbuspp_group_info_t *tbuspp_cache_get_group_info(tbuspp_endpoint_t *self, tbuspp_id_t gid);

/**
 * @brief 从缓存中查询指定 endpoint 信息
 * @param busid  目标 endpoint 的 busid，为0时查询自身
 * @return 非空时有效；
 *         为空时可能为 endpoint 不在线或缓存未加载，结合 tbuspp_cache_group_is_ready 检查；
 *         负载信息仅在变更时或主动查询后进行缓存，请检查 load_update_time
 */
const tbuspp_endpoint_info_t *tbuspp_cache_get_endpoint_info(tbuspp_endpoint_t *self,
                                                             tbuspp_id_t busid);

/**
 * @brief 从缓存中查询指定 endpoint 的 userdata 信息
 * @param busid 目标实例，零值表示查询自身，所属组必须是已缓存的
 * @param buf 出参，保存查询到的 userdata 信息
 * @param size 出参，保存查询到的 userdata 大小
 * @return
 * 0 表示执行成功，否则失败，失败可能是以下场景导致:
 * 缓存未就绪 | endpoint 不在线 | endpoint 没有设置 userdata
 * @note 使用该功能需要先开启 tbuspp_cache_enable_endpoint_udata
 */
int tbuspp_cache_get_endpoint_udata(tbuspp_endpoint_t *self, tbuspp_id_t busid, const char **buf,
                                    size_t *size);

/**
 * @brief 从缓存中查询别名对应的 busid
 * @param alias 要查询的别名，单个别名最长TBUSPP_ALIAS_MAX_SIZE
 * @return 非0时有效
 */
tbuspp_id_t tbuspp_cache_get_alias_busid(tbuspp_endpoint_t *self, const char *alias);

/**
 * @brief 从缓存中查询group revision信息
 *        使用api时需要开启缓存功能(tbuspp_cache_enable_all_groups/tbuspp_cache_enable_group)
 * @return group_revision, 非0:有效, 0:无效
 */
uint32_t tbuspp_cache_get_group_revision(tbuspp_endpoint_t *self);

/**
 * @brief 从缓存中查询当前group的选路结果，只支持有状态路由：HASH,CHASH，Master
 *        使用api时需要开启缓存功能(tbuspp_cache_enable_all_groups/tbuspp_cache_enable_group)
 * @param gid  目标组
 * @param param 路由参数
 * @param dest 查询成功时，返回选路目标busid
 * @return 0为成功；非0失败
 */
int32_t tbuspp_cache_get_route_dest(tbuspp_endpoint_t *self, tbuspp_id_t gid,
                                    const tbuspp_route_param_t *param, tbuspp_id_t *dest);

/**
 * @brief 直接使用别名发送消息，由Agent进行别名解析和路由
 * @param alias 消息目标的别名
 * @return 如果所传入的param版本不符合要求或未初始化，会返回TBUSPP_ERR_OP_DENIED，其他情况参考 tbuspp_queue_write
 */
int tbuspp_queue_write_by_alias(tbuspp_queue_t *q, const char *alias, const void *msg_data,
                                uint32_t msg_size, const tbuspp_msg_param_t *param);

// end cache
/**@}*/

/**
 * @brief 获取自身所属的有状态组的最新版本，仅在自身READY时有效
 * @return 返回0表示 无状态组 || 自身为非READY状态
 */
uint32_t tbuspp_get_self_group_version(tbuspp_endpoint_t *self);

/**
 * @brief 获取所属组的master；仅在自身READY时有效
 * @return 返回0表示 无主 || 自身为非READY状态 || 无法确认当前状态
 */
tbuspp_id_t tbuspp_get_group_master(tbuspp_endpoint_t *self);

/**
 * @brief 获取所属组所属shard的master；仅在自身READY时有效
 * @return 返回0表示 无主 || 自身为非READY状态 || 无法确认当前状态
 */
tbuspp_id_t tbuspp_get_shard_master(tbuspp_endpoint_t *self);

/**
 * @brief
 * 初始化一个 span
 */
void tbuspp_trace_init_span(tbuspp_trace_span_info_t *span);

/**
 * @brief 开始一个span
 * @param parent_span_ctx 父span的 span_ctx，如果是 start root span，此参数置为null即可。
 * @param span  新span，一部分成员由接口自动计算，一部分成员由业务指定，具体如下：
 * span_id/start_time_us以及root_span的trace_id由接口自动计算。
 * 如果业务要修改start_time_us，请start_span之后，end_span之前进行修改。从而支持跨机通信下，时间不一致的场景。
 * 要保证修改后，end_span计算UTC的us大于start_time_us
 * span_name由业务在指定，为空报错；attr/kind由业务按需指定，标识Span的属性，携带上报。
 * start_span后，start_time_us赋值为当前UTC的us。
 * @return 0表示成功，非0表示失败
 */
void tbuspp_trace_start_span(tbuspp_endpoint_t *self,
                             const tbuspp_trace_span_ctx_t *parent_span_ctx,
                             tbuspp_trace_span_info_t *span);

/**
 * @brief 结束一个span
 * 表示当前span对应的调用完成，自动计算end_time_ns（如果不为0，则使用业务主动设置的），并将span_ctx写入当前endpoint的OutQ，Agent过滤后上报。
 * 不支持多线程调用:
 * tbuspp_trace_end_span内部会调用tbuspp_queue_write，因此需要业务保证tbuspp_trace_end_span的多线程调用加锁或者单线程调用
 * 是否上报到服务中心：由 span->span_ctx.flags 决定，具体参考 span->span_ctx.flags 的注释
 * @param span 由对应的tbuspp_trace_start_span初始化后的span.
 * @return 0表示成功，非0表示失败
 */
int tbuspp_trace_end_span(tbuspp_endpoint_t *self, tbuspp_trace_span_info_t *span);

/**
 * \defgroup endpoint act stateful group change transaction
 * 业务层参与有状态组变更事务：需在 GroupConf 中设置 group_trans_level = 3
 *
 * 详细方案参考 https://iwiki.woa.com/pages/viewpage.action?pageId=2030335719
 *
 * @{
 */

enum tbuspp_group_trans_action_t {
  TBUSPP_GROUP_TRANS_ACTION_NULL = 0,
  TBUSPP_GROUP_TRANS_ACTION_PREPARE,
  TBUSPP_GROUP_TRANS_ACTION_COMMIT,
  TBUSPP_GROUP_TRANS_ACTION_ABORT,
  TBUSPP_GROUP_TRANS_ACTION_BLOCK
};

typedef enum tbuspp_group_trans_action_t tbuspp_group_trans_action_t;

enum tbuspp_group_trans_state_t {
  // 0, 1 internal state
  TBUSPP_GROUP_TRANS_PREPARE_DOING = 2,
  TBUSPP_GROUP_TRANS_REQUIRE_BLOCK = 3,
  TBUSPP_GROUP_TRANS_PREPARE_BLOCKING = 4,
  TBUSPP_GROUP_TRANS_REQUIRE_COMMIT = 5,
  TBUSPP_GROUP_TRANS_REQUIRE_ABORT = 6,
  TBUSPP_GROUP_TRANS_END = 7,
};

typedef enum tbuspp_group_trans_state_t tbuspp_group_trans_state_t;

struct tbuspp_group_trans_diff_info_t {
  int32_t joining_count;
  tbuspp_endpoint_info_t *joining_eps;  // 以ready状态注册的成员、join_group的成员
  int32_t exiting_count;
  tbuspp_endpoint_info_t *exiting_eps;  // 以standby状态注册的成员、exit_group的成员、注销的成员
};

typedef struct tbuspp_group_trans_diff_info_t tbuspp_group_trans_diff_info_t;

/**
 * @brief:
 * 报告 endpoint 处理组变更的状态，应周期性调用，周期建议值为 namesvr 有状态单轮通知超时时间的一半
 * @return 0: 成功，<0: 失败
 */
int tbuspp_report_trans_state(tbuspp_endpoint_t *self, tbuspp_group_trans_state_t state);

/**
 * @brief: 获取有状态组两阶段变更时的当前组状态
 * tbuspp_group_info_t    仅 route_types, master, ready_member 相关字段可用
 * tbuspp_endpoint_info_t 仅 busid, shard_id, alias 字段可用
 */
const tbuspp_group_info_t *tbuspp_trans_curr_group_info(tbuspp_endpoint_t *self);

/**
 * @brief: 获取有状态组两阶段变更的最终组状态
 * tbuspp_group_info_t    仅 route_types, master, ready_member 相关字段可用
 * tbuspp_endpoint_info_t 仅 busid, shard_id, alias 字段可用
 */
const tbuspp_group_info_t *tbuspp_trans_next_group_info(tbuspp_endpoint_t *self);

/**
 * @brief: 获取有状态组两阶段变更的变动成员列表
 * tbuspp_endpoint_info_t 仅 busid, shard_id, alias 字段可用
 */
const tbuspp_group_trans_diff_info_t *tbuspp_trans_diff_info(tbuspp_endpoint_t *self);
/**
 * @brief: 获取有状态组两阶段变更时当前组状态下的消息目标
 */
int tbuspp_trans_curr_dest(tbuspp_endpoint_t *self, const tbuspp_route_param_t *param,
                           tbuspp_id_t *dest);

/**
 * @brief: 获取有状态组两阶段变更的最终组状态下的消息目标
 */
int tbuspp_trans_next_dest(tbuspp_endpoint_t *self, const tbuspp_route_param_t *param,
                           tbuspp_id_t *dest);

/**
 * @brief: 获取所属组是否处于变更中
 * @note
 * - 即使当前变更不需要本成员参与协商，也将返回true
 * - 通常用于收到TBUSPP_GROUP_TRANS_ACTION_PREPARE，但一直未收到
 *   TBUSPP_GROUP_TRANS_REQUIRE_COMMIT/ABORT 时，判断所属组是否已结束变更
 * - 如果是在参与变更协商时crash，重启tbuspp_open成功后，可通过此接口判断是否仍处于变更中；
 *   处于变更中时，会重新收到TBUSPP_GROUP_TRANS_REQUIRE_PREPARE/BLOCK通知
 */
bool tbuspp_is_in_group_trans(tbuspp_endpoint_t *self);

/**
 * @brief 设置 OTLP 的 Resource 属性，Agent 通过 Busid --> Resource 保存在 Exporter 线程
          当有 Trace/Metrics 数据需要上报时，通过上报的 Busid 进行查找对应的 Resource 进行聚合上报。
 * @param id        资源ID，范围 [1, 16]，标识当前的 Resource 对应的 id，从而支持多分组 Resource
 * @param attrs     资源属性，参考：https://opentelemetry.io/docs/specs/semconv/resource/
                    必填字段：service.name
                    无需填入字段：telemetry.sdk.* agent_id
 * @param attrs_size 属性个数
 * @param wait_ms   > 0: 最大等待时长 (milliseconds)
 *                  = 0: 则只发送请求，查询结果在tbuspp_update() 回调事件中给出
 *                  < 0: 永久等待，直到返回结果或者通讯失败
 * @param ctx 发送控制参数, NULL 无特殊控制，若非空，则首先调用 `tbuspp_init_context`
 */
int tbuspp_otlp_set_resource(tbuspp_endpoint_t *self, uint32_t id, const tbuspp_otlp_attr_t *attrs,
                             uint16_t attrs_size, int wait_ms, const tbuspp_context_t *ctx);

/**
 * @brief
 * 上报指标数据，建议定期调用以及时更新数据，该接口支持两种使用方式：
 * - 当 `data` 为空时，自动收集并上报基于 comlib/stats 相关接口生成的本地指标数据，Agent
 *   会聚合处理并通过多种可选方式最终上报到业务指定的监控平台
 * - 当 `data` 不为空时，可充当 OTLP exporter 功能，接收业务生成的原始指标数据，自动关联 Resource
 *   属性后，构建完整的 OTLP 请求并上报到 Agent `otlp_server_url` 参数指定的 Collector 地址
 * @param data 包含序列化的指标数据以及要关联的 Resource ID
 * @return 0: 执行成功，否则失败
 * @note 内部会调用 tbuspp_queue_write 接口，非线程安全
 */
int tbuspp_metrics_report_data(tbuspp_endpoint_t *self, const tbuspp_metrics_data_t *data);

// end endpoint act stateful group change transaction
/**@}*/

// for winsock init
int SocketInit(int major_version, int minor_version);
void SocketCleanup();

struct tbuspp_update_cost_warn_limit_t {
  int callback_limit_ms;  // 回调处理时间限制,uint ms
  int internal_limit_ms;  // update内部逻辑时间限制,uint ms
  int warn_interval_sec;  // 告警周期,uint second
};

typedef struct tbuspp_update_cost_warn_limit_t tbuspp_update_cost_warn_limit_t;

/**
 * @brief 设置tbuspp_update运行耗时超时告警,当执行时间超过设置时间时,log输出告警。
 * @param warn_limit 设置超时时间和告警周期, 设置limit值 <= 0时, 关闭告警
 *       参数默认 callback: 5ms, internal: 5ms, warn_interval: 5s
 */
void tbuspp_set_update_cost_warn_limit(const tbuspp_update_cost_warn_limit_t *warn_limit);

// events
enum tbuspp_event_id_t {
  TBUSPP_EVT_FINISH_OPEN = 1,
  TBUSPP_EVT_AGENT_BROKEN,
  TBUSPP_EVT_AGENT_RECOVERY,
  TBUSPP_EVT_SET_STATUS_RES,
  TBUSPP_EVT_QUERY_STATUS_RES,
  TBUSPP_EVT_CMD_TIMEOUT_RES,
  TBUSPP_EVT_QUERY_GROUP_RES,
  TBUSPP_EVT_SUBSCRIBE_GROUP_RES,
  TBUSPP_EVT_UNSUBSCRIBE_GROUP_RES,
  TBUSPP_EVT_GROUP_CHANGE_EVT,  // 10
  TBUSPP_EVT_SET_RESTART_RESERVE_TIME_RES,
  TBUSPP_EVT_SET_UDATA_RES,
  TBUSPP_EVT_GET_UDATA_RES,
  TBUSPP_EVT_GET_LOCAL_ENDPOINTS_RES,
  TBUSPP_EVT_QUERY_ALIAS_BUSID_RES,
  TBUSPP_EVT_ENDPOINT_CHANGE_EVT,
  TBUSPP_EVT_SET_LOAD_RES,  // 17
  TBUSPP_EVT_QUERY_LOAD_RES,
  TBUSPP_EVT_ENDPOINT_LOAD_CHANGE,
  TBUSPP_EVT_GROUP_SET_CONF_RES,
  TBUSPP_EVT_QUERY_GROUP_FULLSET_RES,
  TBUSPP_EVT_FINISH_CONNECT,
  TBUSPP_EVT_SET_KEEPALIVE_WITH_PING_RES,
  TBUSPP_EVT_REQUIRE_PAYLOAD_SERVANT_RES,
  TBUSPP_EVT_RELEASE_PAYLOAD_SERVANT_RES,
  TBUSPP_EVT_GROUP_TRANS_EVT,
  TBUSPP_EVT_SET_CLOSE_BY_UNEXPECT_EXIT_RES,
  TBUSPP_EVT_ENDPOINT_UDATA_CHANGE_EVT,
  TBUSPP_EVT_SET_OTLP_RESOURCE_RES,
  TBUSPP_EVT_LAST
};

typedef enum tbuspp_event_id_t tbuspp_event_id_t;

#define TBUSPP_COMMON_RESULT \
  int result;                \
  char err_info[TBUSPP_ERR_INFO_SIZE];

struct tbuspp_common_result_t {
  TBUSPP_COMMON_RESULT
};

struct tbuspp_event_finish_conn_t {
  TBUSPP_COMMON_RESULT
};

struct tbuspp_event_finish_open_t {
  TBUSPP_COMMON_RESULT
};

struct tbuspp_event_set_status_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t busid;
  int status;
};

struct tbuspp_event_query_status_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t busid;
  int status;
};

struct tbuspp_event_subscribe_group_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t gid;
};

struct tbuspp_event_query_group_res_t {
  TBUSPP_COMMON_RESULT
  const tbuspp_group_info_t *group_info;
};

struct tbuspp_event_get_local_endpoints_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_local_endpoints_info_t eps_info;
};

struct tbuspp_event_set_reserve_time_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t busid;
};

struct tbuspp_event_set_keepalive_with_ping_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t busid;
};

struct tbuspp_event_set_close_by_unexpect_exit_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t busid;
};

struct tbuspp_event_group_change_evt_t {
  tbuspp_id_t gid;
  // 如果version=0, 则表示对应组已删除
  uint32_t version;
};

struct tbuspp_event_endpoint_change_evt_t {
  tbuspp_id_t busid;
  int status;
  int reason;
  int old_status;
};

struct tbuspp_event_set_udata_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t busid;
};

struct tbuspp_event_get_udata_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t busid;
  char userdata[TBUSPP_ENDPOINT_UDATA_MAX_SIZE];
  size_t size;
};

struct tbuspp_event_endpoint_udata_change_evt_t {
  tbuspp_id_t busid;
  char userdata[TBUSPP_ENDPOINT_UDATA_MAX_SIZE];
  size_t size;
};

struct tbuspp_query_alias_busid_res_t {
  TBUSPP_COMMON_RESULT
  char alias[TBUSPP_ALIAS_MAX_SIZE + 1];
  tbuspp_id_t busid;
};

struct tbuspp_event_set_load_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t busid;
};

struct tbuspp_event_query_load_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t busid;
  int load;
  utimestamp_t load_update_time;
};

struct tbuspp_endpoint_load_change_evt_t {
  tbuspp_id_t busid;
  int load;
  utimestamp_t load_update_time;
};

struct tbuspp_event_group_set_conf_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t group_id;
};

struct tbuspp_event_query_group_fullset_res_t {
  TBUSPP_COMMON_RESULT
  const tbuspp_group_fullset_info_t *group_fullset;
};

struct tbuspp_event_require_payload_servant_res_t {
  TBUSPP_COMMON_RESULT
  tbuspp_id_t dest_busid;
};

struct tbuspp_event_release_payload_servant_res_t {
  TBUSPP_COMMON_RESULT
};

struct tbuspp_event_group_trans_evt_t {
  tbuspp_group_trans_action_t action;
  uint32_t curr_version;
  uint32_t next_version;
};

struct tbuspp_event_set_otlp_resource_res_t {
  TBUSPP_COMMON_RESULT
  uint32_t id;
};

typedef struct tbuspp_common_result_t tbuspp_common_result_t;
typedef struct tbuspp_event_finish_conn_t tbuspp_event_finish_conn_t;
typedef struct tbuspp_event_finish_open_t tbuspp_event_finish_open_t;
typedef struct tbuspp_event_set_status_res_t tbuspp_event_set_status_res_t;
typedef struct tbuspp_event_query_status_res_t tbuspp_event_query_status_res_t;
typedef struct tbuspp_event_subscribe_group_res_t tbuspp_event_subscribe_group_res_t;
typedef struct tbuspp_event_subscribe_group_res_t tbuspp_event_unsubscribe_group_res_t;
typedef struct tbuspp_event_query_group_res_t tbuspp_event_query_group_res_t;
typedef struct tbuspp_event_set_reserve_time_res_t tbuspp_event_set_reserve_time_res_t;
typedef struct tbuspp_event_set_keepalive_with_ping_res_t
    tbuspp_event_set_keepalive_with_ping_res_t;
typedef struct tbuspp_event_set_close_by_unexpect_exit_res_t
    tbuspp_event_set_close_by_unexpect_exit_res_t;
typedef struct tbuspp_event_group_change_evt_t tbuspp_event_group_change_evt_t;
typedef struct tbuspp_event_endpoint_change_evt_t tbuspp_event_endpoint_change_evt_t;
typedef struct tbuspp_event_endpoint_udata_change_evt_t tbuspp_event_endpoint_udata_change_evt_t;
typedef struct tbuspp_event_set_udata_res_t tbuspp_event_set_udata_res_t;
typedef struct tbuspp_event_get_udata_res_t tbuspp_event_get_udata_res_t;
typedef struct tbuspp_event_get_local_endpoints_res_t tbuspp_event_get_local_endpoints_res_t;
typedef struct tbuspp_query_alias_busid_res_t tbuspp_query_alias_busid_res_t;
typedef struct tbuspp_event_set_load_res_t tbuspp_event_set_load_res_t;
typedef struct tbuspp_event_query_load_res_t tbuspp_event_query_load_res_t;
typedef struct tbuspp_endpoint_load_change_evt_t tbuspp_endpoint_load_change_evt_t;
typedef struct tbuspp_event_group_set_conf_res_t tbuspp_event_group_set_conf_res_t;
typedef struct tbuspp_event_query_group_fullset_res_t tbuspp_event_query_group_fullset_res_t;
typedef struct tbuspp_event_require_payload_servant_res_t
    tbuspp_event_require_payload_servant_res_t;
typedef struct tbuspp_event_release_payload_servant_res_t
    tbuspp_event_release_payload_servant_res_t;
typedef struct tbuspp_event_group_trans_evt_t tbuspp_event_group_trans_evt_t;
typedef struct tbuspp_event_set_otlp_resource_res_t tbuspp_event_set_otlp_resource_res_t;

struct tbuspp_event_t {
  union {
    struct {
      tbuspp_event_id_t event_id;
      tbuspp_context_t ctx;
    };
    char dummy1[512];
  };  // 512 byte

  union {
    tbuspp_common_result_t common_res;
    tbuspp_event_finish_conn_t finish_connet;
    tbuspp_event_finish_open_t finish_open;
    tbuspp_event_set_status_res_t set_status_res;
    tbuspp_event_query_status_res_t query_status_res;
    tbuspp_event_subscribe_group_res_t subscribe_group_res;
    tbuspp_event_unsubscribe_group_res_t unsubscribe_group_res;
    tbuspp_event_query_group_res_t query_group_res;
    tbuspp_event_group_change_evt_t group_change_evt;
    tbuspp_event_endpoint_change_evt_t endpoint_change_evt;
    tbuspp_event_endpoint_udata_change_evt_t endpoint_udata_change_evt;
    tbuspp_event_set_reserve_time_res_t set_reserve_time_res;
    tbuspp_event_set_keepalive_with_ping_res_t set_keepalive_with_ping_res;
    tbuspp_event_set_close_by_unexpect_exit_res_t set_close_by_unexpect_exit_res;
    tbuspp_event_set_udata_res_t set_udata_res;
    tbuspp_event_get_udata_res_t get_udata_res;
    tbuspp_event_get_local_endpoints_res_t get_local_endpoints_res;
    tbuspp_query_alias_busid_res_t query_alias_busid_res;
    tbuspp_event_set_load_res_t set_load_res;
    tbuspp_event_query_load_res_t query_load_res;
    tbuspp_endpoint_load_change_evt_t endpoint_load_change;
    tbuspp_event_group_set_conf_res_t group_set_conf_res;
    tbuspp_event_query_group_fullset_res_t query_group_fullset_res;
    tbuspp_event_require_payload_servant_res_t require_payload_servant_res;
    tbuspp_event_release_payload_servant_res_t release_payload_servant_res;
    tbuspp_event_group_trans_evt_t group_trans_evt;
    tbuspp_event_set_otlp_resource_res_t set_otlp_resource_res;
    char dummy2[256];
  };
  // total 2k
};
#pragma pack(pop)

// PY_CFFI_END

#ifdef __cplusplus
}
#endif

////////////////////////////////////////////////////////////////////////////////

#endif  // TBUSPP2_INC_TBUSPP2_H_
