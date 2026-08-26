/// Copyright (c) Tencent
// Author: bondshi
// Create: 2021-06-03

#ifndef TBUSPP2_INC_TBUSPP2_DEFS_H_
#define TBUSPP2_INC_TBUSPP2_DEFS_H_

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "comlib/defs/comdefs.h"

////////////////////////////////////////////////////////////////////////////////
// definitions at agent & api

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define TBUSPP_MAGIC_NUM 0x54425332  // TBS2
#else
#define TBUSPP_MAGIC_NUM 0x32534254  // TBS2
#endif

// PY_CFFI_START

typedef uint64_t tbuspp_id_t;   // network byte order, busid saved in lower 32 bits
typedef uint64_t utimestamp_t;  // unit: us
typedef uint64_t uticktime_t;   // unit: us

enum { TBUSPP_LL_DEBUG = 0, TBUSPP_LL_INFO, TBUSPP_LL_WARN, TBUSPP_LL_ERROR, TBUSPP_LL_FATAL };

// local error codes
// local error < 0
// remote error > 0

// queue error
#define TBUSPP_ERR_OK 0
#define TBUSPP_ERR_GENERIC -1
#define TBUSPP_ERR_OP_DENIED -2
#define TBUSPP_ERR_QUEUE_EMPTY -3
#define TBUSPP_ERR_QUEUE_BUSY -4

#define TBUSPP_ERR_LESS_MSG_BUF -5
#define TBUSPP_ERR_UNEXPECTED -6
#define TBUSPP_ERR_CMD_CHANNEL_BROKEN -9
#define TBUSPP_ERR_WRONG_ARG -10
#define TBUSPP_ERR_NOT_FOUND -11
#define TBUSPP_ERR_TIMEOUT -13
#define TBUSPP_ERR_WRONG_AGENT_VERSION -14
#define TBUSPP_ERR_WRONG_GIDMASK -15
#define TBUSPP_ERR_SHM_FAILED -16
#define TBUSPP_ERR_BUSID_MISMATCH -17
#define TBUSPP_ERR_NOT_IMPL -18
#define TBUSPP_ERR_WRONG_BUSID -19
#define TBUSPP_ERR_SSL_INIT_FAIL -20

#define TBUSPP_ERR_ROUTE_FAIL -1200          // p2g选路失败(1.GROUP_SHARDING_STRICT配置下，shard 没有成员 2.灰度规则选路失败)
#define TBUSPP_ERR_ROUTE_NOT_READY -1201     // P2G:成员不存在, P2P:目标不存在
#define TBUSPP_ERR_NET_MESH_NOT_READY -1202  // 目标网络异常导致消息无法发送
#define TBUSPP_ERR_WRONG_ROUTE_TYPE -1203    // P2G消息指定路由类型(route_type)未设置
#define TBUSPP_ERR_MSG_EXPIRE -1204          // p2g消息在选路阶段处理超时, 需要agent开启-msg_expire_secs

#define TBUSPP_MQ_IN 0
#define TBUSPP_MQ_OUT 1
#define TBUSPP_BASIC_MQ_NUM 2
#define TBUSPP_MAX_EXTEND_MQ_NUM 50

#define TBUSPP_MQ_ATTR_OUT 0x01

#define TBUSPP_DOMAIN_LOCAL 0

/*
 * 预定义msg_type值, < TBUSPP_MSG_TYPE_CUSTOM_MIN为tbus2内部保留，
 * 业务定义时要求 >= TBUSPP_MSG_TYPE_CUSTOM_MIN, 最大取值0xFFFF
 * TBUSPP_MSG_TYPE_NORMAL为业务未设置msg_type消息, TBUSPP_MSG_TYPE_TCONND为Tconnd模块消息
 * 其他定义类型消息为tbus2内部特性使用, 业务侧不会收到
 * \sa tbuspp_msg_param_t->msg_type
 */
#define TBUSPP_MSG_TYPE_NORMAL 0
#define TBUSPP_MSG_TYPE_ROUTER 1
#define TBUSPP_MSG_TYPE_SPAN 2
#define TBUSPP_MSG_TYPE_METRICS 3
#define TBUSPP_MSG_TYPE_TCONND 6
#define TBUSPP_MSG_TYPE_RPCREQ 7
#define TBUSPP_MSG_TYPE_RPCRSP 8

#define TBUSPP_MSG_TYPE_CUSTOM_MIN 10000

/**
 * \defgroup message transmit flags
 * @{
 * 消息发送控制标志 (16 bits)
 */

#define TBUSPP_MSG_FLAG_BROADCAST_READY \
  0x1  //! 全组广播，针对 ready 节点(already joined into group)
#define TBUSPP_MSG_FLAG_BROADCAST_STANDBY 0x2  //! 全组广播，针对 standy 节点
#define TBUSPP_MSG_FLAG_BROADCAST_GROUP 0x3    //! 全组广播，针对 ready&standy 全部节点
// (TBUSPP_MSG_FLAG_BROADCAST_READY | TBUSPP_MSG_FLAG_BROADCAST_STANDBY) (pycffi do not
//! support expression)

// msg dest 是 glob_gid(gid 通配掩码), if (dest & gid == dest), then sendto(gid)
// 与其它标志搭配使用
//  GLOB_GID | BROADCAST: 多组广播
//  GLOB_GID: 多组通配
#define TBUSPP_MSG_FLAG_GLOB_GID 0x4
#define TBUSPP_MSG_FLAG_MOCK 0x8  //! msg dest伪造, agent收到消息将源消息返回给发送方
#define TBUSPP_MSG_FLAG_LOCAL_FIRST 0x10  //! 随机路由策略下采用本地优先
#define TBUSPP_MSG_FLAG_FORCE_LOCAL 0x20  //! 随机路由策略下强制本地策略,路由失败则丢弃消息
#define TBUSPP_MSG_FLAG_NOT_BROADCAST_SELF 0x40  //! 广播目的忽略自身,在多域广播中使用时用于排除本域
#define TBUSPP_MSG_FLAG_ENABLE_BACKUP_P2G \
  0X80  //! p2p消息下支持备份选路，如果dest ep不存在 or not ready，在dest ep的group下选路

// end message transmit flags
/**@}*/

// 链路状态
#define TBUSPP_LINK_STATUS_OK 1     // 链路正常
#define TBUSPP_LINK_STATUS_WRONG 2  // 链路断开

/**
 * @brief endpoint 注册状态
 *
 * ( init ) ->  ( ready )  -> ( stop )
 *    |           ^ |       |
 *    |           | V       |
 *    +--   -> ( standby ) -+
 *
 * join group: ready
 * exit group: block
 *
 */
#define TBUSPP_ENDPOINT_STATUS_UNK 0      // unknown
#define TBUSPP_ENDPOINT_STATUS_READY 1    // endpoint ready
#define TBUSPP_ENDPOINT_STATUS_STANDBY 2  // endpoint standby
#define TBUSPP_ENDPOINT_STATUS_STOP 3     // endpoint stopped

// misc common consts
#define TBUSPP_ENDPOINT_UDATA_STR_MAX_SIZE 128
#define TBUSPP_ROUTE_TYPE_MAX_NUM 8

// endpoint userdata size
#define TBUSPP_ENDPOINT_UDATA_MAX_SIZE 512

// max agent url size
#define TBUSPP_AGENT_URL_MAX_SIZE 64

// span id size
#define TBUSPP_SPAN_ID_SIZE 8
// trace id size
#define TBUSPP_TRACE_ID_SIZE 16

// span status code
#define TBUSPP_SPAN_STATUS_CODE_UNSET 0
#define TBUSPP_SPAN_STATUS_CODE_OK 1
#define TBUSPP_SPAN_STATUS_CODE_ERROR 2

// span status message
#define TBUSPP_SPAN_STATUS_ERR_SIZE 63
#define TBUSPP_SPAN_FLAGS_SAMPLED_MASK 0x00000001
#define TBUSPP_SPAN_FLAGS_HAS_IS_REMOTE_MASK 0x00000100
#define TBUSPP_SPAN_FLAGS_IS_REMOTE_MASK 0x00000200

// otlp attr
#define TBUSPP_OTLP_ATTR_MAX_NUM 50
#define TBUSPP_OTLP_ATTR_KEY_VALUE_MAX_SIZE 255

// trace udata
#define TBUSPP_SPAN_UDATA_MAX_SIZE 127

#define TBUSPP_MAX_OTLP_RESOURCE_ID 16

// query group flags
#define TBUSPP_QUERY_GROUP_FLAG_LOAD_FROM_NS 0x01

#define TBUSPP_MAX_EXTRA_DESTS 200

#define TBUSPP_CONTEXT_MAX_UDATA_SIZE 256

// 消息数据在 SHM Queue 中存储格式定义

#define TBUSPP_MAX_MSG_META_SIZE 4096

// err info size
#define TBUSPP_ERR_INFO_SIZE 64

enum { TBUSPP_MSG_META_TYPE_NONE = 0, TBUSPP_MSG_META_TYPE_INQ, TBUSPP_MSG_META_TYPE_OUTQ };
enum tbuspp_route_param_type_t {
  TBUSPP_ROUTE_PARAM_NONE = 0,
  TBUSPP_ROUTE_PARAM_HASH,
  TBUSPP_ROUTE_PARAM_ON_VERSION,
  TBUSPP_ROUTE_PARAM_MAX = TBUSPP_ROUTE_PARAM_ON_VERSION
};

typedef enum tbuspp_route_param_type_t tbuspp_route_param_type_t;

// route type
enum tbuspp_route_type_t {
  TBUSPP_ROUTE_TYPE_NONE = 0,
  TBUSPP_ROUTE_TYPE_RANDOM = 1,
  TBUSPP_ROUTE_TYPE_M_HASH = 3,
  TBUSPP_ROUTE_TYPE_C_HASH = 4,
  TBUSPP_ROUTE_TYPE_MASTER = 5
};

typedef enum tbuspp_route_type_t tbuspp_route_type_t;

/*
  agent处理消息选择丢弃时，支持将消息通过inq写回给发送者，
  业务发送时，在tbuspp_msg_param_t->sendback_level 中设置
  业务peek/read消息时，可以通过 tbuspp_msg_desc_ex_t->sendback_level 确认是否为打回消息
*/
enum tbuspp_msg_sendback_level_t {
  TBUSPP_MSG_SENDBACK_LEVEL_NONE =
      0,  //! default, agent处理失败直接丢弃消息时，不打回, peek/read时，为业务非打回消息
  TBUSPP_MSG_SENDBACK_LEVEL_1 = 1,  //! 消息被打回时，返回简要消息：仅tbuspp_msg_desc_ex_t信息有效
                                    //! msg_size = 0
  TBUSPP_MSG_SENDBACK_LEVEL_2 = 2,  //! 消息被打回时，返回原始业务消息
};

// \sa TBUSPP_MSG_SENDBACK_LEVEL_XXX
enum tbuspp_sendback_reason_t {
  TBUSPP_SENDBACK_REASON_NULL = 0,
  // P2G:成员不存在, P2P:目标不存在
  TBUSPP_SENDBACK_REASON_ROUTE_NOT_READY = TBUSPP_ERR_ROUTE_NOT_READY,

  // 目标节点（p2p 和 p2g选中的节点）网络异常导致消息无法发送
  TBUSPP_SENDBACK_REASON_NET_MESH_NOT_READY = TBUSPP_ERR_NET_MESH_NOT_READY,

  // P2G消息指定路由类型(route_type)未设置
  TBUSPP_SENDBACK_REASON_WRONG_ROUTE_TYPE = TBUSPP_ERR_WRONG_ROUTE_TYPE,

  // p2g选路失败(1.GROUP_SHARDING_STRICT配置下，shard没有成员 2.灰度规则选路失败)
  TBUSPP_SENDBACK_REASON_ROUTE_FAIL = TBUSPP_ERR_ROUTE_FAIL,

  // p2g消息在选路阶段处理超时, 需要agent开启-msg_expire_secs
  TBUSPP_SENDBACK_REASON_MSG_EXPIRE = TBUSPP_ERR_MSG_EXPIRE
};

typedef enum tbuspp_sendback_reason_t tbuspp_sendback_reason_t;

typedef enum tbuspp_msg_sendback_level_t tbuspp_msg_sendback_level_t;

// shard route policy
enum tbuspp_shard_route_policy_t {
  TBUSPP_GROUP_SHARDING_SAFE = 0,    // 启用，若shard_id 不存在，则全组匹配
  TBUSPP_GROUP_SHARDING_STRICT = 1,  // 严格匹配shard_id，不存在报错
  TBUSPP_SHARDING_OFF = 2            // 取消shard_id匹配，默认全组匹配
};

typedef enum tbuspp_shard_route_policy_t tbuspp_shard_route_policy_t;

// PY_CFFI_END

#pragma pack(push, 1)

struct tbuspp_msg_head_t {
  uint32_t magic;
  uint8_t version;
  uint8_t flags;  // used by tbus2 internal
  uint16_t meta_size;
  uint32_t size;  // head + data
  uint16_t usr_flags;
  uint16_t msg_type;  // message category
  tbuspp_id_t src;    // src busid
  tbuspp_id_t dest;   // dest busid
  uint64_t ctime;     // last change time (abstime, us)

  // 40B
};

struct tbuspp_msg_t {
  struct tbuspp_msg_head_t head;
  // body = [meta_data] + <msg_data>
  // meta_data maybe: None/tbuspp_outq_msg_meta_t/tbuspp_inq_msg_meta_t
  char body[1];
};

struct mq_hash_route_param_t {
  uint64_t hash_key;
};

struct mq_on_version_route_param_t {
  // 目标服务不能低于改版本(endpoint.module_version), 若为0，则忽略
  uint32_t min_require_version;
  // 目标服务不能高于该版本(endpoint.module_version), 若为0，则忽略
  uint32_t max_require_version;
};

// msg meta info in output_mq
struct tbuspp_outq_msg_meta_t {
  uint8_t route_param_type;
  union {
    struct mq_hash_route_param_t hash;  // route_param_type == TBUSPP_ROUTE_PARAM_HASH
    struct mq_on_version_route_param_t
        on_version;  // route_param_type == TBUSPP_ROUTE_PARAM_ON_VERSION
  } route_param;

  // proxy进程转发消息时设置，若为0，则忽略
  tbuspp_id_t proxy_busid;
  // 目标服务sharding的id值，若为0，则忽略
  uint32_t shard_id;
  // 消息路由策略设置，若为0，则使用默认路由策略
  uint8_t require_route_type;
};

// msg meta info in input_mq
struct tbuspp_inq_msg_meta_t {
  // 代理模式转发消息，消息当前接收者为代理节点
  tbuspp_id_t proxy_busid;
  uint32_t group_version;
};

struct tbuspp_msg_meta_v2_t {
  uint8_t type;

  // pb_offset=0, no pb_data, pb_data maybe at pb_data or pb_data_inplace
  uint8_t pb_offset;
  union {
    struct tbuspp_outq_msg_meta_t outq_meta;
    struct tbuspp_inq_msg_meta_t inq_meta;
    char pb_data_inplace[1];
  };
  char pb_data[0];
};

typedef struct tbuspp_msg_meta_v2_t tbuspp_msg_meta_t;

#pragma pack(pop)

#define TBUS2_MAKE_VERSION(major, minor, patch) ((major << 16) | (minor << 8) | patch)

#if __cplusplus >= 201703  // C++17

enum {
  TBUSPP_MSG_FLAG_GROUP_CAST
  [[deprecated("please use TBUSPP_MSG_FLAG_BROADCAST_READY")]] =  // NOLINT
  TBUSPP_MSG_FLAG_BROADCAST_READY
};

#endif

#ifdef __cplusplus

#define TBUS2_NS tbus2

#define TBUS2_NS_BEGIN namespace TBUS2_NS {
#define TBUS2_NS_END \
  }                  \
  ;

#define TBUS2_NS_USING using namespace TBUS2_NS;  // NOLINT

#endif

/**
 * @brief 通过 span_ctx.flags 对应位判断 span_ctx 是否被采样
 */
static inline bool tbuspp_trace_is_sampled_flag(uint32_t flags) {
  return flags & TBUSPP_SPAN_FLAGS_SAMPLED_MASK;
}

/**
 * @brief 通过 span_ctx.flags 对应位判断 span_ctx 是否是 remote span_ctx
 */
static inline bool tbuspp_trace_is_remote_flag(uint32_t flags) {
  return (flags & TBUSPP_SPAN_FLAGS_HAS_IS_REMOTE_MASK) &&
         (flags & TBUSPP_SPAN_FLAGS_IS_REMOTE_MASK);
}

////////////////////////////////////////////////////////////////////////////////

#endif  // TBUSPP2_INC_TBUSPP2_DEFS_H_
