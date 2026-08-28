/*
 * * file name: db_error.h
 * * description: dbproxy错误码定义，从ua_server的error_code.proto移植
 * */

#ifndef _DB_ERROR_H_
#define _DB_ERROR_H_

#include <cstdint>

namespace dbproxy
{

constexpr int32_t DB_ERR_SUCCESS = 0;
constexpr int32_t DB_ERR_TCAPLUS = -70010;
constexpr int32_t DB_ERR_NOT_DATA = -70011;
constexpr int32_t DB_ERR_DATA_EXIST = -70012;
constexpr int32_t DB_ERR_NOT_FIN = -70013;
constexpr int32_t DB_ERR_SIZE_OVER_FLOW = -70023;
constexpr int32_t DB_ERR_INVALID_VERSION = -70027;
constexpr int32_t DB_ERR_MYSQL = -70030;  // mysql后端专用错误码，与DB_ERR_TCAPLUS并列
constexpr int32_t DB_ERR_BUSY = -70031;   // mysql连接池排队超时，调用方可重试

}  // namespace dbproxy

#endif
