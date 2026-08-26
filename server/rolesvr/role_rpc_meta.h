/*
 * * file name: role_rpc_meta.h
 * * description: rolesvr的RPC元信息定义：
 * *              GetRoleMethodCmd()：从RoleRpcService的protobuf方法描述符读取
 * *              METHOD_CMD选项的辅助函数(纯运行时反射，不依赖protoc插件生成service类)，
 * *              模式与echo_demo的echo_rpc_meta/connsvr的conn_rpc_meta一致。
 * *              transport下标常量已统一到core/transport_type.h，此处不再定义。
 * */

#ifndef _ROLE_RPC_META_H_
#define _ROLE_RPC_META_H_

#include <cstdint>
#include <string>

namespace rolesvr
{
/// 从RoleRpcService描述符里按方法名取出METHOD_CMD选项值，找不到该方法或未设置选项时返回0
uint32_t GetRoleMethodCmd(const std::string& method_name);

}  // namespace rolesvr

#endif
