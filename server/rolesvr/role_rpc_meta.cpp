/*
 * * file name: role_rpc_meta.cpp
 * * description: GetRoleMethodCmd()实现，见role_rpc_meta.h说明
 * */

#include "role_rpc_meta.h"
#include "common/method_cmd.h"
#include "role.pb.h"

namespace rolesvr
{
uint32_t GetRoleMethodCmd(const std::string& method_name)
{
    return app::rpc::GetMethodCmd(LoginReq::descriptor(), "RoleRpcService", method_name);
}

}  // namespace rolesvr
