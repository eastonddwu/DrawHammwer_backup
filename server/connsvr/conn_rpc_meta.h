/*
 * * file name: conn_rpc_meta.h
 * * description: connsvr的RPC元信息定义：
 * *              GetConnMethodCmd()：从ConnRpcService的protobuf方法描述符读取
 * *              METHOD_CMD选项的辅助函数(纯运行时反射，不依赖protoc插件生成service类)，
 * *              模式与echo_demo的echo_rpc_meta一致。
 * *              transport下标常量已统一到core/transport_type.h，此处不再定义。
 * */

#ifndef _CONN_RPC_META_H_
#define _CONN_RPC_META_H_

#include <cstdint>
#include <string>

namespace connsvr
{
/// 从ConnRpcService描述符里按方法名取出METHOD_CMD选项值，找不到该方法或未设置选项时返回0
uint32_t GetConnMethodCmd(const std::string& method_name);

}  // namespace connsvr

#endif
