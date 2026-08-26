/*
 * * file name: base_server.cpp
 * * description: ...
 * */

#include "svr_base/base_server.h"
#include "coroutine/app_coroutine.h"

namespace app
{
bool BaseServer::SvrInitImpl(uint32_t max_coro_num, uint32_t max_deal_pkg_num)
{
    ServerCore::SvrOption option;
    option.coroutine = &(CoroutineMgr::GetInst());
    if (max_coro_num > 0)
        option.max_coro_num = max_coro_num;
    if (max_deal_pkg_num > 0)
        option.max_deal_pkg_num = max_deal_pkg_num;

    return ServerCore::SvrInit(option);
}

}  // namespace app
