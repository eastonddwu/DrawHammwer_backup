/*
 * * file name: db_app.h
 * * description: dbproxy业务server，继承BaseServer获得tapp驱动+ServerCore服务循环，
 * *              负责初始化tbus2通道(对内) + tcaplus连接 + 注册RPC方法。
 * *              tcaplus的回包轮询在OnProc()中手动驱动。
 * */

#ifndef _DB_APP_H_
#define _DB_APP_H_

#include <cstdint>
#include <string>
#include "patterns/singleton.h"
#include "svr_base/base_server.h"
#include "utils/db_conf.h"

namespace dbproxy
{

class DBApp : public app::BaseServer, public app::Singleton<DBApp>
{
public:
    // 存储后端开关，见APP_DB_BACKEND运行期配置。kTcaplus为原有行为，
    // kMysql为新增后端；两者代码路径均保留，互不删改。
    enum class DbBackend { kTcaplus, kMysql };

    void Setup(const std::string& tbus2_agent_url, const std::string& conf_file);
    const TcaplusConf& GetTcaplusConf() const { return tcaplus_conf_; }
    DbBackend GetDbBackend() const { return db_backend_; }

    /// GroupBase（用于命令行模式下 main.cpp 组合完整busid）
    static constexpr uint32_t kDBProxyGroupBase = 0x05000000;

protected:
    virtual bool OnInit() override;
    virtual size_t OnProc(uint64_t now_ms, bool stop) override;
    virtual bool OnFinish() override;

private:
    friend class app::Singleton<DBApp>;
    DBApp() = default;

    bool InitConf();
    int InitTcaplus();
    int InitMysql();

    // dbproxy group base: group 5 (0x05000000)
    // gid=0 reserved by namesrv, must use non-zero group
    // busid = kDBProxyGroupBase | svr_id（命令行模式），或由--conf-file直接指定。

    std::string tbus2_agent_url_;
    std::string conf_file_;
    TcaplusConf tcaplus_conf_;

    // 新增：mysql后端相关，与上面tcaplus_conf_并列，不影响原有字段
    DbBackend db_backend_ = DbBackend::kTcaplus;
    MysqlConf mysql_conf_;
};

}  // namespace dbproxy

#endif
