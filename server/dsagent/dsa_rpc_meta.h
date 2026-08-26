/*
 * * file name: dsa_rpc_meta.h
 * * description: dsagent的RPC元信息定义
 */

#ifndef _DSA_RPC_META_H_
#define _DSA_RPC_META_H_

#include <cstdint>
#include <string>

namespace dsagent
{
uint32_t GetDsaMethodCmd(const std::string& method_name);

}  // namespace dsagent

#endif
