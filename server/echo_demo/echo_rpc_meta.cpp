/*
 * * file name: echo_rpc_meta.cpp
 * * description: GetEchoMethodCmd()实现，见echo_rpc_meta.h说明
 * */

#include "echo_rpc_meta.h"
#include "common/method_cmd.h"
#include "echo.pb.h"

namespace echo_demo
{
uint32_t GetEchoMethodCmd(const std::string& method_name)
{
    return app::rpc::GetMethodCmd(EchoRequest::descriptor(), "EchoRpcService", method_name);
}

}  // namespace echo_demo
