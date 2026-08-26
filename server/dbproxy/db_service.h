/*
 * * file name: db_service.h
 * * description: dbproxy RPC方法实现，协程模式，处理CommonGetData/CommonSetData
 * */

#ifndef _DB_SERVICE_H_
#define _DB_SERVICE_H_

#include "core/rpc_context.h"

namespace dbproxy
{

class DBService
{
public:
    static void CommonGetData(app::RpcContext& context);
    static void CommonSetData(app::RpcContext& context);
};

}  // namespace dbproxy

#endif
