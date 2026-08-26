/*
 * * file name: room_rpc_meta.cpp
 * * description: GetRoomMethodCmd()实现，见room_rpc_meta.h说明
 */

#include "room_rpc_meta.h"
#include "common/method_cmd.h"
#include "room.pb.h"

namespace roomsvr
{
uint32_t GetRoomMethodCmd(const std::string& method_name)
{
    return app::rpc::GetMethodCmd(CreateRoomReq::descriptor(), "RoomRpcService", method_name);
}

}  // namespace roomsvr
