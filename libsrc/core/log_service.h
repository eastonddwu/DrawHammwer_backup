/*
 * * file name: log_service.h
 * * description: tlog接入封装，管理LPTLOGCTX的生命周期和一个默认category，
 * *              供log.h里的APP_LOG_*宏调用底层tlog_xxx写日志。
 * *              日志文件默认写到log_dir下（相对路径，以进程启动时的CWD为基准），按天滚动。
 * */

#ifndef _APP_LOG_SERVICE_H_
#define _APP_LOG_SERVICE_H_

#include <string>
#include "patterns/singleton.h"
#include "tlog/tlog.h"

namespace app
{
class LogService : public Singleton<LogService>
{
public:
    /// log_dir为日志目录（相对路径以进程CWD为基准，目录不存在时tlog会自动创建）
    /// module_name用作category名和日志文件名前缀，一般取自进程名（argv[0]）
    /// 需要在第一条APP_LOG_*调用之前完成初始化，否则日志会被静默丢弃（Category()为nullptr时tlog内部判空跳过）
    ///
    /// 日志级别默认TLOG_PRIORITY_INFO，可用环境变量APP_LOG_LEVEL覆盖，取值（大小写不敏感）：
    /// fatal/alert/crit/error/warn/notice/info/debug/trace。
    /// 压测时建议设为warn或error：tlog是同步写盘，单线程reactor下每条INFO都是一次阻塞write，
    /// 而业务层每个RPC出入口都有INFO日志，会显著污染压测数据。
    bool Init(const std::string& log_dir, const std::string& module_name);
    /// 释放tlog上下文，调用之后Category()重新变为nullptr
    void Fini();

    LPTLOGCATEGORYINST Category() const { return category_; }

    /// 当前生效的日志级别（TLOG_PRIORITY_*，数值越小越严重）
    int Level() const { return level_; }

    /// 把级别名解析为TLOG_PRIORITY_*，无法识别时返回default_level
    static int ParseLevel(const std::string& name, int default_level);

private:
    friend Singleton<LogService>;
    LogService() = default;
    ~LogService() = default;

private:
    LPTLOGCTX ctx_ = nullptr;
    LPTLOGCATEGORYINST category_ = nullptr;
    int level_ = TLOG_PRIORITY_INFO;
};

}  // namespace app

#endif
