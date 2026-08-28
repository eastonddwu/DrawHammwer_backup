/*
 * * file name: db_app.cpp
 * * description: DBApp实现，对齐ua_server的DBProxyApp但使用app_server的BaseServer/协程模式
 * */

#include "db_app.h"
#include "common/runtime_config.h"
#include "core/log.h"
#include "core/rpc_service.h"
#include "db_rpc_meta.h"
#include "db_service.h"
#include "dbproxy.pb.h"
#include "mysql_wrap.h"
#include "svr_base/default_init.h"
#include "tcaplus_wrap.h"
#include "utils/db_conf.h"
#include "utils/mysql_config.h"
#include "utils/tcaplus_config.h"

namespace dbproxy
{

void DBApp::Setup(const std::string& tbus2_agent_url, const std::string& conf_file)
{
    tbus2_agent_url_ = tbus2_agent_url;
    conf_file_ = conf_file;
}

bool DBApp::OnInit()
{
    // 1. 注册默认transport (tbus2, 自动驱动)
    // MySvrID() 在 --conf-file 模式下已是完整 busid（如 0x05010001）；
    // 命令行模式下 main.cpp 已在调用 Init() 前完成 kDBProxyGroupBase | svr_id 的 OR。
    if (!UseDefaultPlugin(*this, MySvrID(), tbus2_agent_url_))
    {
        APP_LOG_ERROR(0, "UseDefaultPlugin fail, svr_id(%u), agent_url(%s)",
                      MySvrID(), tbus2_agent_url_.c_str());
        return false;
    }

    // 2. 加载配置（优先命令行指定，否则使用默认conf/tcaplus.conf）
    if (!InitConf())
    {
        APP_LOG_ERROR(0, "init conf failed");
        return false;
    }

    // 3. 初始化tcaplus（无条件执行，保留原有代码路径；
    //    backend=mysql时tcaplus不可用不再是致命错误，仅WARN）
    int ret = InitTcaplus();
    if (ret != 0)
    {
        if (db_backend_ == DbBackend::kTcaplus)
        {
            APP_LOG_ERROR(0, "init tcaplus failed, ret=%d", ret);
            return false;
        }
        APP_LOG_WARN(0, "init tcaplus failed (non-fatal, backend=mysql), ret=%d", ret);
    }

    // 3.1 新增：backend=mysql时初始化mysql连接（失败则致命，因为此时是唯一后端）
    if (db_backend_ == DbBackend::kMysql)
    {
        int mysql_ret = InitMysql();
        if (mysql_ret != 0)
        {
            APP_LOG_ERROR(0, "init mysql failed, ret=%d", mysql_ret);
            return false;
        }
    }

    // 4. 注册RPC方法
    if (!app::RpcService::GetInst().RegisterMethod(
            GetDBMethodCmd("CommonGetData"),
            {DBService::CommonGetData,
             &app::protocol::CommonGetDataReq::default_instance(),
             &app::protocol::CommonGetDataResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register CommonGetData fail");
        return false;
    }

    if (!app::RpcService::GetInst().RegisterMethod(
            GetDBMethodCmd("CommonSetData"),
            {DBService::CommonSetData,
             &app::protocol::CommonSetDataReq::default_instance(),
             &app::protocol::CommonSetDataResp::default_instance()}))
    {
        APP_LOG_ERROR(0, "register CommonSetData fail");
        return false;
    }

    APP_LOG_INFO(0, "DBApp init ok, svr_id(%u), busid(%u)", MySvrID(), MySvrID());
    return true;
}

size_t DBApp::OnProc(uint64_t now_ms, bool stop)
{
    // tcaplus回包轮询：tbus2是default transport由框架自动驱动，
    // tcaplus的OnUpdate/RecvResponse需要在这里手动驱动
    size_t count = stop ? 0 : TcapWrap::GetInst().Proc();

    // mysql异步回包轮询。stop时也要继续驱动：停机流程靠SvrStopReady()等
    // PendingContextNum()归零，不驱动的话在飞的等待上下文只能靠超时清空。
    // 返回值非0会让tapp主循环跳过iIdleSleep保持热转，这是异步化不引入额外
    // 唤醒延迟的关键（见mysql_wrap.h对Proc()的说明）。
    if (db_backend_ == DbBackend::kMysql)
        count += MysqlWrap::GetInst().Proc();

    return count;
}

bool DBApp::OnFinish()
{
    TcapWrap::GetInst().Finish();
    if (db_backend_ == DbBackend::kMysql)
        MysqlWrap::GetInst().Finish();
    return true;
}

bool DBApp::InitConf()
{
    // 如果命令行未指定conf_file，使用默认路径 conf/tcaplus.conf
    std::string conf_path = conf_file_.empty() ? TCAPLUS_DEFAULT_CONF_PATH : conf_file_;

    DbConf db_conf;
    if (!db_conf.ParseFromFile(conf_path))
        return false;

    tcaplus_conf_ = db_conf.tcaplus_conf;

    // 新增：读取存储后端开关，与APP_LOG_LEVEL/APP_METRICS同一套runtime_config机制
    // （先查环境变量，再查app_runtime.conf，默认tcaplus，保持原有行为不变）
    std::string backend = app::runtime_config::Get("APP_DB_BACKEND", "tcaplus");
    db_backend_ = (backend == "mysql") ? DbBackend::kMysql : DbBackend::kTcaplus;

    if (db_backend_ == DbBackend::kMysql)
    {
        // mysql.conf 与 tcaplus.conf 推送到同一conf目录，取其同级路径
        std::string mysql_conf_path = MYSQL_DEFAULT_CONF_PATH;
        auto pos = conf_path.find_last_of('/');
        if (pos != std::string::npos)
            mysql_conf_path = conf_path.substr(0, pos + 1) + "mysql.conf";

        if (!db_conf.ParseMysqlFromFile(mysql_conf_path))
        {
            APP_LOG_ERROR(0, "init conf failed: mysql.conf load fail, path=%s", mysql_conf_path.c_str());
            return false;
        }
        mysql_conf_ = db_conf.mysql_conf;
    }

    return true;
}

int DBApp::InitTcaplus()
{
    int ret = TcapWrap::GetInst().Init(tcaplus_conf_, &context_ctrl_,
                                       app::LogService::GetInst().Category());
    if (ret != 0)
    {
        APP_LOG_ERROR(0, "tcaplus init fail, ret=%d", ret);
        return ret;
    }

    std::string tables;
    for (size_t i = 0; i < tcaplus_conf_.table_names.size(); i++)
    {
        if (i > 0) tables += ",";
        tables += tcaplus_conf_.table_names[i];
    }
    APP_LOG_INFO(0, "init tcaplus succ, app_id=%ld, zone_id=%d, tables=[%s]",
                 tcaplus_conf_.app_id, tcaplus_conf_.zone_id, tables.c_str());
    return 0;
}

int DBApp::InitMysql()
{
    return MysqlWrap::GetInst().Init(mysql_conf_, &context_ctrl_);
}

}  // namespace dbproxy
