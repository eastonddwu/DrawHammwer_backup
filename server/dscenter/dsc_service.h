/*
 * * file name: dsc_service.h
 * * description: dscenter具体RPC方法实现
 */

#ifndef _DSC_SERVICE_H_
#define _DSC_SERVICE_H_

#include "core/rpc_context.h"

namespace dscenter
{
class DscService
{
public:
    /// 分配DSAgent：选择负载最低的DSA
    static void AllocDsa(app::RpcContext& context);
    /// DSA负载上报
    static void ReportDsaLoad(app::RpcContext& context);
};

}  // namespace dscenter

#endif
