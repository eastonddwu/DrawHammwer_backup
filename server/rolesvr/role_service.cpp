/*
 * * file name: role_service.cpp
 * * description: RoleService各RPC handler实现
 * *              Login流程: 用gid查询dbproxy.CommonGetData(user_info)，
 * *              查不到则调用CommonSetData创建login和user_info记录，返回玩家信息给connsvr。
 * */

#include "role_service.h"
#include "common/text_util.h"
#include "common/user_info_codec.h"
#include "db_rpc_meta.h"
#include "dbproxy.pb.h"
#include "core/log.h"
#include "core/rpc_error.h"
#include "core/rpc_service.h"
#include "core/svr_type.h"
#include "core/transport_type.h"
#include "role.pb.h"

namespace rolesvr
{

/// 昵称trim后为空或超过8字。与connsvr的kCodeInvalidUserName一致，透传给客户端。
constexpr int32_t kCodeInvalidUserName = 1301;

void RoleService::Login(app::RpcContext& context)
{
    const auto& req = static_cast<const LoginReq&>(context.GetReq());
    uint64_t gid = req.gid();

    APP_LOG_INFO(gid, "RoleService Login recv, gid(%llu), gopenid(%llu), session_id(%u), src(%u)",
                 static_cast<unsigned long long>(gid), static_cast<unsigned long long>(req.gopenid()),
                 req.session_id(), context.head.src);

    // 1. 调用dbproxy.CommonGetData查询user_info，dest用svr_type组地址由路由策略自动选路
    app::protocol::CommonGetDataReq get_req;
    get_req.mutable_key()->set_first(gid);
    get_req.set_table_name("user_info");

    app::protocol::CommonGetDataResp get_rsp;
    uint32_t get_cmd = dbproxy::GetDBMethodCmd("CommonGetData");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, get_cmd, get_req, &get_rsp, nullptr,
        app::kGroupAddrDBProxy, 1000);

    bool is_new = false;
    uint32_t role_type = 0;
    std::string user_name;
    uint64_t points = 0;

    if (ret == app::RPC_SUCCESS && get_rsp.data().size() > 0)
    {
        // 解析dbproxy返回的user_info: is_new(4B) + role_type(4B) + user_name_len(4B) + user_name(NB) + points(8B)
        app::codec::UserInfo info;
        app::codec::DecodeUserInfo(get_rsp.data(), info);
        is_new = (info.is_new != 0);
        role_type = info.role_type;
        user_name = info.user_name;
        points = info.points;

        APP_LOG_INFO(gid, "Login get user_info ok, gid(%lu), is_new(%u), role_type(%u)",
                     static_cast<unsigned long>(gid), is_new ? 1 : 0, role_type);

        // 老用户首次登录后，将is_new标记更新为0（不再是新玩家）
        if (is_new)
        {
            uint32_t set_cmd = dbproxy::GetDBMethodCmd("CommonSetData");

            app::codec::UserInfo update_info;
            update_info.is_new = 0;
            update_info.role_type = role_type;
            update_info.user_name = user_name;
            update_info.points = points;
            std::string update_data = app::codec::EncodeUserInfo(update_info);

            app::protocol::CommonSetDataReq set_req;
            set_req.mutable_key()->set_first(gid);
            set_req.set_table_name("user_info");
            set_req.set_data(update_data);

            app::protocol::CommonSetDataResp set_rsp;
            int32_t set_ret = app::RpcService::GetInst().Rpc(
                app::TRANSPORT_PB_TBUSPP, gid, set_cmd, set_req, &set_rsp, nullptr,
                app::kGroupAddrDBProxy, 1000);
            if (set_ret != app::RPC_SUCCESS)
            {
                APP_LOG_WARN(gid, "Login update is_new fail, ret(%d)", set_ret);
            }
            else
            {
                is_new = false;
            }
        }
    }
    else
    {
        // 查不到或dbproxy不可达，标记为新玩家
        APP_LOG_INFO(gid, "Login user not found or dbproxy unreachable, creating new player, gid(%lu), ret(%d)",
                     static_cast<unsigned long>(gid), ret);
        is_new = true;

        // 尝试调用dbproxy.CommonSetData创建login和user_info记录
        uint32_t set_cmd = dbproxy::GetDBMethodCmd("CommonSetData");

        // 创建login记录
        {
            app::protocol::CommonSetDataReq set_req;
            set_req.mutable_key()->set_first(gid);
            set_req.set_table_name("login");
            uint32_t login_flag = 1;
            set_req.set_data(reinterpret_cast<const char*>(&login_flag), sizeof(login_flag));

            app::protocol::CommonSetDataResp set_rsp;
            int32_t set_ret = app::RpcService::GetInst().Rpc(
                app::TRANSPORT_PB_TBUSPP, gid, set_cmd, set_req, &set_rsp, nullptr,
                app::kGroupAddrDBProxy, 1000);
            if (set_ret != app::RPC_SUCCESS)
            {
                APP_LOG_WARN(gid, "Login create login record fail, ret(%d)", set_ret);
            }
        }

        // 创建user_info记录
        {
            const std::string new_user_name = "画个锤子";
            user_name = new_user_name;

            app::codec::UserInfo new_info;
            new_info.is_new = 1;
            new_info.role_type = 0;
            new_info.user_name = new_user_name;
            new_info.points = 0;
            std::string data = app::codec::EncodeUserInfo(new_info);

            app::protocol::CommonSetDataReq set_req;
            set_req.mutable_key()->set_first(gid);
            set_req.set_table_name("user_info");
            set_req.set_data(data);

            app::protocol::CommonSetDataResp set_rsp;
            int32_t set_ret = app::RpcService::GetInst().Rpc(
                app::TRANSPORT_PB_TBUSPP, gid, set_cmd, set_req, &set_rsp, nullptr,
                app::kGroupAddrDBProxy, 1000);
            if (set_ret != app::RPC_SUCCESS)
            {
                APP_LOG_WARN(gid, "Login create user_info fail, ret(%d)", set_ret);
            }
        }
    }

    auto& rsp = static_cast<LoginResp&>(context.GetRsp());
    rsp.set_gid(gid);
    rsp.set_is_new(is_new);
    rsp.set_role_type(role_type);
    rsp.set_user_name(user_name);
    rsp.set_points(points);

    context.ret_code = app::RPC_SUCCESS;
}

void RoleService::SetUserInfo(app::RpcContext& context)
{
    const auto& req = static_cast<const SetUserInfoReq&>(context.GetReq());
    uint64_t gid = req.gid();

    APP_LOG_INFO(gid, "RoleService SetUserInfo recv, gid(%llu), user_name(\"%s\"), role_type(%u)",
                 static_cast<unsigned long long>(gid), req.user_name().c_str(), req.role_type());

    // 构造user_info二进制数据: is_new(4B) + role_type(4B) + user_name_len(4B) + user_name(NB) + points(8B)
    // is_new置0（注册完成，不再是新玩家）
    uint32_t updated_role_type = req.role_type();
    // 落库前再过一遍8字闸门（connsvr已拦一次，这里防内部调用绕过）；超长拒写，不截断
    std::string updated_user_name;
    if (!app::text::ValidateLength(req.user_name(), app::text::kMaxUserNameLen, &updated_user_name))
    {
        APP_LOG_WARN(gid, "SetUserInfo reject, invalid user_name(\"%s\")", req.user_name().c_str());
        auto& reject_rsp = static_cast<SetUserInfoResp&>(context.GetRsp());
        reject_rsp.set_gid(gid);
        reject_rsp.set_role_type(updated_role_type);
        reject_rsp.set_ret_code(kCodeInvalidUserName);
        context.ret_code = app::RPC_SUCCESS;
        return;
    }

    app::codec::UserInfo info;
    info.is_new = 0;
    info.role_type = updated_role_type;
    info.user_name = updated_user_name;
    info.points = 0;
    std::string data = app::codec::EncodeUserInfo(info);

    // 调用dbproxy.CommonSetData写入user_info
    app::protocol::CommonSetDataReq set_req;
    set_req.mutable_key()->set_first(gid);
    set_req.set_table_name("user_info");
    set_req.set_data(data);

    app::protocol::CommonSetDataResp set_rsp;
    uint32_t set_cmd = dbproxy::GetDBMethodCmd("CommonSetData");
    int32_t ret = app::RpcService::GetInst().Rpc(
        app::TRANSPORT_PB_TBUSPP, gid, set_cmd, set_req, &set_rsp, nullptr,
        app::kGroupAddrDBProxy, 1000);

    auto& rsp = static_cast<SetUserInfoResp&>(context.GetRsp());
    rsp.set_gid(gid);
    rsp.set_user_name(updated_user_name);
    rsp.set_role_type(updated_role_type);

    if (ret == app::RPC_SUCCESS)
    {
        rsp.set_ret_code(0);
        context.ret_code = app::RPC_SUCCESS;
        APP_LOG_INFO(gid, "SetUserInfo ok, gid(%llu), user_name(\"%s\"), role_type(%u)",
                     static_cast<unsigned long long>(gid), updated_user_name.c_str(), updated_role_type);
    }
    else
    {
        rsp.set_ret_code(ret);
        context.ret_code = app::RPC_SUCCESS;
        APP_LOG_WARN(gid, "SetUserInfo dbproxy unreachable, ret(%d)", ret);
    }
}

}  // namespace rolesvr
