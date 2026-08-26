/*
 * * file name: dsc_rpc_meta.h
 * * description: dscenter的RPC元信息定义
 */

#ifndef _DSC_RPC_META_H_
#define _DSC_RPC_META_H_

#include <cstdint>
#include <string>

namespace dscenter
{
uint32_t GetDscMethodCmd(const std::string& method_name);

}  // namespace dscenter

#endif
