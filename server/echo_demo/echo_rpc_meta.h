/*
 * * file name: echo_rpc_meta.h
 * * description: echo_demo的RPC元信息定义：
 * *              GetEchoMethodCmd()：从EchoRpcService的protobuf方法描述符读取
 * *              METHOD_CMD选项的辅助函数（纯运行时反射，不依赖protoc插件生成service类）。
 * *              transport下标常量已统一到core/transport_type.h，此处不再定义。
 * */

#ifndef _ECHO_RPC_META_H_
#define _ECHO_RPC_META_H_

#include <cstdint>
#include <string>

namespace echo_demo
{
/// 从EchoRpcService描述符里按方法名取出METHOD_CMD选项值，找不到该方法或未设置选项时返回0
uint32_t GetEchoMethodCmd(const std::string& method_name);

}  // namespace echo_demo

#endif
