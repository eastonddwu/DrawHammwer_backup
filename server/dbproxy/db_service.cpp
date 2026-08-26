/*
 * * file name: db_service.cpp
 * * description: dbproxy RPC方法实现，协程模式。
 * *              根据请求中的table_name分发到对应TDR表的操作。
 * *              新增表时，在此添加对应的handler分支即可。
 * */

#include "db_service.h"
#include "common/user_info_codec.h"
#include "core/log.h"
#include "db_app.h"
#include "dbproxy.pb.h"
#include "mysql_wrap.h"
#include "table/tb_app_tcaplus.h"
#include "tcaplus_wrap.h"
#include "utils/db_error.h"
#include "utils/table_util.h"

using namespace TcaplusService;

namespace dbproxy
{

// ============================================================================
// CommonGetData — 通用读取，根据请求的table_name分发
// ============================================================================

void DBService::CommonGetData(app::RpcContext& context)
{
    const auto& req = static_cast<const app::protocol::CommonGetDataReq&>(context.GetReq());
    auto& rsp = static_cast<app::protocol::CommonGetDataResp&>(context.GetRsp());
    const std::string& table_name = req.table_name();

    APP_LOG_INFO(context.head.gid, "CommonGetData gid=%lu, table(%s)",
                 req.key().first(), table_name.c_str());

    const bool use_mysql = (DBApp::GetInst().GetDbBackend() == DBApp::DbBackend::kMysql);

    if (table_name == "user_info")
    {
        // 新增：mysql后端分支，与下方原有tcaplus分支二选一，tcaplus代码保持不变
        if (use_mysql)
        {
            USER_INFO tb_info;
            int32_t data_version = 0;
            int ret = MysqlWrap::GetInst().GetUserInfo(context.head.gid, tb_info, data_version);
            if (ret == 0)
            {
                app::codec::UserInfo info;
                info.is_new = tb_info.dwIs_new;
                info.role_type = tb_info.dwRole_type;
                info.user_name.assign(tb_info.szUser_name,
                                      strnlen(tb_info.szUser_name, sizeof(tb_info.szUser_name)));
                info.points = tb_info.ullPoints;
                rsp.set_data(app::codec::EncodeUserInfo(info));
            }
            else if (ret != DB_ERR_NOT_DATA)
            {
                APP_LOG_WARN(context.head.gid, "CommonGetData(user_info,mysql) fail, ret=%d", ret);
            }
            rsp.mutable_key()->CopyFrom(req.key());
            rsp.set_table_name(table_name);
            rsp.set_data_version(data_version);
            context.ret_code = (ret == DB_ERR_NOT_DATA) ? 0 : ret;
            return;
        }

        USER_INFO tb_info;
        ResetTableInfo(tb_info);
        FillTableKey(tb_info, req.key());

        int32_t data_version = 0;
        int ret = TcapWrap::GetInst().SendTcapReqCoroutine<USER_INFO>(
            context.head.gid,
            [&rsp, &data_version, gid = context.head.gid](
                int ret_code, TcaplusServiceResponse& tcap_rsp) -> int {
                if (ret_code != 0)
                    return ret_code;
                USER_INFO parsed;
                int32_t dv = 0;
                int r = TcapWrap::GetInst().ParseOneData(gid, tcap_rsp, parsed, dv);
                data_version = dv;
                if (r == 0)
                {
                    // 序列化user_info字段到response.data
                    // 格式: is_new(4B) + role_type(4B) + user_name_len(4B) + user_name(NB) + points(8B)
                    app::codec::UserInfo info;
                    info.is_new = parsed.dwIs_new;
                    info.role_type = parsed.dwRole_type;
                    info.user_name.assign(parsed.szUser_name,
                                          strnlen(parsed.szUser_name, sizeof(parsed.szUser_name)));
                    info.points = parsed.ullPoints;
                    rsp.set_data(app::codec::EncodeUserInfo(info));
                }
                return r;
            },
            "user_info", TCAPLUS_API_GET_REQ, tb_info);

        if (ret != 0 && ret != DB_ERR_NOT_DATA)
            APP_LOG_WARN(context.head.gid, "CommonGetData(user_info) fail, ret=%d", ret);

        rsp.mutable_key()->CopyFrom(req.key());
        rsp.set_table_name(table_name);
        rsp.set_data_version(data_version);
        context.ret_code = (ret == DB_ERR_NOT_DATA) ? 0 : ret;
        return;
    }

    // 默认走login表
    // 新增：mysql后端分支，与下方原有tcaplus分支二选一，tcaplus代码保持不变
    if (use_mysql)
    {
        LOGIN tb_info;
        int32_t data_version = 0;
        int ret = MysqlWrap::GetInst().GetLogin(context.head.gid, tb_info, data_version);
        if (ret == 0)
        {
            uint32_t flag = tb_info.dwLogin_flag;
            rsp.set_data(reinterpret_cast<const char*>(&flag), sizeof(flag));
        }
        else if (ret != DB_ERR_NOT_DATA)
        {
            APP_LOG_WARN(context.head.gid, "CommonGetData(login,mysql) fail, ret=%d", ret);
        }
        rsp.mutable_key()->CopyFrom(req.key());
        rsp.set_table_name("login");
        rsp.set_data_version(data_version);
        context.ret_code = (ret == DB_ERR_NOT_DATA) ? 0 : ret;
        return;
    }

    {
        LOGIN tb_info;
        ResetTableInfo(tb_info);
        FillTableKey(tb_info, req.key());

        int32_t data_version = 0;
        int ret = TcapWrap::GetInst().SendTcapReqCoroutine<LOGIN>(
            context.head.gid,
            [&rsp, &data_version, gid = context.head.gid](
                int ret_code, TcaplusServiceResponse& tcap_rsp) -> int {
                if (ret_code != 0)
                    return ret_code;
                LOGIN parsed;
                int32_t dv = 0;
                int r = TcapWrap::GetInst().ParseOneData(gid, tcap_rsp, parsed, dv);
                data_version = dv;
                if (r == 0)
                {
                    uint32_t flag = parsed.dwLogin_flag;
                    rsp.set_data(reinterpret_cast<const char*>(&flag), sizeof(flag));
                }
                return r;
            },
            "login", TCAPLUS_API_GET_REQ, tb_info);

        if (ret != 0 && ret != DB_ERR_NOT_DATA)
            APP_LOG_WARN(context.head.gid, "CommonGetData(login) fail, ret=%d", ret);

        rsp.mutable_key()->CopyFrom(req.key());
        rsp.set_table_name("login");
        rsp.set_data_version(data_version);
        context.ret_code = (ret == DB_ERR_NOT_DATA) ? 0 : ret;
    }
}

// ============================================================================
// CommonSetData — 通用写入（REPLACE语义），根据请求的table_name分发
// ============================================================================

void DBService::CommonSetData(app::RpcContext& context)
{
    const auto& req = static_cast<const app::protocol::CommonSetDataReq&>(context.GetReq());
    (void)static_cast<app::protocol::CommonSetDataResp&>(context.GetRsp());
    const std::string& table_name = req.table_name();

    APP_LOG_INFO(context.head.gid, "CommonSetData gid=%lu, data_len=%zu, table(%s)",
                 req.key().first(), req.data().size(), table_name.c_str());

    const bool use_mysql = (DBApp::GetInst().GetDbBackend() == DBApp::DbBackend::kMysql);

    if (table_name == "user_info")
    {
        // 新增：mysql后端分支，与下方原有tcaplus分支二选一，tcaplus代码保持不变
        if (use_mysql)
        {
            USER_INFO tb_info;
            ResetTableInfo(tb_info);
            FillTableKey(tb_info, req.key());

            app::codec::UserInfo info;
            app::codec::DecodeUserInfo(req.data(), info);
            tb_info.dwIs_new = info.is_new;
            tb_info.dwRole_type = info.role_type;
            if (!info.user_name.empty() && info.user_name.size() < sizeof(tb_info.szUser_name))
            {
                memcpy(tb_info.szUser_name, info.user_name.data(), info.user_name.size());
            }
            tb_info.ullPoints = info.points;

            int ret = MysqlWrap::GetInst().SetUserInfo(context.head.gid, tb_info, req.data_version());
            if (ret != 0)
                APP_LOG_WARN(context.head.gid, "CommonSetData(user_info,mysql) fail, ret=%d", ret);
            context.ret_code = ret;
            return;
        }

        USER_INFO tb_info;
        ResetTableInfo(tb_info);
        FillTableKey(tb_info, req.key());

        // 解析data: is_new(4B) + role_type(4B) + user_name_len(4B) + user_name(NB) + points(8B)
        app::codec::UserInfo info;
        app::codec::DecodeUserInfo(req.data(), info);
        tb_info.dwIs_new = info.is_new;
        tb_info.dwRole_type = info.role_type;
        if (!info.user_name.empty() && info.user_name.size() < sizeof(tb_info.szUser_name))
        {
            memcpy(tb_info.szUser_name, info.user_name.data(), info.user_name.size());
        }
        tb_info.ullPoints = info.points;

        int ret = TcapWrap::GetInst().SendTcapReqCoroutine<USER_INFO>(
            context.head.gid,
            [](int ret_code, TcaplusServiceResponse&) -> int {
                return (ret_code == 0) ? 0 : ret_code;
            },
            "user_info", TCAPLUS_API_REPLACE_REQ, tb_info, req.data_version());

        if (ret != 0)
            APP_LOG_WARN(context.head.gid, "CommonSetData(user_info) fail, ret=%d", ret);

        context.ret_code = ret;
        return;
    }

    // 默认走login表
    // 新增：mysql后端分支，与下方原有tcaplus分支二选一，tcaplus代码保持不变
    if (use_mysql)
    {
        LOGIN tb_info;
        ResetTableInfo(tb_info);
        FillTableKey(tb_info, req.key());

        if (req.data().size() >= sizeof(uint32_t))
        {
            uint32_t flag;
            memcpy(&flag, req.data().data(), sizeof(flag));
            tb_info.dwLogin_flag = flag;
        }
        else
        {
            tb_info.dwLogin_flag = 1;
        }

        int ret = MysqlWrap::GetInst().SetLogin(context.head.gid, tb_info, req.data_version());
        if (ret != 0)
            APP_LOG_WARN(context.head.gid, "CommonSetData(login,mysql) fail, ret=%d", ret);
        context.ret_code = ret;
        return;
    }

    {
        LOGIN tb_info;
        ResetTableInfo(tb_info);
        FillTableKey(tb_info, req.key());

        if (req.data().size() >= sizeof(uint32_t))
        {
            uint32_t flag;
            memcpy(&flag, req.data().data(), sizeof(flag));
            tb_info.dwLogin_flag = flag;
        }
        else
        {
            tb_info.dwLogin_flag = 1;
        }

        int ret = TcapWrap::GetInst().SendTcapReqCoroutine<LOGIN>(
            context.head.gid,
            [](int ret_code, TcaplusServiceResponse&) -> int {
                return (ret_code == 0) ? 0 : ret_code;
            },
            "login", TCAPLUS_API_REPLACE_REQ, tb_info, req.data_version());

        if (ret != 0)
            APP_LOG_WARN(context.head.gid, "CommonSetData(login) fail, ret=%d", ret);

        context.ret_code = ret;
    }
}

}  // namespace dbproxy
