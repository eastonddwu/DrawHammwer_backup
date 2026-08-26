/*
 * * file name: conn_rpc_meta.cpp
 * * description: GetConnMethodCmd()实现，见conn_rpc_meta.h说明
 * */

#include "conn_rpc_meta.h"
#include "common/method_cmd.h"
#include "conn.pb.h"

namespace connsvr
{
uint32_t GetConnMethodCmd(const std::string& method_name)
{
    return app::rpc::GetMethodCmd(LoginReq::descriptor(), "ConnRpcService", method_name);
}

}  // namespace connsvr
