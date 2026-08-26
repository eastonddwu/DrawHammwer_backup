/*
 * * file name: table_util.h
 * * description: TDR表结构与protobuf消息之间的转换工具函数。
 * *              每新增一种TDR表，需要在此添加对应的FillTableKey/ResetTableInfo等特化。
 * */

#ifndef _DB_TABLE_UTIL_H_
#define _DB_TABLE_UTIL_H_

#include <cstring>
#include "dbproxy.pb.h"
#include "table/tb_app_tcaplus.h"

namespace dbproxy
{

// ============================================================================
// LOGIN — 登录表（数值主键: gid + login_flag）
// ============================================================================

inline void FillTableKey(LOGIN& tb, const app::protocol::CommonKey& key)
{
    tb.ullGid = key.first();
}

inline void FillMsgKey(app::protocol::CommonKey& key, const LOGIN& tb)
{
    key.set_first(tb.ullGid);
}

inline void ResetTableInfo(LOGIN& tb)
{
    memset(&tb, 0, sizeof(tb));
}

// ============================================================================
// USER_INFO — 用户信息表（数值主键: gid + is_new + role_type + user_name + points）
// ============================================================================

inline void FillTableKey(USER_INFO& tb, const app::protocol::CommonKey& key)
{
    tb.ullGid = key.first();
}

inline void FillMsgKey(app::protocol::CommonKey& key, const USER_INFO& tb)
{
    key.set_first(tb.ullGid);
}

inline void ResetTableInfo(USER_INFO& tb)
{
    memset(&tb, 0, sizeof(tb));
}

}  // namespace dbproxy

#endif
