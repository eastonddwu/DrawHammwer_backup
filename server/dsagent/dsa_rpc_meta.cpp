/*
 * * file name: dsa_rpc_meta.cpp
 * * description: GetDsaMethodCmd()实现
 *
 * dsagent.proto没有自己的message类型（所有Req/Resp都在room.proto中），
 * 导致protoc生成的dsagent.pb.h是空壳，不含ServiceDescriptor。
 * 因此无法通过descriptor反射获取METHOD_CMD，改为直接硬编码cmd值。
 * 若dsagent.proto的METHOD_CMD有变更，此处需同步修改。
 */

#include "dsa_rpc_meta.h"

namespace dsagent
{

uint32_t GetDsaMethodCmd(const std::string& method_name)
{
    // 与dsagent.proto中的METHOD_CMD option保持一致
    if (method_name == "CreateGame")
        return 1;
    if (method_name == "DestroyDs")
        return 2;
    if (method_name == "DsHeartBeat")
        return 3;
    if (method_name == "SetDsAuth")
        return 4;
    if (method_name == "DsAuthNtf")
        return 5;
    if (method_name == "DsGetPlayerInfo")
        return 6;
    return 0;
}

}  // namespace dsagent
