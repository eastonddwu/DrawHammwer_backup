/*
 * * file name: role_service.h
 * * description: rolesvr具体RPC方法实现
 * */

#ifndef _ROLE_SERVICE_H_
#define _ROLE_SERVICE_H_

#include "core/rpc_context.h"

namespace rolesvr
{
class RoleService
{
public:
    /// 登录：查询dbproxy获取角色数据，不存在则创建新玩家
    static void Login(app::RpcContext& context);

    /// 新用户注册：更新user_info（设置user_name/role_type），并清除is_new标记
    static void SetUserInfo(app::RpcContext& context);
};

}  // namespace rolesvr

#endif
