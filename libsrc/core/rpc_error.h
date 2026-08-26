/*
 * * file name: rpc_error.h
 * * description: RPC相关错误码定义
 * */

#ifndef _APP_RPC_ERROR_H_
#define _APP_RPC_ERROR_H_

#include <cstdint>

namespace app
{
constexpr int32_t RPC_SUCCESS = 0;
constexpr int32_t RPC_SYS_ERR = -1;
constexpr int32_t RPC_CHANNEL_SEND_ERR = -2;
constexpr int32_t RPC_TIME_OUT = -3;
constexpr int32_t RPC_SEND_MSG_TOO_LONG = -4;
constexpr int32_t RPC_MSG_ENCODE_ERR = -5;
constexpr int32_t RPC_RECV_MSG_TOO_LONG = -6;
constexpr int32_t RPC_MSG_DECODE_ERR = -7;
constexpr int32_t RPC_METHOD_NOT_FOUND = -8;
constexpr int32_t RPC_ROUTER_FIND_DST_ERR = -9;

}  // namespace app

#endif
