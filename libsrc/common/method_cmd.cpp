/*
 * * file name: method_cmd.cpp
 * * description: GetMethodCmd()实现，见method_cmd.h说明
 * */

#include "method_cmd.h"
#include <google/protobuf/descriptor.h>
#include "rpc_options.pb.h"

namespace app
{
namespace rpc
{
uint32_t GetMethodCmd(const google::protobuf::Descriptor* any_msg_desc, const std::string& service_name,
                      const std::string& method_name)
{
    if (any_msg_desc == nullptr)
        return 0;

    const google::protobuf::FileDescriptor* file_desc = any_msg_desc->file();
    const google::protobuf::ServiceDescriptor* service_desc = file_desc->FindServiceByName(service_name);
    if (service_desc == nullptr)
        return 0;

    const google::protobuf::MethodDescriptor* method_desc = service_desc->FindMethodByName(method_name);
    if (method_desc == nullptr)
        return 0;

    return method_desc->options().GetExtension(app::protocol::METHOD_CMD);
}

}  // namespace rpc
}  // namespace app
