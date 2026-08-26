/*
 * * file name: db_rpc_meta.h
 * * description: dbproxy RPC元信息，包含GetDBMethodCmd()辅助函数
 * */

#ifndef _DB_RPC_META_H_
#define _DB_RPC_META_H_

#include <cstdint>
#include <string>

namespace dbproxy
{

uint32_t GetDBMethodCmd(const std::string& method_name);

}  // namespace dbproxy

#endif
