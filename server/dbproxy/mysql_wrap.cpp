/*
 * * file name: mysql_wrap.cpp
 * * description: MySQL业务层封装实现，见mysql_wrap.h说明
 * */

#include "mysql_wrap.h"
#include <algorithm>
#include <cstring>
#include "core/log.h"
#include "utils/db_error.h"

namespace dbproxy
{

int MysqlWrap::Init(const MysqlConf& conf)
{
    conn_ = mysql_init(nullptr);
    if (!conn_)
    {
        APP_LOG_ERROR(0, "mysql_init fail");
        return DB_ERR_MYSQL;
    }

    // 显式管理事务边界(START TRANSACTION/COMMIT/ROLLBACK)，关闭autocommit避免隐式提交
    my_bool reconnect = 1;
    mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(conn_, conf.host.c_str(), conf.user.c_str(), conf.password.c_str(),
                             conf.database.c_str(), conf.port, nullptr, 0))
    {
        APP_LOG_ERROR(0, "mysql_real_connect fail, host=%s:%u, db=%s, err=%s",
                      conf.host.c_str(), conf.port, conf.database.c_str(), mysql_error(conn_));
        mysql_close(conn_);
        conn_ = nullptr;
        return DB_ERR_MYSQL;
    }

    if (mysql_autocommit(conn_, 0) != 0)
    {
        APP_LOG_ERROR(0, "mysql_autocommit(off) fail, err=%s", mysql_error(conn_));
        mysql_close(conn_);
        conn_ = nullptr;
        return DB_ERR_MYSQL;
    }

    std::string tables;
    for (size_t i = 0; i < conf.table_names.size(); i++)
    {
        if (i > 0) tables += ",";
        tables += conf.table_names[i];
    }
    APP_LOG_INFO(0, "mysql init succ, host=%s:%u, db=%s, tables=[%s]",
                 conf.host.c_str(), conf.port, conf.database.c_str(), tables.c_str());
    return 0;
}

void MysqlWrap::Finish()
{
    if (conn_)
    {
        mysql_close(conn_);
        conn_ = nullptr;
    }
}

int MysqlWrap::SelectVersionForUpdate(const char* table_name, uint64_t gid, int32_t& version, bool& exists)
{
    version = 0;
    exists = false;

    std::string sql = std::string("SELECT data_version FROM ") + table_name + " WHERE gid=? FOR UPDATE";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt)
        return DB_ERR_MYSQL;

    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0)
    {
        APP_LOG_WARN(gid, "SelectVersionForUpdate(%s) prepare fail: %s", table_name, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    MYSQL_BIND param = {};
    param.buffer_type = MYSQL_TYPE_LONGLONG;
    param.buffer = &gid;
    param.is_unsigned = 1;
    if (mysql_stmt_bind_param(stmt, &param) != 0)
    {
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        APP_LOG_WARN(gid, "SelectVersionForUpdate(%s) execute fail: %s", table_name, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    int32_t dv = 0;
    MYSQL_BIND result = {};
    result.buffer_type = MYSQL_TYPE_LONG;
    result.buffer = &dv;

    if (mysql_stmt_bind_result(stmt, &result) != 0 || mysql_stmt_store_result(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    int fetch_ret = mysql_stmt_fetch(stmt);
    mysql_stmt_close(stmt);

    if (fetch_ret == MYSQL_NO_DATA)
        return 0;  // exists=false, 记录不存在
    if (fetch_ret != 0)
        return DB_ERR_MYSQL;

    exists = true;
    version = dv;
    return 0;
}

// ============================================================================
// LOGIN
// ============================================================================

int MysqlWrap::GetLogin(uint64_t gid, LOGIN& out, int32_t& data_version)
{
    memset(&out, 0, sizeof(out));
    out.ullGid = gid;
    data_version = 0;

    const char* sql = "SELECT login_flag, data_version FROM login WHERE gid=?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt)
        return DB_ERR_MYSQL;

    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0)
    {
        APP_LOG_WARN(gid, "GetLogin prepare fail: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    MYSQL_BIND param = {};
    param.buffer_type = MYSQL_TYPE_LONGLONG;
    param.buffer = &gid;
    param.is_unsigned = 1;
    if (mysql_stmt_bind_param(stmt, &param) != 0)
    {
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        APP_LOG_WARN(gid, "GetLogin execute fail: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    uint32_t login_flag = 0;
    int32_t dv = 0;
    MYSQL_BIND result[2] = {};
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &login_flag;
    result[0].is_unsigned = 1;
    result[1].buffer_type = MYSQL_TYPE_LONG;
    result[1].buffer = &dv;

    if (mysql_stmt_bind_result(stmt, result) != 0 || mysql_stmt_store_result(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    int fetch_ret = mysql_stmt_fetch(stmt);
    mysql_stmt_close(stmt);

    if (fetch_ret == MYSQL_NO_DATA)
        return DB_ERR_NOT_DATA;
    if (fetch_ret != 0)
        return DB_ERR_MYSQL;

    out.dwLogin_flag = login_flag;
    data_version = dv;
    return 0;
}

int MysqlWrap::SetLogin(uint64_t gid, const LOGIN& in, int32_t data_version)
{
    if (mysql_query(conn_, "START TRANSACTION") != 0)
    {
        APP_LOG_WARN(gid, "SetLogin START TRANSACTION fail: %s", mysql_error(conn_));
        return DB_ERR_MYSQL;
    }

    int32_t cur_version = 0;
    bool exists = false;
    int ret = SelectVersionForUpdate("login", gid, cur_version, exists);
    if (ret != 0)
    {
        mysql_query(conn_, "ROLLBACK");
        return ret;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt)
    {
        mysql_query(conn_, "ROLLBACK");
        return DB_ERR_MYSQL;
    }

    uint32_t login_flag = in.dwLogin_flag;
    bool ok = false;

    if (!exists)
    {
        const char* sql = "INSERT INTO login (gid, login_flag, data_version) VALUES (?, ?, 1)";
        if (mysql_stmt_prepare(stmt, sql, strlen(sql)) == 0)
        {
            MYSQL_BIND param[2] = {};
            param[0].buffer_type = MYSQL_TYPE_LONGLONG;
            param[0].buffer = &gid;
            param[0].is_unsigned = 1;
            param[1].buffer_type = MYSQL_TYPE_LONG;
            param[1].buffer = &login_flag;
            param[1].is_unsigned = 1;
            ok = (mysql_stmt_bind_param(stmt, param) == 0) && (mysql_stmt_execute(stmt) == 0);
        }
    }
    else if (data_version != 0 && cur_version != data_version)
    {
        // data_version==0 表示调用方（如rolesvr）未做版本追踪/CAS，语义等价于tcaplus REPLACE_REQ
        // 在该场景下的"无条件覆盖写+自动增版本"行为；仅当调用方显式传入非0版本号时才做校验拦截。
        mysql_stmt_close(stmt);
        mysql_query(conn_, "ROLLBACK");
        APP_LOG_WARN(gid, "SetLogin version mismatch, cur=%d, req=%d", cur_version, data_version);
        return DB_ERR_INVALID_VERSION;
    }
    else
    {
        const char* sql = "UPDATE login SET login_flag=?, data_version=data_version+1 WHERE gid=?";
        if (mysql_stmt_prepare(stmt, sql, strlen(sql)) == 0)
        {
            MYSQL_BIND param[2] = {};
            param[0].buffer_type = MYSQL_TYPE_LONG;
            param[0].buffer = &login_flag;
            param[0].is_unsigned = 1;
            param[1].buffer_type = MYSQL_TYPE_LONGLONG;
            param[1].buffer = &gid;
            param[1].is_unsigned = 1;
            ok = (mysql_stmt_bind_param(stmt, param) == 0) && (mysql_stmt_execute(stmt) == 0);
        }
    }

    if (!ok)
        APP_LOG_WARN(gid, "SetLogin stmt fail: %s", mysql_stmt_error(stmt));
    mysql_stmt_close(stmt);

    if (!ok)
    {
        mysql_query(conn_, "ROLLBACK");
        return DB_ERR_MYSQL;
    }

    if (mysql_query(conn_, "COMMIT") != 0)
    {
        APP_LOG_WARN(gid, "SetLogin COMMIT fail: %s", mysql_error(conn_));
        return DB_ERR_MYSQL;
    }
    return 0;
}

// ============================================================================
// USER_INFO
// ============================================================================

int MysqlWrap::GetUserInfo(uint64_t gid, USER_INFO& out, int32_t& data_version)
{
    memset(&out, 0, sizeof(out));
    out.ullGid = gid;
    data_version = 0;

    const char* sql = "SELECT is_new, role_type, user_name, points, data_version FROM user_info WHERE gid=?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt)
        return DB_ERR_MYSQL;

    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0)
    {
        APP_LOG_WARN(gid, "GetUserInfo prepare fail: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    MYSQL_BIND param = {};
    param.buffer_type = MYSQL_TYPE_LONGLONG;
    param.buffer = &gid;
    param.is_unsigned = 1;
    if (mysql_stmt_bind_param(stmt, &param) != 0)
    {
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        APP_LOG_WARN(gid, "GetUserInfo execute fail: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    uint32_t is_new = 0, role_type = 0;
    uint64_t points = 0;
    int32_t dv = 0;
    char user_name[128] = {};
    unsigned long user_name_len = 0;
    my_bool user_name_is_null = 0;

    MYSQL_BIND result[5] = {};
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &is_new;
    result[0].is_unsigned = 1;
    result[1].buffer_type = MYSQL_TYPE_LONG;
    result[1].buffer = &role_type;
    result[1].is_unsigned = 1;
    result[2].buffer_type = MYSQL_TYPE_STRING;
    result[2].buffer = user_name;
    result[2].buffer_length = sizeof(user_name);
    result[2].length = &user_name_len;
    result[2].is_null = &user_name_is_null;
    result[3].buffer_type = MYSQL_TYPE_LONGLONG;
    result[3].buffer = &points;
    result[3].is_unsigned = 1;
    result[4].buffer_type = MYSQL_TYPE_LONG;
    result[4].buffer = &dv;

    if (mysql_stmt_bind_result(stmt, result) != 0 || mysql_stmt_store_result(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return DB_ERR_MYSQL;
    }

    int fetch_ret = mysql_stmt_fetch(stmt);
    mysql_stmt_close(stmt);

    if (fetch_ret == MYSQL_NO_DATA)
        return DB_ERR_NOT_DATA;
    if (fetch_ret != 0 && fetch_ret != MYSQL_DATA_TRUNCATED)
        return DB_ERR_MYSQL;

    out.dwIs_new = is_new;
    out.dwRole_type = role_type;
    size_t copy_len = std::min<unsigned long>(user_name_len, sizeof(out.szUser_name) - 1);
    if (!user_name_is_null && copy_len > 0)
        memcpy(out.szUser_name, user_name, copy_len);
    out.ullPoints = points;
    data_version = dv;
    return 0;
}

int MysqlWrap::SetUserInfo(uint64_t gid, const USER_INFO& in, int32_t data_version)
{
    if (mysql_query(conn_, "START TRANSACTION") != 0)
    {
        APP_LOG_WARN(gid, "SetUserInfo START TRANSACTION fail: %s", mysql_error(conn_));
        return DB_ERR_MYSQL;
    }

    int32_t cur_version = 0;
    bool exists = false;
    int ret = SelectVersionForUpdate("user_info", gid, cur_version, exists);
    if (ret != 0)
    {
        mysql_query(conn_, "ROLLBACK");
        return ret;
    }

    if (exists && data_version != 0 && cur_version != data_version)
    {
        // 同SetLogin：data_version==0视为调用方未做CAS追踪，等价tcaplus REPLACE_REQ的
        // 无条件覆盖写语义，仅当调用方显式传入非0版本号时才校验拦截。
        mysql_query(conn_, "ROLLBACK");
        APP_LOG_WARN(gid, "SetUserInfo version mismatch, cur=%d, req=%d", cur_version, data_version);
        return DB_ERR_INVALID_VERSION;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt)
    {
        mysql_query(conn_, "ROLLBACK");
        return DB_ERR_MYSQL;
    }

    uint32_t is_new = in.dwIs_new;
    uint32_t role_type = in.dwRole_type;
    uint64_t points = in.ullPoints;
    unsigned long user_name_len = strnlen(in.szUser_name, sizeof(in.szUser_name));
    bool ok = false;

    if (!exists)
    {
        const char* sql =
            "INSERT INTO user_info (gid, is_new, role_type, user_name, points, data_version) "
            "VALUES (?, ?, ?, ?, ?, 1)";
        if (mysql_stmt_prepare(stmt, sql, strlen(sql)) == 0)
        {
            MYSQL_BIND param[5] = {};
            param[0].buffer_type = MYSQL_TYPE_LONGLONG;
            param[0].buffer = &gid;
            param[0].is_unsigned = 1;
            param[1].buffer_type = MYSQL_TYPE_LONG;
            param[1].buffer = &is_new;
            param[1].is_unsigned = 1;
            param[2].buffer_type = MYSQL_TYPE_LONG;
            param[2].buffer = &role_type;
            param[2].is_unsigned = 1;
            param[3].buffer_type = MYSQL_TYPE_STRING;
            param[3].buffer = const_cast<char*>(in.szUser_name);
            param[3].buffer_length = static_cast<unsigned long>(user_name_len);
            param[3].length = &user_name_len;
            param[4].buffer_type = MYSQL_TYPE_LONGLONG;
            param[4].buffer = &points;
            param[4].is_unsigned = 1;
            ok = (mysql_stmt_bind_param(stmt, param) == 0) && (mysql_stmt_execute(stmt) == 0);
        }
    }
    else
    {
        const char* sql =
            "UPDATE user_info SET is_new=?, role_type=?, user_name=?, points=?, "
            "data_version=data_version+1 WHERE gid=?";
        if (mysql_stmt_prepare(stmt, sql, strlen(sql)) == 0)
        {
            MYSQL_BIND param[5] = {};
            param[0].buffer_type = MYSQL_TYPE_LONG;
            param[0].buffer = &is_new;
            param[0].is_unsigned = 1;
            param[1].buffer_type = MYSQL_TYPE_LONG;
            param[1].buffer = &role_type;
            param[1].is_unsigned = 1;
            param[2].buffer_type = MYSQL_TYPE_STRING;
            param[2].buffer = const_cast<char*>(in.szUser_name);
            param[2].buffer_length = static_cast<unsigned long>(user_name_len);
            param[2].length = &user_name_len;
            param[3].buffer_type = MYSQL_TYPE_LONGLONG;
            param[3].buffer = &points;
            param[3].is_unsigned = 1;
            param[4].buffer_type = MYSQL_TYPE_LONGLONG;
            param[4].buffer = &gid;
            param[4].is_unsigned = 1;
            ok = (mysql_stmt_bind_param(stmt, param) == 0) && (mysql_stmt_execute(stmt) == 0);
        }
    }

    if (!ok)
        APP_LOG_WARN(gid, "SetUserInfo stmt fail: %s", mysql_stmt_error(stmt));
    mysql_stmt_close(stmt);

    if (!ok)
    {
        mysql_query(conn_, "ROLLBACK");
        return DB_ERR_MYSQL;
    }

    if (mysql_query(conn_, "COMMIT") != 0)
    {
        APP_LOG_WARN(gid, "SetUserInfo COMMIT fail: %s", mysql_error(conn_));
        return DB_ERR_MYSQL;
    }
    return 0;
}

}  // namespace dbproxy
