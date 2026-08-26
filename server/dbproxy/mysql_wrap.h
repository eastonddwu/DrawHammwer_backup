/*
 * * file name: mysql_wrap.h
 * * description: MySQL业务层封装，作为TcapWrap的并列后端。
 * *              仅在APP_DB_BACKEND=mysql时被db_app.cpp/db_service.cpp调用，
 * *              不影响、不依赖任何tcaplus相关代码。
 * *              采用同步阻塞的MariaDB C API（prepared statement），
 * *              因当前表数量少(2张)、请求量低，未引入协程异步封装，
 * *              以降低实现复杂度为优先。
 * *              REPLACE语义通过显式事务(SELECT...FOR UPDATE + 条件UPDATE/INSERT)
 * *              复刻tcaplus的CHECKDATAVERSION_AUTOINCREASE乐观锁行为。
 * */

#ifndef _DB_MYSQL_WRAP_H_
#define _DB_MYSQL_WRAP_H_

#include <cstdint>
#include <mysql.h>
#include "patterns/singleton.h"
#include "table/tb_app_tcaplus.h"
#include "utils/db_conf.h"

namespace dbproxy
{

class MysqlWrap : public app::Singleton<MysqlWrap>
{
public:
    int Init(const MysqlConf& conf);
    void Finish();

    int GetLogin(uint64_t gid, LOGIN& out, int32_t& data_version);
    int SetLogin(uint64_t gid, const LOGIN& in, int32_t data_version);

    int GetUserInfo(uint64_t gid, USER_INFO& out, int32_t& data_version);
    int SetUserInfo(uint64_t gid, const USER_INFO& in, int32_t data_version);

private:
    friend class app::Singleton<MysqlWrap>;
    MysqlWrap() = default;

    // 事务内加锁查询当前data_version；exists=false表示记录不存在。
    // 调用方必须已执行START TRANSACTION，且调用后自行COMMIT/ROLLBACK。
    int SelectVersionForUpdate(const char* table_name, uint64_t gid, int32_t& version, bool& exists);

    MYSQL* conn_ = nullptr;
};

}  // namespace dbproxy

#endif
