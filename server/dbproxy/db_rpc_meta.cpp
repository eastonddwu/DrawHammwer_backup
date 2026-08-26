/*
 * * file name: db_rpc_meta.cpp
 * * description: GetDBMethodCmd()实现，通过protobuf运行时反射读取METHOD_CMD选项
 * */

#include "db_rpc_meta.h"
#include "common/method_cmd.h"
#include "dbproxy.pb.h"

namespace dbproxy
{

uint32_t GetDBMethodCmd(const std::string& method_name)
{
    return app::rpc::GetMethodCmd(app::protocol::CommonGetDataReq::descriptor(), "DBProxyService", method_name);
}

}  // namespace dbproxy
