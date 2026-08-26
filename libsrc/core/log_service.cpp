/*
 * * file name: log_service.cpp
 * * description: LogService实现，见log_service.h说明
 * */

#include "log_service.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <tloghelp/tlogload.h>
#include "common/runtime_config.h"

namespace app
{
namespace
{
struct LevelName
{
    const char* name;
    int priority;
};

// tlog优先级数值越小越严重，piPriorityLow充当"最低严重程度"上限
constexpr LevelName kLevelNames[] = {
    {"fatal", TLOG_PRIORITY_FATAL}, {"alert", TLOG_PRIORITY_ALERT},   {"crit", TLOG_PRIORITY_CRIT},
    {"error", TLOG_PRIORITY_ERROR}, {"warn", TLOG_PRIORITY_WARN},     {"notice", TLOG_PRIORITY_NOTICE},
    {"info", TLOG_PRIORITY_INFO},   {"debug", TLOG_PRIORITY_DEBUG},   {"trace", TLOG_PRIORITY_TRACE},
};
}  // namespace

int LogService::ParseLevel(const std::string& name, int default_level)
{
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(::tolower(c)); });

    for (const auto& item : kLevelNames)
    {
        if (lower == item.name)
            return item.priority;
    }
    return default_level;
}

bool LogService::Init(const std::string& log_dir, const std::string& module_name)
{
    if (ctx_)
        return true;

    // 级别优先取环境变量APP_LOG_LEVEL，其次取运行期配置文件，都没有则用INFO。
    // 压测时设为warn可避免同步写盘干扰（tlog宏在级别不满足时连实参都不求值）。
    level_ = TLOG_PRIORITY_INFO;
    std::string level_name = runtime_config::Get("APP_LOG_LEVEL");
    if (!level_name.empty())
        level_ = ParseLevel(level_name, TLOG_PRIORITY_INFO);

    // 日志文件名带%Y%m%d，按天滚动；iMaxRotate=10保留最近10个文件；iSizeLimit=200MB单文件上限；
    // iRotateStick=1表示一直写当前文件（而不是每次都重新打开判断），配置为NULL则用tlog默认layout格式
    std::string file_pattern = log_dir + "/" + module_name + ".%Y%m%d.log";
    ctx_ = tlog_init_file_ctx_ex(module_name.c_str(), level_, file_pattern.c_str(), 10,
                                  200 * 1024 * 1024, 1, nullptr);
    if (!ctx_)
    {
        fprintf(stderr, "tlog_init_file_ctx_ex failed, log_dir(%s), module_name(%s)\n", log_dir.c_str(),
                module_name.c_str());
        return false;
    }

    category_ = tlog_get_category(ctx_, module_name.c_str());
    if (!category_)
    {
        fprintf(stderr, "tlog_get_category failed, module_name(%s)\n", module_name.c_str());
        tlog_fini_ctx(&ctx_);
        return false;
    }

    return true;
}

void LogService::Fini()
{
    if (ctx_)
        tlog_fini_ctx(&ctx_);
    category_ = nullptr;
}

}  // namespace app
