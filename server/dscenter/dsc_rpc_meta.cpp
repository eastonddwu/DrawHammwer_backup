/*
 * * file name: dsc_rpc_meta.cpp
 * * description: GetDscMethodCmd()实现
 *
 * dscenter.proto没有自己的message类型（所有Req/Resp都在room.proto中），
 * 导致protoc生成的dscenter.pb.h是空壳，不含ServiceDescriptor。
 * 因此无法通过descriptor反射获取METHOD_CMD，改为直接硬编码cmd值。
 * 若dscenter.proto的METHOD_CMD有变更，此处需同步修改。
 */

#include "dsc_rpc_meta.h"

namespace dscenter
{

uint32_t GetDscMethodCmd(const std::string& method_name)
{
    // 与dscenter.proto中的METHOD_CMD option保持一致
    if (method_name == "AllocDsa")
        return 1;
    if (method_name == "ReportDsaLoad")
        return 2;
    return 0;
}

}  // namespace dscenter
