/*
 * * file name: method_cmd.h
 * * description: 从protobuf方法描述符读取METHOD_CMD选项的通用辅助函数。
 * *
 * *              proto3默认关闭cc_generic_services，service类不会被protoc生成，
 * *              但ServiceDescriptor仍存在于descriptor pool中。本函数通过某个
 * *              属于同一.proto文件的message的FileDescriptor，按service名+method名
 * *              反射取出METHOD_CMD扩展值。
 * *
 * *              各业务服务原先在 *_rpc_meta.cpp 中重复实现同一段反射逻辑，
 * *              现统一委托到此处（各服务的Get<Xxx>MethodCmd()仅保留一行转发，
 * *              以维持既有命名空间与调用点不变）。
 * *
 * *              注意：dscenter/dsagent的.proto没有自己的message类型（Req/Resp全在
 * *              room.proto），生成的pb.h不含ServiceDescriptor，无法用本函数，
 * *              仍需在各自的 *_rpc_meta.cpp 中硬编码cmd值。
 * */

#ifndef _APP_METHOD_CMD_H_
#define _APP_METHOD_CMD_H_

#include <cstdint>
#include <string>

namespace google
{
namespace protobuf
{
class Descriptor;
}
}  // namespace google

namespace app
{
namespace rpc
{
/// 通过任意一个属于目标.proto的message描述符，按service名+method名反射取出METHOD_CMD选项值。
/// any_msg_desc：目标.proto里任意message的descriptor（用于定位FileDescriptor）。
/// 找不到service/method或未设置选项时返回0。
uint32_t GetMethodCmd(const google::protobuf::Descriptor* any_msg_desc, const std::string& service_name,
                      const std::string& method_name);

}  // namespace rpc
}  // namespace app

#endif
