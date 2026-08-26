/*
 * * file name: room_rpc_meta.h
 * * description: roomsvr的RPC元信息定义，GetRoomMethodCmd()从RoomRpcService的
 * *              protobuf方法描述符读取METHOD_CMD选项值
 */

#ifndef _ROOM_RPC_META_H_
#define _ROOM_RPC_META_H_

#include <cstdint>
#include <string>

namespace roomsvr
{
/// 从RoomRpcService描述符里按方法名取出METHOD_CMD选项值，找不到返回0
uint32_t GetRoomMethodCmd(const std::string& method_name);

}  // namespace roomsvr

#endif
