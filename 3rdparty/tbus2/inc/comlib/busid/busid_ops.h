// Copyright (c) Tencent
// Author: longfeilu
// Create: 2022-02-24

#ifndef COMLIB_BUSID_BUSID_OPS_H_
#define COMLIB_BUSID_BUSID_OPS_H_

#pragma once

#include "comlib/defs/comdefs.h"

////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

// PY_CFFI_START

/**
 * \defgroup busid operations, only for executable binaries
 * @{
 */

/**
 * 设置全局的busid模板定义
 * @note 如未进行设置，则为8.8.8.8/16格式
 * @param temp          点分式模板，比特总数不超过56，格式为 [bitsize#]x.x.x.*
 * @param gid_mask      busid & gid_mask = gid, if gid_mask == 0, 不启用group
 * @param gid_mask_hex  以 hex string 形式提供 gid_mask, 必须以 "0x" (or "0X") 开头，如果
 * gid_mask_hex != NULL，则忽略 gid_mask
 *
 * @return !=0 失败
 */
MGSE_API int tbuspp_busid_template_init(const char *temp, tbuspp_id_t gid_mask,
                                        const char *gid_mask_hex);

MGSE_API tbuspp_id_t tbuspp_get_gid_mask();
MGSE_API const char *tbuspp_get_busid_template();
MGSE_API tbuspp_id_t tbuspp_get_gid_from_busid(tbuspp_id_t busid);

// 获取当前busid_template busid 分段数
MGSE_API int tbuspp_get_busid_fields_num();

// PY_CFFI_END

inline uint64_t tbuspp_busid_ingroup_id(tbuspp_id_t busid) {
  tbuspp_id_t mask = tbuspp_get_gid_mask();
  return ~mask & busid;
}

// PY_CFFI_START
MGSE_API tbuspp_id_t tbuspp_busid_get_group_internal_id(tbuspp_id_t busid);
MGSE_API bool tbuspp_busid_is_groupid(tbuspp_id_t busid);
MGSE_API bool tbuspp_is_same_group(tbuspp_id_t busid1, tbuspp_id_t busid2);

// gid 通配比较
MGSE_API bool tbuspp_busid_match_glob_gid(tbuspp_id_t glob_gid, tbuspp_id_t gid);

/* 获得 busid 点分字符串
  @note 如下 busid ntoa/aton 转换需要在调用 tbuspp_busid_template_init() 设置busid template 后才有效
在 endpoint 完成注册后，Agent 会自动下发系统配置的 busid_template/gid_mask，tbuspp_open() 调用内完成
busid template 初始化
  @param busid  busid
  @param buf  保存busid 点分格式，如提供buf，buf 大小按 TBUSPP_ALIAS_MAX_SIZE + 1 设置,
若为NULL，则使用共享 static buffer (thread safe)
  @return busid_str buffer pointer, if buf != nullptr, 则为buf

  如果设置buf ==
nullptr,注意不要连续使用返回值作为其它函数参数，这会导致两个参数是相同值，类似如下：
  printf("busid1=%s,busid2=%s", tbuspp_busid_ntoa(busi1, NULL), tbuspp_busid_aton(busid2, NULL));
//WRONG

  如果 busid_template 未正确初始化，会返回 busid 的hex string形式
 */
MGSE_API const char *tbuspp_busid_ntoa(tbuspp_id_t busid, char *buf);

//! @note 如果 busid_template 未正确初始化，返回 0
MGSE_API tbuspp_id_t tbuspp_busid_aton(const char *busid_str);

//! @note 确认busid_str是否是alias
MGSE_API bool tbuspp_busid_is_alias(const char *busid_str);

//! @note 在跨集群通信中，判断domain是否合法值
MGSE_API bool tbuspp_busid_is_valid_domain(tbuspp_id_t domain_id);

//! @note 在跨集群通信中，指定busid所属的外部domain
MGSE_API tbuspp_id_t tbuspp_busid_set_domain_id(tbuspp_id_t busid, tbuspp_id_t domain_id);

//! @note 在跨集群通信中，从busid中获取domain
MGSE_API tbuspp_id_t tbuspp_busid_get_domain_id(tbuspp_id_t busid);

//! @note 在跨集群通信中，指定alias所属的外部domain
//  alias的buf空间按 TBUSPP_ALIAS_MAX_SIZE + 1 设置
MGSE_API const char *tbuspp_busid_alias_set_domain_id(char *alias, tbuspp_id_t domain_id);

//! @note 在跨集群通信中，从alias中获取domain
MGSE_API tbuspp_id_t tbuspp_busid_alias_get_domain_id(const char *alias);

//! @note 在跨集群通信中，根据当前domain_id的位数获取指定为广播到所有domain的domain_id
//  return 0xFFULL for 8bits domain, 0xFFFFULL for 16bits domain
MGSE_API tbuspp_id_t tbuspp_busid_get_broadcast_domain_id();

//! @note 在跨集群通信中，是否启用16位的domain
MGSE_API int tbuspp_busid_enable_16bits_domain_id(bool enable);

//! @note 在跨集群通信中，是否已启用16位的domain
MGSE_API bool tbuspp_busid_is_16bits_domain_id();

//! @note 判断busid_str是否group_alias
MGSE_API bool tbuspp_busid_is_group_alias(const char *busid_str);

//! @note 判断busid_str是否glob_alias
MGSE_API bool tbuspp_busid_is_glob_alias(const char *busid_str);

//! @note 判断alias是否完整，残缺部分需要自动分配id
MGSE_API bool tbuspp_busid_is_incomplete_alias(const char *busid_str);

/**}@*/

// PY_CFFI_END

static inline bool tbuspp_is_local_domain(tbuspp_id_t busid) {
  return tbuspp_busid_get_domain_id(busid) == TBUSPP_DOMAIN_LOCAL;
}

#ifdef __cplusplus
}
#endif

#endif  // COMLIB_BUSID_BUSID_OPS_H_
